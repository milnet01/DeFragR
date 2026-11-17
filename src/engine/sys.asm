; System calls: thin wrappers that return the kernel's result unchanged,
; and process setup. Each returns rax as the kernel left it, so a value in
; [-4095, -1] is -errno and the caller checks it. Each keeps the System V
; convention: rdi, rsi, rdx in; rax out; rcx and r11 clobbered by syscall.
; A wrapper that overwrites any other register says so.

bits 64
default rel

%include "kernel.inc"

section .rodata
; Handler SIG_IGN, no flags, empty mask: no handler runs, so no restorer.
ignore_action:
    istruc kernel_sigaction
        at kernel_sigaction.handler,  dq SIG_IGN
        at kernel_sigaction.flags,    dq 0
        at kernel_sigaction.restorer, dq 0
        at kernel_sigaction.mask,     dq 0
    iend

section .text
global sys_read
global sys_write
global sys_open
global sys_close
global sys_stat
global sys_openat2
global sys_statx
global sys_fstatfs
global sys_getdents64
global sys_ioctl
global sys_geteuid
global sys_exit
global sys_ignore_sigpipe

; sys_write(rdi = fd, rsi = buffer, rdx = length) -> rax = bytes written
; or -errno. May write fewer bytes than asked; the caller loops.
sys_write:
    mov eax, SYS_write
    syscall
    ret

; sys_read(rdi = fd, rsi = buffer, rdx = length) -> rax = bytes read,
; 0 at end of file, or -errno.
sys_read:
    mov eax, SYS_read
    syscall
    ret

; sys_open(rdi = NUL-terminated path, rsi = flags) -> rax = fd or -errno.
; Opens an existing file only, so no mode is passed. Clobbers rdx too.
sys_open:
    mov eax, SYS_open
    xor edx, edx
    syscall
    ret

; sys_close(rdi = fd) -> rax = 0 or -errno.
sys_close:
    mov eax, SYS_close
    syscall
    ret

; sys_stat(rdi = NUL-terminated path, rsi = kernel_stat buffer)
;   -> rax = 0 or -errno. Follows symbolic links.
sys_stat:
    mov eax, SYS_stat
    syscall
    ret

; sys_openat2(rdi = folder fd, rsi = NUL-terminated name, rdx = open_how)
;   -> rax = fd or -errno. Clobbers r10 too.
sys_openat2:
    mov eax, SYS_openat2
    mov r10d, open_how_size
    syscall
    ret

; sys_statx(rdi = folder fd, rsi = NUL-terminated name, rdx = flags,
;   rcx = mask, r8 = kernel_statx buffer) -> rax = 0 or -errno.
; Clobbers r10 too.
sys_statx:
    mov eax, SYS_statx
    mov r10, rcx
    syscall
    ret

; sys_fstatfs(rdi = fd, rsi = kernel_statfs buffer) -> rax = 0 or -errno.
sys_fstatfs:
    mov eax, SYS_fstatfs
    syscall
    ret

; sys_getdents64(rdi = folder fd, rsi = buffer, rdx = its size)
;   -> rax = bytes of linux_dirent64 entries, 0 at the folder's end, or
;   -errno.
sys_getdents64:
    mov eax, SYS_getdents64
    syscall
    ret

; sys_ioctl(rdi = fd, rsi = request, rdx = argument) -> rax as the
; request defines it, or -errno.
sys_ioctl:
    mov eax, SYS_ioctl
    syscall
    ret

; sys_geteuid() -> rax = effective user id. Cannot fail.
sys_geteuid:
    mov eax, SYS_geteuid
    syscall
    ret

; sys_exit(rdi = status) -> does not return. Ends every thread.
sys_exit:
    mov eax, SYS_exit_group
    syscall
    ud2

; sys_ignore_sigpipe() -> rax = 0 or -errno. A write to a closed pipe
; then fails with EPIPE instead of killing the engine (DFR-0002 § 6).
; Clobbers rdi, rsi, rdx and r10 too.
sys_ignore_sigpipe:
    mov eax, SYS_rt_sigaction
    mov edi, SIGPIPE
    lea rsi, [ignore_action]
    xor edx, edx
    mov r10d, KERNEL_SIGSET_SIZE
    syscall
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
