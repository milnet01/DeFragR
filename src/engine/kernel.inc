; Kernel ABI: system call numbers, flags, struct layouts and errno values
; the engine uses (x86-64 Linux). Changes only when a new kernel call is
; used. Values are from the kernel's uapi headers.
%ifndef KERNEL_INC
%define KERNEL_INC

; System call numbers (arch/x86/entry/syscalls/syscall_64.tbl).
SYS_read            equ 0
SYS_write           equ 1
SYS_open            equ 2
SYS_close           equ 3
SYS_stat            equ 4
SYS_rt_sigaction    equ 13
SYS_geteuid         equ 107
SYS_statfs          equ 137
SYS_exit_group      equ 231

; A system call result in [-4095, -1] is -errno.
MAX_ERRNO           equ 4095

; errno values (asm-generic/errno-base.h, errno.h).
EINTR               equ 4
EPIPE               equ 32
ENAMETOOLONG        equ 36

; open flags (asm-generic/fcntl.h).
O_RDONLY            equ 0
O_CLOEXEC           equ 0o2000000

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

; The kernel's struct stat for stat on x86-64
; (arch/x86/include/uapi/asm/stat.h). Only the fields the engine reads
; are named; the rest is padding to the full size.
struc kernel_stat
    .dev        resq 1
    .ino        resq 1
    .nlink      resq 1
    .mode       resd 1
    .uid        resd 1
    .gid        resd 1
    .pad0       resd 1
    .rdev       resq 1
    .rest       resb 144 - 48
endstruc
S_IFMT              equ 0o170000
S_IFDIR             equ 0o040000
S_IFBLK             equ 0o060000

; The kernel's struct statfs on x86-64 (asm-generic/statfs.h, 64-bit
; longs).
struc kernel_statfs
    .type       resq 1
    .bsize      resq 1
    .blocks     resq 1
    .bfree      resq 1
    .bavail     resq 1
    .files      resq 1
    .ffree      resq 1
    .fsid       resq 1
    .namelen    resq 1
    .frsize     resq 1
    .flags      resq 1
    .spare      resq 4
endstruc
; statfs f_flags: the mount or its filesystem is read-only.
ST_RDONLY           equ 1

; st_rdev splits as glibc's gnu_dev_major and gnu_dev_minor split it:
;   major = (dev >> 8 & 0xfff)  | (dev >> 32 & ~0xfff)
;   minor = (dev & 0xff)        | (dev >> 12 & ~0xff)
DEV_MAJOR_LOW_MASK  equ 0xfff
DEV_MINOR_LOW_MASK  equ 0xff

%endif
