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
SYS_ioctl           equ 16
SYS_geteuid         equ 107
SYS_fstatfs         equ 138
SYS_getdents64      equ 217
SYS_exit_group      equ 231
SYS_statx           equ 332
SYS_openat2         equ 437

; A system call result in [-4095, -1] is -errno.
MAX_ERRNO           equ 4095

; errno values (asm-generic/errno-base.h, errno.h).
EINTR               equ 4
EIO                 equ 5
EXDEV               equ 18
EINVAL              equ 22
EPIPE               equ 32
ENAMETOOLONG        equ 36
EOPNOTSUPP          equ 95

; open flags (asm-generic/fcntl.h).
O_RDONLY            equ 0
O_DIRECTORY         equ 0o200000
O_NOFOLLOW          equ 0o400000
O_NOATIME           equ 0o1000000
O_CLOEXEC           equ 0o2000000
O_PATH              equ 0o10000000

; *at flags (linux/fcntl.h): act on the fd itself when the name is "".
AT_EMPTY_PATH       equ 0x1000

; openat2's struct open_how and resolve flags (linux/openat2.h).
struc open_how
    .flags      resq 1
    .mode       resq 1
    .resolve    resq 1
endstruc
RESOLVE_NO_XDEV     equ 0x01            ; cross no mount point: EXDEV

; Standard file descriptors.
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
S_IFREG             equ 0o100000
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

; The kernel's struct statx (linux/stat.h). Only the fields the engine
; reads are named. stx_mask says which fields the kernel filled.
struc kernel_statx
    .mask       resd 1
    .blksize    resd 1
    .attributes resq 1
    .nlink      resd 1
    .uid        resd 1
    .gid        resd 1
    .mode       resw 1
    .spare0     resw 1
    .rest1      resb 136 - 32
    .dev_major  resd 1          ; the device the file is on; always filled
    .dev_minor  resd 1
    .mnt_id     resq 1
    .rest2      resb 256 - 152
endstruc
; stx_mask bits asked for and checked.
STATX_TYPE          equ 0x0001
STATX_MNT_ID        equ 0x1000

; One entry of getdents64's buffer (linux/dirent.h). d_name is
; NUL-terminated; d_reclen is the whole entry's size, to the next one.
struc linux_dirent64
    .ino        resq 1
    .off        resq 1
    .reclen     resw 1
    .type       resb 1
    .name       resb 0
endstruc
; d_type values (DT_* in dirent.h). DT_UNKNOWN means: stat it to know.
DT_UNKNOWN          equ 0
DT_DIR              equ 4
DT_REG              equ 8

; FS_IOC_FIEMAP: _IOWR('f', 11, struct fiemap) (linux/fs.h, linux/fiemap.h).
FS_IOC_FIEMAP       equ 0xC020660B
struc fiemap
    .start          resq 1
    .length         resq 1
    .flags          resd 1
    .mapped_extents resd 1
    .extent_count   resd 1
    .reserved       resd 1
endstruc
struc fiemap_extent
    .logical        resq 1
    .physical       resq 1
    .length         resq 1
    .reserved64     resq 2
    .flags          resd 1
    .reserved       resd 3
endstruc
; fe_flags: the file's last extent; an extent with no place on the drive
; yet (delayed allocation sets it too); one sharing its block with other
; data (inline data and tails set it).
FIEMAP_EXTENT_LAST          equ 0x001
FIEMAP_EXTENT_UNKNOWN       equ 0x002
FIEMAP_EXTENT_NOT_ALIGNED   equ 0x100

; FS_IOC_GETFSMAP: _IOWR('X', 59, struct fsmap_head) (linux/fsmap.h).
; Positions and lengths are in bytes. A query returns the records between
; two keys; the last record returned, as the low key, asks for the next.
FS_IOC_GETFSMAP     equ 0xC0C0583B
struc fsmap
    .device         resd 1      ; as new_encode_dev encodes it
    .flags          resd 1
    .physical       resq 1
    .owner          resq 1
    .offset         resq 1
    .length         resq 1
    .reserved       resq 3
endstruc
struc fsmap_head
    .iflags         resd 1
    .oflags         resd 1
    .count          resd 1      ; records the buffer holds
    .entries        resd 1      ; records returned
    .reserved       resq 6
    .keys           resb 2 * fsmap_size
endstruc                        ; the records follow
; fmr_flags: the owner is special, not an inode; the drive's last record.
FMR_OF_SPECIAL_OWNER    equ 0x10
FMR_OF_LAST             equ 0x20
; Special owners: free space; in use, owner not known (ext4's file data).
FMR_OWN_FREE            equ 1
FMR_OWN_UNKNOWN         equ 2

%endif
