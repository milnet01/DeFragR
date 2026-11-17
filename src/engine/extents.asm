; Extents: how many pieces a file is in, read with FS_IOC_FIEMAP, and
; where a file and the drive's free and filesystem blocks lie, read with
; FS_IOC_GETFSMAP. The engine side of S3's figures and S4's map. Calls
; only sys.asm and records.asm (docs/design.md).
;
; A piece is a physically separate run, counted as filefrag counts them
; (DFR-0002 § 4.6; e2fsprogs misc/filefrag.c, filefrag_fiemap), so its
; count is the named reference in the image tests. Against the extent
; before it, an extent starts a new piece unless it begins at logical 0,
; or its physical start is where the one before ends, or where the one
; before would have put this logical offset across a hole. The first
; extent is always one piece; a file with no extents is in none.
;
; FIEMAP is asked with no flags, as filefrag asks by default: nothing is
; flushed, so the drive is not written to. GETFSMAP writes nothing either.
;
; The map's runs (DFR-0002 § 4.6) go to rec_map_run in blocks: a run
; starts at the block holding its first byte and ends at the block holding
; its last. A file's kind is known only once its pieces are counted, so
; the caller reads its extents twice: once to count, once to send.

bits 64
default rel

%include "kernel.inc"
%include "records.inc"

extern sys_ioctl
extern sys_statx
extern rec_map_run

EXTENTS_PER_CALL equ 256
; Small enough that the image test's free space needs a second call
; (DFR-0002 INV-17), so the carrying on is tested.
FSMAP_PER_CALL   equ 64

section .rodata
empty_name: db 0

section .bss
alignb 8
fm_buf:     resb fiemap_size + fiemap_extent_size * EXTENTS_PER_CALL
prev:       resb fiemap_extent_size     ; the extent before this one
map_kind:   resd 1                      ; the kind send_run sends, or 0 for none
alignb 8
fsmap_buf:  resb fsmap_head_size + fsmap_size * FSMAP_PER_CALL
drive_statx: resb kernel_statx_size

section .text
global extents_count
global extents_space

; extents_count(edi = an open regular file, esi = the block size, not 0,
;   edx = a MAP_ kind to send the file's runs as, or 0 to send none)
;   -> rax = the file's pieces, or -errno when FIEMAP fails; rdx = the
;   blocks its extents cover, each extent's bytes rounded up to a block.
; An inline-data file counts its one block, but sends no run for it, nor
; for an extent with no place yet. Clobbers rcx, rsi, rdi, r8 to r11.
extents_count:
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rbp
    sub rsp, 8
    mov ebx, edi                    ; ebx = the file
    mov ebp, esi                    ; rbp = the block size
    mov [map_kind], edx
    xor r12d, r12d                  ; r12 = pieces
    xor r13d, r13d                  ; r13 = blocks
    xor r14d, r14d                  ; r14 = the previous extent, or 0
    xor r15d, r15d                  ; r15 = the next logical offset asked for
.call:
    lea rdx, [fm_buf]
    mov [rdx + fiemap.start], r15
    mov qword [rdx + fiemap.length], -1
    mov dword [rdx + fiemap.flags], 0
    mov dword [rdx + fiemap.mapped_extents], 0
    mov dword [rdx + fiemap.extent_count], EXTENTS_PER_CALL
    mov dword [rdx + fiemap.reserved], 0
    mov edi, ebx
    mov esi, FS_IOC_FIEMAP
    call sys_ioctl
    cmp rax, -EINTR
    je .call
    cmp rax, -MAX_ERRNO
    jae .return                     ; rax = -errno
    lea rsi, [fm_buf]
    mov ecx, [rsi + fiemap.mapped_extents]
    test ecx, ecx
    jz .done
    cmp ecx, EXTENTS_PER_CALL
    ja .broken                      ; more than the buffer holds
    lea rsi, [rsi + fiemap_size]    ; rsi = this extent
    xor r8d, r8d                    ; r8 = 1 once the last extent is seen
.extent:
    test r14, r14
    jz .new_piece                   ; the first extent: one piece
    mov r9, [rsi + fiemap_extent.logical]
    test r9, r9
    jz .counted                     ; filefrag counts no piece at logical 0
    mov rax, [r14 + fiemap_extent.physical]
    mov r10, rax
    add r10, [r14 + fiemap_extent.length]
    cmp [rsi + fiemap_extent.physical], r10
    je .counted                     ; follows the previous end directly
    add rax, r9
    sub rax, [r14 + fiemap_extent.logical]
    cmp [rsi + fiemap_extent.physical], rax
    je .counted                     ; follows it across a hole
.new_piece:
    inc r12
.counted:
    mov rax, [rsi + fiemap_extent.length]
    add rax, rbp
    dec rax
    xor edx, edx
    div rbp
    add r13, rax
    cmp dword [map_kind], 0
    je .mapped
    test dword [rsi + fiemap_extent.flags], FIEMAP_EXTENT_UNKNOWN | FIEMAP_EXTENT_NOT_ALIGNED
    jnz .mapped                     ; no blocks of its own
    push rsi
    push rcx
    push r8
    sub rsp, 8
    mov rdi, [rsi + fiemap_extent.physical]
    mov rsi, [rsi + fiemap_extent.length]
    call send_run
    add rsp, 8
    pop r8
    pop rcx
    pop rsi
.mapped:
    test dword [rsi + fiemap_extent.flags], FIEMAP_EXTENT_LAST
    jz .not_last
    mov r8d, 1
.not_last:
    ; The previous extent is kept by copy: the next call overwrites
    ; fm_buf. Logical, physical and length are all the counting needs.
    mov rax, [rsi + fiemap_extent.logical]
    mov [prev + fiemap_extent.logical], rax
    mov rax, [rsi + fiemap_extent.physical]
    mov [prev + fiemap_extent.physical], rax
    mov rax, [rsi + fiemap_extent.length]
    mov [prev + fiemap_extent.length], rax
    lea r14, [prev]
    add rsi, fiemap_extent_size
    dec ecx
    jnz .extent
    mov r15, [r14 + fiemap_extent.logical]
    add r15, [r14 + fiemap_extent.length]
    test r8d, r8d
    jz .call
.done:
    mov rax, r12
    mov rdx, r13
.return:
    add rsp, 8
    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
.broken:
    mov rax, -EIO
    jmp .return

; extents_space(edi = a descriptor on the drive, esi = the block size,
;   not 0) -> rax = 0, or -errno when statx or GETFSMAP fails.
; Sends the drive's free space as MAP_FREE runs and what the filesystem
; keeps for itself as MAP_FILESYSTEM runs, in physical order. Blocks in use
; whose owner GETFSMAP does not know, or that an inode owns, are the
; walk's to send. Only the drive's own device is asked for, so an external
; journal's records, numbered on another device, never arrive. Runs sent
; before a failure stand. Clobbers rcx, rdx, rsi, rdi, r8 to r11.
extents_space:
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rbp
    sub rsp, 8
    mov ebx, edi                    ; ebx = the descriptor
    mov ebp, esi                    ; rbp = the block size
    mov edi, ebx
    lea rsi, [empty_name]
    mov edx, AT_EMPTY_PATH
    mov ecx, STATX_TYPE
    lea r8, [drive_statx]
    call sys_statx
    cmp rax, -MAX_ERRNO
    jae .return
    ; The device as GETFSMAP numbers it, new_encode_dev's 32 bits:
    ; minor's low byte, major above it, minor's other bits above that.
    mov eax, [drive_statx + kernel_statx.dev_minor]
    movzx r12d, al
    and eax, ~0xFF
    shl eax, 12
    or r12d, eax
    mov eax, [drive_statx + kernel_statx.dev_major]
    shl eax, 8
    or r12d, eax                    ; r12d = the device

    lea r13, [fsmap_buf]            ; r13 = the head
    mov rdi, r13
    mov ecx, fsmap_head_size
    xor eax, eax
    rep stosb
    mov dword [r13 + fsmap_head.count], FSMAP_PER_CALL
    ; Low key: the device's start. High key: the device's end.
    mov [r13 + fsmap_head.keys + fsmap.device], r12d
    lea rax, [r13 + fsmap_head.keys + fsmap_size]
    mov [rax + fsmap.device], r12d
    mov dword [rax + fsmap.flags], -1
    mov qword [rax + fsmap.physical], -1
    mov qword [rax + fsmap.owner], -1
    mov qword [rax + fsmap.offset], -1
    xor r12d, r12d                  ; r12 = where the last call's records ended
.call:
    mov edi, ebx
    mov esi, FS_IOC_GETFSMAP
    mov rdx, r13
    call sys_ioctl
    cmp rax, -EINTR
    je .call
    cmp rax, -MAX_ERRNO
    jae .return
    mov r14d, [r13 + fsmap_head.entries]
    test r14d, r14d
    jz .done
    cmp r14d, FSMAP_PER_CALL
    ja .broken                      ; more than the buffer holds
    lea r15, [r13 + fsmap_head_size]    ; r15 = this record
.record:
    test dword [r15 + fsmap.flags], FMR_OF_SPECIAL_OWNER
    jz .next                        ; an inode's
    mov rax, [r15 + fsmap.owner]
    cmp rax, FMR_OWN_UNKNOWN
    je .next                        ; in use, by an unknown owner
    mov edx, MAP_FREE
    cmp rax, FMR_OWN_FREE
    je .send
    mov edx, MAP_FILESYSTEM
.send:
    mov [map_kind], edx
    mov rdi, [r15 + fsmap.physical]
    mov rsi, [r15 + fsmap.length]
    call send_run
.next:
    add r15, fsmap_size
    dec r14d
    jnz .record
    lea rsi, [r15 - fsmap_size]     ; the last record returned
    test dword [rsi + fsmap.flags], FMR_OF_LAST
    jnz .done
    ; It becomes the low key, which asks for what follows it. A call that
    ; ends no further on would ask the same again, for ever.
    mov rax, [rsi + fsmap.physical]
    add rax, [rsi + fsmap.length]
    cmp rax, r12
    jbe .broken
    mov r12, rax
    lea rdi, [r13 + fsmap_head.keys]
    mov ecx, fsmap_size
    rep movsb
    jmp .call
.done:
    xor eax, eax
.return:
    add rsp, 8
    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
.broken:
    mov rax, -EIO
    jmp .return

; send_run(rdi = first byte, rsi = bytes; rbp = the block size) -> rax = 0.
;   Sends the blocks they touch as a map_kind run. Clobbers as rec_map_run.
send_run:
    sub rsp, 8
    lea rcx, [rdi + rsi]
    add rcx, rbp
    dec rcx                         ; rcx = one past the last byte, rounded up
    mov rax, rdi
    xor edx, edx
    div rbp
    mov rsi, rax                    ; rsi = the first block
    mov rax, rcx
    xor edx, edx
    div rbp
    sub rax, rsi
    mov rdx, rax                    ; rdx = blocks
    mov edi, [map_kind]
    call rec_map_run
    add rsp, 8
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
