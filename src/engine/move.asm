; Move: defrags one file, the engine side of S7. The steps, the records
; and what each failure sends are docs/specs/DFR-0002-engine-protocol.md
; § 4.12; this file follows them in order. Calls extents.asm, crc32c.asm,
; records.asm and sys.asm (docs/design.md).
;
; Data moves only through EXT4_IOC_MOVE_EXT, which swaps the file's
; blocks with a donor's and copies the data (the kernel's
; fs/ext4/move_extent.c). The donor is a file with no name in the
; file's own folder, so nothing is left behind. Its blocks come from one
; fallocate, which asks ext4 for the file's whole size at once.
;
; Where each move put each block comes from reading the file's map
; before and after it, not from the kernel's moved_len: that counts blocks
; swapped, not where a failed move stopped. A block whose place changed
; is one that moved.

bits 64
default rel

%include "kernel.inc"
%include "records.inc"
%include "extents.inc"

extern sys_statx
extern sys_fdatasync
extern sys_pread64
extern sys_openat2
extern sys_fallocate
extern sys_ioctl
extern sys_close
extern extents_count
extern extents_map
extern crc32c_update
extern rec_start
extern rec_string
extern rec_send
extern rec_skipped

; Blocks one move asks for: 4 MiB in 4 KiB blocks. A pause or cancel
; waits for at most one (DFR-0002 § 4.8), and BLOCKS_MOVED holds at most
; two runs per block of one, so 2 × MOVE_CHUNK_BLOCKS runs must fit a
; record.
MOVE_CHUNK_BLOCKS equ 1024
%if 2 * MOVE_CHUNK_BLOCKS > (PAYLOAD_MAX - blocks_moved_size) / map_run_size
%error "one move's BLOCKS_MOVED would not fit a record"
%endif
; Each checksum read. O_DIRECT needs the buffer, the offset and the length
; aligned to the drive's logical block, at most 4096 bytes.
READ_SIZE       equ 1024 * 1024
READ_ALIGN      equ 4096
STATX_WANTED    equ STATX_TYPE | STATX_SIZE | STATX_MTIME | STATX_MNT_ID

section .rodata
empty_name:     db 0
this_folder:    db ".", 0
alignb 8
donor_how:                              ; the donor: no name, read and write
    istruc open_how
        at open_how.flags,   dq O_TMPFILE | O_RDWR | O_CLOEXEC
        at open_how.mode,    dq 0o600
        at open_how.resolve, dq 0
    iend

section .bss
alignb READ_ALIGN
read_buf:       resb READ_SIZE
alignb 8
statx_buf:      resb kernel_statx_size
move_args:      resb move_extent_size
before_runs:    resb file_run_size * MOVE_CHUNK_BLOCKS  ; one move's range, before
after_runs:     resb file_run_size * MOVE_CHUNK_BLOCKS  ; and after
from_runs:      resb map_run_size * MOVE_CHUNK_BLOCKS   ; BLOCKS_MOVED's lists
to_runs:        resb map_run_size * MOVE_CHUNK_BLOCKS
from_end:       resq 1                  ; one past the last from run
to_end:         resq 1
; Set once by move_setup.
block_size:     resq 1
mount_id:       resq 1
; The file this turn: move_file's arguments and what step 1 saw.
file_fd:        resq 1
folder_fd:      resq 1
path:           resq 1
path_len:       resq 1
size:           resq 1
mtime_sec:      resq 1
mtime_nsec:     resq 1
pieces:         resq 1
crc_before:     resq 1
donor_fd:       resq 1
moved:          resq 1                  ; blocks whose place changed
failure:        resq 1                  ; a move's -errno, or 0

section .text
global move_setup
global move_file

; move_setup(edi = the drive's block size, not 0, rsi = its mount id)
;   -> rax = 0. Call once, before move_file.
move_setup:
    mov edi, edi
    mov [block_size], rdi
    mov [mount_id], rsi
    xor eax, eax
    ret

; move_file(edi = a regular file, open as WALK_TO_MOVE opens one,
;   esi = its folder, rdx = its path, rcx = the path's length)
;   -> eax = OK to go on, else the result the run must finish with:
;   CHECKSUM_MISMATCH, or INTERNAL when the path does not fit a record.
; Takes the file through § 4.12's steps and sends its records. Closes
; neither descriptor. Clobbers rcx, rdx, rsi, rdi, r8 to r11.
move_file:
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rbp
    sub rsp, 8
    mov [file_fd], rdi
    mov [folder_fd], rsi
    mov [path], rdx
    mov [path_len], rcx
    mov qword [moved], 0
    mov qword [failure], 0

    ; 1. Checked: a regular file on this mount, not immutable or
    ; append-only. What it is now is what step 6 compares against.
    call look
    test rax, rax
    jnz .skip                       ; eax = errno, edx = stage
    mov rax, [statx_buf + kernel_statx.size]
    mov [size], rax
    mov rax, [statx_buf + kernel_statx.mtime + statx_timestamp.sec]
    mov [mtime_sec], rax
    mov eax, [statx_buf + kernel_statx.mtime + statx_timestamp.nsec]
    mov [mtime_nsec], rax

    ; 2. Synced: the kernel moves no block not yet given a place. A file
    ; in one piece is left before the sync, which would cost every whole
    ; file a write; the count is taken again after it.
    call count_pieces
    jae .skip_errno
    cmp rax, 2
    jb .ok                          ; in one piece: no record
    mov rdi, [file_fd]
    call sys_fdatasync
    mov edx, STAGE_MOVE
    cmp rax, -MAX_ERRNO
    jae .skip_errno
    call count_pieces
    jae .skip_errno
    cmp rax, 2
    jb .ok
    mov [pieces], rax

    ; 3. Its checksum, read from the drive.
    call checksum
    mov edx, STAGE_READ
    cmp rax, -MAX_ERRNO
    jae .skip_errno
    mov [crc_before], rax

    ; 4. A donor with the file's size, kept only in fewer pieces.
    mov rdi, [folder_fd]
    lea rsi, [this_folder]
    lea rdx, [donor_how]
    call sys_openat2
    mov edx, STAGE_DONOR
    cmp rax, -MAX_ERRNO
    jae .skip_errno
    mov [donor_fd], rax
    mov rdi, rax
    xor esi, esi                    ; mode 0: the size grows to cover it
    xor edx, edx
    mov rcx, [size]
    call sys_fallocate
    cmp rax, -MAX_ERRNO
    jae .donor_failed
    mov rdi, [donor_fd]
    mov rsi, [block_size]
    xor edx, edx
    call extents_count
    cmp rax, -MAX_ERRNO
    jae .donor_failed
    cmp rax, [pieces]
    jb .move
    call close_donor                ; no better: left as it was
    mov rdi, [pieces]
    mov rsi, rdi
    xor edx, edx
    call send_defragged
    jmp .sent

    ; 5. The moves, MOVE_CHUNK_BLOCKS at a time, until the first failure.
.move:
    xor r12d, r12d                  ; r12 = the next block to move
    mov rax, [size]
    add rax, [block_size]
    dec rax
    xor edx, edx
    div qword [block_size]
    mov r13, rax                    ; r13 = the file's blocks, to its end
.chunk:
    cmp r12, r13
    jae .moved
    mov r14, r13
    sub r14, r12
    mov eax, MOVE_CHUNK_BLOCKS
    cmp r14, rax
    cmova r14, rax                  ; r14 = this move's blocks
    mov rdi, [file_fd]
    mov rsi, r12
    mov rdx, r14
    mov rcx, [block_size]
    lea r8, [before_runs]
    mov r9d, MOVE_CHUNK_BLOCKS
    call extents_map
    cmp rax, -MAX_ERRNO
    jae .chunk_failed
    mov r15, rax                    ; r15 = runs before
    lea rdx, [move_args]
    mov dword [rdx + move_extent.reserved], 0
    mov rax, [donor_fd]
    mov [rdx + move_extent.donor_fd], eax
    mov [rdx + move_extent.orig_start], r12
    mov [rdx + move_extent.donor_start], r12
    mov [rdx + move_extent.len], r14
    mov qword [rdx + move_extent.moved_len], 0
    mov rdi, [file_fd]
    mov esi, EXT4_IOC_MOVE_EXT
    call sys_ioctl
    mov rbx, rax                    ; rbx = the move's result
    mov rdi, [file_fd]
    mov rsi, r12
    mov rdx, r14
    mov rcx, [block_size]
    lea r8, [after_runs]
    mov r9d, MOVE_CHUNK_BLOCKS
    call extents_map
    cmp rax, -MAX_ERRNO
    jae .chunk_failed               ; where blocks went is unknown
    mov rdi, r15
    mov rsi, rax
    call send_moved
    test rax, rax
    js .internal
    add [moved], rax
    ; Any error ends the file's moves, EINTR too: moving a range again
    ; would swap its moved blocks back with the donor's.
    cmp rbx, -MAX_ERRNO
    jae .chunk_failed_rbx
    add r12, r14
    jmp .chunk
.chunk_failed_rbx:
    mov rax, rbx
.chunk_failed:
    mov [failure], rax
.moved:
    call close_donor                ; frees the blocks the file left

    ; 6. Read back. Nothing moved is nothing to check.
    mov rax, [failure]
    cmp qword [moved], 0
    jne .check
    test rax, rax
    jz .check
    mov edx, STAGE_MOVE
    jmp .skip_errno
.check:
    mov rdi, [file_fd]
    call sys_fdatasync
    mov edx, STAGE_MOVE
    cmp rax, -MAX_ERRNO
    jae .skip_errno
    call checksum
    mov edx, STAGE_READ
    cmp rax, -MAX_ERRNO
    jae .skip_errno
    mov rbx, rax                    ; rbx = the checksum after
    call look
    test rax, rax
    jnz .skip
    mov eax, 0                      ; no call failed
    mov edx, STAGE_CHANGED
    mov rcx, [statx_buf + kernel_statx.size]
    cmp rcx, [size]
    jne .skip
    mov rcx, [statx_buf + kernel_statx.mtime + statx_timestamp.sec]
    cmp rcx, [mtime_sec]
    jne .skip
    mov ecx, [statx_buf + kernel_statx.mtime + statx_timestamp.nsec]
    cmp rcx, [mtime_nsec]
    jne .skip
    cmp rbx, [crc_before]
    jne .mismatch
    mov rax, [failure]
    mov edx, STAGE_MOVE
    test rax, rax
    jnz .skip_errno
    call count_pieces
    jae .skip_errno
    mov rdi, [pieces]
    mov rsi, rax
    mov rdx, [moved]
    call send_defragged
.sent:
    test rax, rax
    jnz .internal
.ok:
    mov eax, RESULT_OK
    jmp .return

.mismatch:
    xor edi, edi
    mov esi, STAGE_CHECKSUM
    call skipped
    test rax, rax
    jnz .internal
    mov eax, RESULT_CHECKSUM_MISMATCH
    jmp .return
.donor_failed:
    mov rbx, rax
    call close_donor
    mov rax, rbx
    mov edx, STAGE_DONOR
.skip_errno:                        ; rax = -errno, edx = stage
    neg eax
.skip:                              ; eax = errno, edx = stage
    mov edi, eax
    mov esi, edx
    call skipped
    test rax, rax
    jz .ok
.internal:
    mov eax, RESULT_INTERNAL
.return:
    add rsp, 8
    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; count_pieces() -> rax = the file's pieces, or -errno with edx =
;   STAGE_EXTENTS; the flags are cmp rax, -MAX_ERRNO's, so jae takes the
;   error. Clobbers rcx, rdx, rsi, rdi, r8 to r11.
count_pieces:
    sub rsp, 8
    mov rdi, [file_fd]
    mov rsi, [block_size]
    xor edx, edx                    ; count only
    call extents_count
    add rsp, 8
    mov edx, STAGE_EXTENTS
    cmp rax, -MAX_ERRNO             ; last: ret leaves the flags alone
    ret

; look() -> rax = 0, or eax = errno and edx = the stage it failed at.
; Fills statx_buf for the file: step 1's checks (§ 4.12). Clobbers rcx,
; rsi, rdi, r8 to r11.
look:
    sub rsp, 8
    mov rdi, [file_fd]
    lea rsi, [empty_name]
    mov edx, AT_EMPTY_PATH
    mov ecx, STATX_WANTED
    lea r8, [statx_buf]
    call sys_statx
    cmp rax, -MAX_ERRNO
    jae .stat_failed
    mov ecx, [statx_buf + kernel_statx.mask]
    and ecx, STATX_WANTED
    mov rax, -EOPNOTSUPP            ; a kernel that cannot say all four
    cmp ecx, STATX_WANTED
    jne .stat_failed
    mov edx, STAGE_REFUSED
    mov eax, EINVAL
    movzx ecx, word [statx_buf + kernel_statx.mode]
    and ecx, S_IFMT
    cmp ecx, S_IFREG
    jne .return
    mov eax, EXDEV
    mov rcx, [statx_buf + kernel_statx.mnt_id]
    cmp rcx, [mount_id]
    jne .return
    mov eax, EPERM
    mov rcx, [statx_buf + kernel_statx.attributes]
    and rcx, [statx_buf + kernel_statx.attributes_mask]
    test ecx, STATX_ATTR_IMMUTABLE | STATX_ATTR_APPEND
    jnz .return
    xor eax, eax
    jmp .return
.stat_failed:
    neg eax
    mov edx, STAGE_STAT
.return:
    add rsp, 8
    ret

; checksum() -> rax = the file's CRC32C, or -errno. Reads it from start
; to end, past the cache. Clobbers rcx, rdx, rsi, rdi, r8 to r11.
checksum:
    push rbx
    push r12
    sub rsp, 8
    xor ebx, ebx                    ; ebx = the CRC so far
    xor r12d, r12d                  ; r12 = the next offset
.read:
    mov rdi, [file_fd]
    lea rsi, [read_buf]
    mov edx, READ_SIZE
    mov rcx, r12
    call sys_pread64
    cmp rax, -EINTR
    je .read
    cmp rax, -MAX_ERRNO
    jae .return
    test rax, rax
    jz .done
    add r12, rax
    mov edi, ebx
    lea rsi, [read_buf]
    mov rdx, rax
    call crc32c_update
    mov ebx, eax
    jmp .read
.done:
    mov eax, ebx
.return:
    add rsp, 8
    pop r12
    pop rbx
    ret

; send_moved(rdi = runs in before_runs, rsi = runs in after_runs)
;   -> rax = the blocks whose place changed, sent as one BLOCKS_MOVED when
;   not 0; -1 when the record would not fit, which MOVE_CHUNK_BLOCKS
;   rules out. Both lists cover the same range of the file, in its order.
;   Clobbers rcx, rdx, rsi, rdi, r8 to r11.
; A block is in both lists unless it is a hole in one of them. Where the
; two places of a block differ, it moved: its old place goes to the from
; runs, its new one to the to runs, each joined onto the run before when
; it follows on.
send_moved:
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rbp
    sub rsp, 8
    lea rbx, [before_runs]
    imul rdi, rdi, file_run_size
    lea r12, [rbx + rdi]            ; r12 = past the last before run
    lea r13, [after_runs]
    imul rsi, rsi, file_run_size
    lea r14, [r13 + rsi]            ; r14 = past the last after run
    lea rax, [from_runs]
    mov [from_end], rax
    lea rax, [to_runs]
    mov [to_end], rax
    xor ebp, ebp                    ; rbp = blocks moved
.pair:                              ; rbx = a before run, r13 = an after run
    cmp rbx, r12
    jae .send
    cmp r13, r14
    jae .send
    mov r8, [rbx + file_run.logical]
    mov r9, [r13 + file_run.logical]
    mov r10, r8
    cmp r10, r9
    cmovb r10, r9                   ; r10 = where both start
    mov r11, r8
    add r11, [rbx + file_run.length]    ; r11 = the before run's end
    mov r15, r9
    add r15, [r13 + file_run.length]    ; r15 = the after run's end
    mov rcx, r11
    cmp rcx, r15
    cmova rcx, r15                  ; rcx = where the first ends
    cmp r10, rcx
    jae .advance                    ; they do not overlap
    mov rsi, r10
    sub rsi, r8
    add rsi, [rbx + file_run.physical]  ; rsi = the overlap's place before
    mov rdx, r10
    sub rdx, r9
    add rdx, [r13 + file_run.physical]  ; rdx = and after
    cmp rsi, rdx
    je .advance                     ; did not move
    sub rcx, r10                    ; rcx = the overlap's blocks
    add rbp, rcx
    lea rdi, [from_end]
    lea r8, [from_runs]
    call add_run
    mov rsi, rdx
    lea rdi, [to_end]
    lea r8, [to_runs]
    call add_run
.advance:                           ; past whichever run ends first
    cmp r11, r15
    ja .next_after
    add rbx, file_run_size
    jmp .pair
.next_after:
    add r13, file_run_size
    jmp .pair

.send:
    mov rax, rbp
    test rbp, rbp
    jz .return
    lea rax, [from_runs]
    mov r12, [from_end]
    sub r12, rax                    ; r12 = the from runs' bytes
    lea rax, [to_runs]
    mov r13, [to_end]
    sub r13, rax                    ; r13 = the to runs' bytes
    lea rsi, [r12 + r13 + blocks_moved_size]
    mov rax, -1
    cmp rsi, PAYLOAD_MAX
    ja .return
    mov edi, REC_BLOCKS_MOVED
    call rec_start                  ; keeps rsi
    mov rcx, r12
    shr rcx, 4                      ; map_run_size is 16
    mov [rax + blocks_moved.from_count], ecx
    mov rcx, r13
    shr rcx, 4
    mov [rax + blocks_moved.to_count], ecx
    lea rdi, [rax + blocks_moved_size]
    lea rsi, [from_runs]
    mov rcx, r12
    rep movsb
    lea rsi, [to_runs]
    mov rcx, r13
    rep movsb
    call rec_send
    mov rax, rbp
.return:
    add rsp, 8
    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; add_run(rdi = from_end or to_end, r8 = that list's first run,
;   rsi = the run's first block, rcx = its blocks) Appends the run, joined
;   onto the list's last run when it follows on, and moves the end.
;   Clobbers rax and r9 only.
add_run:
    mov rax, [rdi]                  ; rax = past the last run
    cmp rax, r8
    je .append                      ; the list is empty
    mov r9, [rax - map_run_size + map_run.start]
    add r9, [rax - map_run_size + map_run.length]
    cmp r9, rsi
    jne .append
    add [rax - map_run_size + map_run.length], rcx
    ret
.append:
    mov [rax + map_run.start], rsi
    mov [rax + map_run.length], rcx
    add rax, map_run_size
    mov [rdi], rax
    ret

; send_defragged(rdi = pieces before, rsi = pieces after, rdx = blocks
;   moved) -> rax = 0, or -1 when the path does not fit; then nothing is
;   sent. Sends FILE_DEFRAGGED for the file. Clobbers as rec_send.
send_defragged:
    push rbx
    push r12
    push r13
    mov rbx, rdi
    mov r12, rsi
    mov r13, rdx
    mov edi, REC_FILE_DEFRAGGED
    mov esi, file_defragged_size
    call rec_start
    mov ecx, 0xFFFFFFFF             ; the fields are 32 bits
    cmp rbx, rcx
    cmova rbx, rcx
    cmp r12, rcx
    cmova r12, rcx
    mov [rax + file_defragged.extents_before], ebx
    mov [rax + file_defragged.extents_after], r12d
    mov [rax + file_defragged.moved_blocks], r13
    mov rdi, [path]
    mov rsi, [path_len]
    call rec_string
    test rax, rax
    jnz .return                     ; -1
    call rec_send
.return:
    pop r13
    pop r12
    pop rbx
    ret

; skipped(edi = errno, esi = stage) -> rax = 0, or -1 as rec_skipped.
; Sends FILE_SKIPPED for the file. Clobbers as rec_send.
skipped:
    sub rsp, 8
    mov rdx, [path]
    mov rcx, [path_len]
    call rec_skipped
    add rsp, 8
    ret

; close_donor() -> rax = 0. Closes the donor, which frees its blocks; a
; failed close loses nothing, as it has no name. Clobbers as sys_close.
close_donor:
    sub rsp, 8
    mov rdi, [donor_fd]
    call sys_close
    xor eax, eax
    add rsp, 8
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
