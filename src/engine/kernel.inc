; Kernel ABI: system call numbers, flags, struct layouts and errno values
; the engine uses (x86-64 Linux). Changes only when a new kernel call is
; used. Values are from the kernel's uapi headers.
%ifndef KERNEL_INC
%define KERNEL_INC

; System call numbers (arch/x86/entry/syscalls/syscall_64.tbl).
SYS_write           equ 1
SYS_rt_sigaction    equ 13
SYS_geteuid         equ 107
SYS_exit_group      equ 231

; A system call result in [-4095, -1] is -errno.
MAX_ERRNO           equ 4095

; errno values (asm-generic/errno-base.h, errno.h).
EINTR               equ 4
EPIPE               equ 32
ENAMETOOLONG        equ 36

; Standard file descriptors.
STDIN_FD            equ 0
STDOUT_FD           equ 1

; Signals.
SIGPIPE             equ 13
SIG_IGN             equ 1

; The kernel's struct sigaction for rt_sigaction on x86-64
; (arch/x86/include/uapi/asm/signal.h), not the C library's.
struc kernel_sigaction
    .handler    resq 1
    .flags      resq 1
    .restorer   resq 1
    .mask       resq 1
endstruc
; rt_sigaction's sigsetsize: the kernel's sigset_t is 64 bits.
KERNEL_SIGSET_SIZE  equ 8

%endif
