; Walk: visits every regular file under a mount point, the engine side of
; S3's "which files". One file at a time: walk_next hands each to its
; caller, so walk calls nothing but sys.asm (docs/design.md).
;
; The rules:
;   - It looks before it opens. Each name is first opened as a path only
;     (O_PATH), which runs no driver's open, so a device or pipe is never
;     opened. The file found is then opened again for reading through
;     /proc/self/fd, so nothing swapped in meanwhile is opened instead.
;   - It stays on the mount it started on: that path-only open crosses
;     no mount point (openat2 RESOLVE_NO_XDEV), so another mount is
;     never entered and an automount never fires. It is left out
;     silently. A btrfs subvolume inside the same mount is walked.
;   - It follows no symbolic link: every name is opened O_NOFOLLOW, and
;     a link is never visited.
;   - It changes nothing: every open for reading is O_NOATIME, so
;     reading a folder does not update its access time. O_NOATIME fails
;     with EPERM unless the caller owns the file or is root, so the walk
;     assumes root, as report runs.
;   - It visits regular files only. A hard link is visited once per
;     name, as filefrag would list it.
;   - Each folder holds one descriptor open while it is walked. A folder
;     that cannot be opened, past the process's descriptor limit too, is
;     skipped with its errno, and the walk goes on.
;   - Paths are relative to the mount point (DFR-0002 § 4.6). One longer
;     than STRING_MAX is skipped, stage 5, and not descended into.
;   - The top folder failing to read is a drive-level error, WALK_ERROR.
;   - It counts every name it reads, of any type, in walk_names, and
;     walk_start sets walk_estimate: REPORT_PROGRESS's two fields
;     (DFR-0002 § 4.6).

bits 64
default rel

%include "kernel.inc"
%include "records.inc"
%include "walk.inc"

extern sys_open
extern sys_openat2
extern sys_close
extern sys_statx
extern sys_fstatfs
extern sys_getdents64

; A folder at depth k has a path of at least 2k - 1 bytes ("a/b/..."),
; and none longer than STRING_MAX is descended into: so 2048 levels below
; the top, and the top itself. Unused .bss costs no memory until touched.
WALK_DEPTH_MAX  equ (STRING_MAX + 1) / 2 + 1
DENTS_SIZE      equ 4096

FOLDER_FLAGS    equ O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NOATIME | O_CLOEXEC
; The reopens through /proc/self/fd follow that link, as they must, to
; the file already checked; so no O_NOFOLLOW.
REOPEN_FOLDER   equ O_RDONLY | O_DIRECTORY | O_NOATIME | O_CLOEXEC
REOPEN_FILE     equ O_RDONLY | O_NOATIME | O_CLOEXEC
FD_DIGITS_MAX   equ 10                  ; a 32-bit fd's decimal digits

; One open folder: its descriptor, how far through its entries the walk
; is, and its path's length in walk_path.
struc walk_level
    .fd         resq 1
    .pos        resq 1
    .len        resq 1
    .path_len   resq 1
endstruc

section .rodata
empty_name:     db 0
proc_fd:        db "/proc/self/fd/"
PROC_FD_LEN     equ $ - proc_fd
alignb 8
path_only:                              ; how each name is first opened
    istruc open_how
        at open_how.flags,   dq O_PATH | O_NOFOLLOW | O_CLOEXEC
        at open_how.mode,    dq 0
        at open_how.resolve, dq RESOLVE_NO_XDEV
    iend

section .bss
alignb 8
levels:         resb walk_level_size * WALK_DEPTH_MAX
dents:          resb DENTS_SIZE * WALK_DEPTH_MAX    ; one buffer per level
depth:          resq 1                  ; the level being read
statx_buf:      resb kernel_statx_size
reopen_path:    resb PROC_FD_LEN + FD_DIGITS_MAX + 1
statfs_buf:     resb kernel_statfs_size
; The path of what walk_next returned: a folder's path of at most
; STRING_MAX bytes, a "/", and a name no longer than one entry.
walk_path:      resb STRING_MAX + 1 + DENTS_SIZE
walk_path_len:  resq 1                  ; at most STRING_MAX
walk_names:     resq 1                  ; names read, not counting . and ..
walk_estimate:  resq 1                  ; used inodes; 0 when unknown

section .text
global walk_start
global walk_next
global walk_path
global walk_path_len
global walk_names
global walk_estimate

; walk_start(rdi = NUL-terminated mount point, rsi = its mount id)
;   -> eax = OK, NOT_A_MOUNT or DRIVE_ERROR; edx = errno on DRIVE_ERROR,
;   the drive's block size on OK, else 0.
; Opens the top folder and checks it is that mount: a path swapped for
; another folder since mountinfo was read is NOT_A_MOUNT. A block size of
; 0 or above 32 bits is DRIVE_ERROR with EINVAL. On OK, sets
; walk_estimate and zeroes walk_names. Clobbers rax, rcx, rdx, rsi, rdi,
; r8 to r11.
walk_start:
    push rbx
    push r12
    push r13
    mov r12, rsi                    ; r12 = the mount id expected
    mov esi, FOLDER_FLAGS
    call sys_open
    cmp rax, -MAX_ERRNO
    jae .failed
    mov ebx, eax                    ; ebx = the top folder
    mov edi, ebx
    lea rsi, [empty_name]
    mov edx, AT_EMPTY_PATH
    call statx_of
    cmp rax, -MAX_ERRNO
    jae .failed_close
    cmp [statx_buf + kernel_statx.mnt_id], r12
    jne .not_a_mount
    mov edi, ebx
    lea rsi, [statfs_buf]
    call sys_fstatfs
    cmp rax, -MAX_ERRNO
    jae .failed_close
    mov r13, [statfs_buf + kernel_statfs.bsize]
    mov eax, r13d
    cmp rax, r13
    jne .bad_block_size             ; above 32 bits
    test eax, eax
    jz .bad_block_size
    mov rax, [statfs_buf + kernel_statfs.files]
    sub rax, [statfs_buf + kernel_statfs.ffree]
    jae .estimated                  ; no count gives 0 - 0
    xor eax, eax                    ; more free than in all: no estimate
.estimated:
    mov [walk_estimate], rax

    mov qword [walk_names], 0
    mov qword [depth], 0
    lea rax, [levels]
    mov [rax + walk_level.fd], rbx
    mov qword [rax + walk_level.pos], 0
    mov qword [rax + walk_level.len], 0
    mov qword [rax + walk_level.path_len], 0
    xor eax, eax
    mov edx, r13d
    jmp .return
.not_a_mount:
    mov edi, ebx
    call sys_close                  ; read-only; a failed close loses nothing
    mov eax, RESULT_NOT_A_MOUNT
    xor edx, edx
    jmp .return
.bad_block_size:
    mov rax, -EINVAL
.failed_close:
    neg rax
    mov r13, rax                    ; r13 = errno
    mov edi, ebx
    call sys_close
    mov eax, RESULT_DRIVE_ERROR
    mov edx, r13d
    jmp .return
.failed:
    neg eax
    mov edx, eax
    mov eax, RESULT_DRIVE_ERROR
.return:
    pop r13
    pop r12
    pop rbx
    ret

; walk_next() -> eax = WALK_FILE, WALK_SKIPPED, WALK_END or WALK_ERROR,
;   with edx and ecx as walk.inc says. For WALK_FILE and WALK_SKIPPED,
;   walk_path holds the path, walk_path_len bytes of it.
; Call after walk_start returned OK, and not again after WALK_END or
; WALK_ERROR. Clobbers rax, rcx, rdx, rsi, rdi, r8 to r11.
walk_next:
    push rbx
    push r12
    push r13
    push r14
    push r15
.next:
    mov rax, [depth]
    imul rbx, rax, walk_level_size
    lea rcx, [levels]
    add rbx, rcx                    ; rbx = this level
    imul r12, rax, DENTS_SIZE
    lea rcx, [dents]
    add r12, rcx                    ; r12 = its entries
    mov rax, [rbx + walk_level.pos]
    cmp rax, [rbx + walk_level.len]
    jb .entry
.read:
    mov rdi, [rbx + walk_level.fd]
    mov rsi, r12
    mov edx, DENTS_SIZE
    call sys_getdents64
    cmp rax, -EINTR
    je .read
    cmp rax, -MAX_ERRNO
    jae .read_failed
    test rax, rax
    jz .folder_done
    cmp rax, DENTS_SIZE
    ja .bad_entries
    mov [rbx + walk_level.len], rax
    mov qword [rbx + walk_level.pos], 0
    xor eax, eax

.entry:                             ; rax = this entry's offset
    lea rdx, [rax + linux_dirent64_size]
    cmp rdx, [rbx + walk_level.len]
    jae .bad_entries                ; no room for the fixed part and a name
    lea r14, [r12 + rax]            ; r14 = this entry
    movzx ecx, word [r14 + linux_dirent64.reclen]
    cmp ecx, linux_dirent64_size
    jbe .bad_entries                ; no room for a name
    lea rdx, [rax + rcx]
    cmp rdx, [rbx + walk_level.len]
    ja .bad_entries                 ; runs past what was read
    mov [rbx + walk_level.pos], rdx
    lea rdi, [r14 + linux_dirent64.name]
    mov rdx, rdi
    sub ecx, linux_dirent64_size    ; rcx = bytes the name may span
    xor eax, eax
    repne scasb
    jne .bad_entries                ; no NUL inside the entry
    mov r15, rdi
    sub r15, rdx
    dec r15                         ; r15 = the name's length
    jz .bad_entries
    cmp byte [rdx], '.'
    jne .named
    cmp r15, 1
    je .next                        ; "."
    cmp r15, 2
    jne .named
    cmp byte [rdx + 1], '.'
    je .next                        ; ".."
.named:
    inc qword [walk_names]
    mov r13, [rbx + walk_level.path_len]
    lea rdi, [walk_path]
    add rdi, r13
    test r13, r13
    jz .copy_name                   ; the top folder's path is empty
    mov byte [rdi], '/'
    inc rdi
    inc r13
.copy_name:
    mov rsi, rdx
    mov rcx, r15
    rep movsb
    add r13, r15                    ; r13 = the entry's path length

    movzx eax, byte [r14 + linux_dirent64.type]
    cmp eax, DT_DIR
    je .look
    cmp eax, DT_REG
    je .look
    cmp eax, DT_UNKNOWN
    jne .next                       ; a link, device, pipe or socket

.look:                              ; a path only, on this mount only
    mov rdi, [rbx + walk_level.fd]
    lea rsi, [r14 + linux_dirent64.name]
    lea rdx, [path_only]
    call sys_openat2
    cmp rax, -EXDEV
    je .next                        ; another mount: left out silently
    cmp rax, -MAX_ERRNO
    jae .open_failed
    mov r12, rax                    ; r12 = the name, as a path only
    mov edi, r12d
    lea rsi, [empty_name]
    mov edx, AT_EMPTY_PATH
    call statx_of
    cmp rax, -MAX_ERRNO
    jae .stat_failed_close
    movzx eax, word [statx_buf + kernel_statx.mode]
    and eax, S_IFMT
    cmp eax, S_IFDIR
    je .wanted
    cmp eax, S_IFREG
    jne .not_visited_close          ; another type, or swapped since listed
.wanted:                            ; eax = S_IFDIR or S_IFREG
    cmp r13, STRING_MAX
    ja .too_long_close
    cmp eax, S_IFDIR
    je .folder

    mov esi, REOPEN_FILE
    call reopen
    cmp rax, -MAX_ERRNO
    jae .open_failed
    mov [walk_path_len], r13
    mov edx, eax
    mov eax, WALK_FILE
    jmp .return

.folder:
    mov rax, [depth]
    inc rax
    cmp rax, WALK_DEPTH_MAX
    jae .too_long_close             ; cannot happen: the path is bounded
    mov esi, REOPEN_FOLDER
    call reopen
    cmp rax, -MAX_ERRNO
    jae .open_failed
    inc qword [depth]
    add rbx, walk_level_size
    mov [rbx + walk_level.fd], rax
    mov qword [rbx + walk_level.pos], 0
    mov qword [rbx + walk_level.len], 0
    mov [rbx + walk_level.path_len], r13
    jmp .next

.not_visited_close:
    mov edi, r12d
    call sys_close                  ; read-only; a failed close loses nothing
    jmp .next
.too_long_close:
    mov edi, r12d
    call sys_close
    jmp .too_long
.stat_failed_close:
    neg rax
    mov r15, rax                    ; r15 = errno
    mov edi, r12d
    call sys_close
    mov edx, r15d
    mov ecx, STAGE_STAT
    jmp .skipped
.open_failed:
    neg eax
    mov edx, eax
    mov ecx, STAGE_OPEN
    jmp .skipped
.too_long:
    mov edx, ENAMETOOLONG
    mov ecx, STAGE_PATH_TOO_LONG
.skipped:                           ; edx = errno, ecx = stage, r13 = length
    mov rax, r13
    cmp rax, STRING_MAX
    jbe .length_set
    mov eax, STRING_MAX             ; the record carries the first bytes
.length_set:
    mov [walk_path_len], rax
    mov eax, WALK_SKIPPED
    jmp .return

.bad_entries:                       ; the kernel's buffer broke its shape
    mov rax, -EIO
.read_failed:                       ; rax = -errno
    neg rax
    mov r15, rax                    ; r15 = errno
    mov rdi, [rbx + walk_level.fd]
    call sys_close
    cmp qword [depth], 0
    je .top_failed
    mov rax, [rbx + walk_level.path_len]
    mov [walk_path_len], rax        ; the folder's own path, still in place
    dec qword [depth]
    mov eax, WALK_SKIPPED
    mov edx, r15d
    mov ecx, STAGE_READ_FOLDER
    jmp .return
.top_failed:
    mov eax, WALK_ERROR
    mov edx, r15d
    jmp .return
.folder_done:
    mov rdi, [rbx + walk_level.fd]
    call sys_close
    cmp qword [depth], 0
    je .end
    dec qword [depth]
    jmp .next
.end:
    mov eax, WALK_END
.return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; reopen(r12 = a path-only fd, esi = open flags) -> rax = a new fd on
;   the very file r12 holds, or -errno. Opens it through /proc/self/fd,
;   so nothing swapped in under its name since is opened instead. Closes
;   r12 either way. Clobbers as walk_next.
; Writes r12's digits itself: walk may call only sys.asm and records.asm
; (docs/design.md), so probe's write_decimal is out of reach.
reopen:
    push rbx
    mov ebx, esi                    ; ebx = the flags
    lea rdi, [reopen_path + PROC_FD_LEN + FD_DIGITS_MAX]
    mov byte [rdi], 0
    mov eax, r12d
    mov ecx, 10
.digit:                             ; last digit first, leftward
    xor edx, edx
    div ecx
    add dl, '0'
    dec rdi
    mov [rdi], dl
    test eax, eax
    jnz .digit
    sub rdi, PROC_FD_LEN            ; the prefix goes just before them
    mov rdx, rdi                    ; rdx = the path's start
    lea rsi, [proc_fd]
    mov ecx, PROC_FD_LEN
    rep movsb
    mov rdi, rdx
    mov esi, ebx
    call sys_open
    mov rbx, rax                    ; rbx = the new fd, or -errno
    mov edi, r12d
    call sys_close                  ; read-only; a failed close loses nothing
    mov rax, rbx
    pop rbx
    ret

; statx_of(edi = folder fd, rsi = NUL-terminated name, edx = AT_ flags)
;   With an empty name and AT_EMPTY_PATH, edi is the file itself.
;   -> rax = 0, or -errno. Fills statx_buf with the type and mount id;
;   a kernel that cannot give both (before Linux 5.8) is -EOPNOTSUPP.
; Clobbers as walk_next.
statx_of:
    sub rsp, 8
    mov ecx, STATX_TYPE | STATX_MNT_ID
    lea r8, [statx_buf]
    call sys_statx
    cmp rax, -MAX_ERRNO
    jae .return
    mov ecx, [statx_buf + kernel_statx.mask]
    and ecx, STATX_TYPE | STATX_MNT_ID
    cmp ecx, STATX_TYPE | STATX_MNT_ID
    je .return                      ; rax = 0
    mov rax, -EOPNOTSUPP
.return:
    add rsp, 8
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
