; hold-lease PATH: takes a write lease on PATH, prints "held", and keeps
; it until killed. For image_report (check-report.sh): while it holds the
; lease, a plain open of PATH waits up to /proc/sys/fs/lease-break-time
; for it to let go, which is how another user could stall a report.
; It ignores SIGIO, the signal that asks it to let go, so only the
; kernel's timeout ends the lease. Exits 1 on any failure.
; Built as the engine is, with no C library; its kernel numbers are its
; own, since kernel.inc lists only the engine's.

bits 64
default rel

SYS_write           equ 1
SYS_open            equ 2
SYS_rt_sigaction    equ 13
SYS_pause           equ 34
SYS_fcntl           equ 72
SYS_exit_group      equ 231
MAX_ERRNO           equ 4095
O_RDONLY            equ 0
F_SETLEASE          equ 1024
F_WRLCK             equ 1
SIGIO               equ 29
SIG_IGN             equ 1
SIGSET_SIZE         equ 8

section .rodata
alignb 8
ignore:                                 ; struct sigaction: SIG_IGN, nothing else
    dq SIG_IGN, 0, 0, 0
held:   db "held", 10
HELD_LEN equ $ - held

section .text
global _start

_start:
    cmp qword [rsp], 2                  ; argc: the program and PATH
    jne .fail
    mov eax, SYS_rt_sigaction
    mov edi, SIGIO
    lea rsi, [ignore]
    xor edx, edx
    mov r10d, SIGSET_SIZE
    syscall
    test rax, rax
    jnz .fail
    mov eax, SYS_open
    mov rdi, [rsp + 16]                 ; argv[1]
    mov esi, O_RDONLY
    syscall
    cmp rax, -MAX_ERRNO
    jae .fail
    mov edi, eax
    mov eax, SYS_fcntl
    mov esi, F_SETLEASE
    mov edx, F_WRLCK
    syscall
    test rax, rax
    jnz .fail
    mov eax, SYS_write
    mov edi, 1
    lea rsi, [held]
    mov edx, HELD_LEN
    syscall
    cmp rax, HELD_LEN
    jne .fail
.wait:
    mov eax, SYS_pause
    syscall
    jmp .wait
.fail:
    mov eax, SYS_exit_group
    mov edi, 1
    syscall

section .note.GNU-stack noalloc noexec nowrite progbits
