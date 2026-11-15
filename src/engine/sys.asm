; System calls: thin wrappers that return the kernel's result unchanged,
; and process setup. Each returns rax as the kernel left it, so a value in
; [-4095, -1] is -errno and the caller checks it. Each keeps the System V
; convention: rdi, rsi, rdx in; rax out; rcx and r11 clobbered by syscall.
%if __?NASM_VERSION_ID?__ < 0x03020000
%error "NASM 3.02 or later required"
%endif

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
global sys_write
global sys_geteuid
global sys_exit
global sys_ignore_sigpipe

; sys_write(rdi = fd, rsi = buffer, rdx = length) -> rax = bytes written
; or -errno. May write fewer bytes than asked; the caller loops.
sys_write:
    mov eax, SYS_write
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
sys_ignore_sigpipe:
    mov eax, SYS_rt_sigaction
    mov edi, SIGPIPE
    lea rsi, [ignore_action]
    xor edx, edx
    mov r10d, KERNEL_SIGSET_SIZE
    syscall
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
