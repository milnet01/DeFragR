; Probe: lists the mounted drives and judges each one, the engine side of
; S1 and S9. This file is the one home of the rules (ADR-0002); the
; record layout is docs/specs/DFR-0002-engine-protocol.md § 4.5 and § 4.7.
; Needs no admin rights.
;
; A mount in /proc/self/mountinfo is a drive when all of these hold:
;   - its device is a block device: the major of mountinfo's major:minor
;     is non-zero, or, where it is 0 (btrfs), the mount source is an
;     absolute path to a block device, whose number is used instead;
;   - its mount point is a folder this user can stat;
;   - its mount point, source and filesystem type each fit a record
;     string (§ 4.1).
; Any other mount is left out. A device mounted in several places is
; listed once, at its first mount in mountinfo order (the user's choice,
; 2026-11-15).
;
; Each drive's reason is the first of these that applies (§ 4.7):
;   1 FS_NOT_EXT4     the filesystem type is not exactly "ext4";
;   2 READ_ONLY       statfs reports ST_RDONLY;
;   3 SSD             the device's rotational file reads 0;
;   4 MEDIA_UNKNOWN   it cannot be read, or reads neither 0 nor 1.
; No reason gives verdict 2, report and defrag; any reason, verdict 1.
;
; The rotational file is /sys/dev/block/MAJ:MIN/queue/rotational, or, for
; a partition, its disk's at /sys/dev/block/MAJ:MIN/../queue/rotational:
; the /sys/block/*/queue/rotational files ADR-0002 names, reached by
; device number.
%if __?NASM_VERSION_ID?__ < 0x03020000
%error "NASM 3.02 or later required"
%endif

bits 64
default rel

%include "kernel.inc"
%include "records.inc"

extern sys_read
extern sys_open
extern sys_close
extern sys_stat
extern sys_statfs
extern rec_start
extern rec_string
extern rec_send

; Limits. Reaching one ends the probe with INTERNAL rather than a partial
; list. Unused .bss costs no memory until touched.
MOUNTINFO_MAX   equ 4 * 1024 * 1024
DEVICES_MAX     equ 4096
SYS_PATH_MAX    equ 64              ; longest: prefix, two u32s, ':', rot_part

section .rodata
mountinfo_path: db "/proc/self/mountinfo", 0
sys_prefix:     db "/sys/dev/block/"
sys_prefix_len  equ $ - sys_prefix
rot_disk:       db "/queue/rotational", 0
rot_disk_len    equ $ - rot_disk
rot_part:       db "/../queue/rotational", 0
rot_part_len    equ $ - rot_part
fs_ext4:        db "ext4"
fs_ext4_len     equ $ - fs_ext4

section .bss
alignb 8
mi_buf:         resb MOUNTINFO_MAX
mi_len:         resq 1
devices:        resq DEVICES_MAX    ; (major << 32) | minor, one per drive sent
devices_count:  resq 1
stat_buf:       resb kernel_stat_size
statfs_buf:     resb kernel_statfs_size
; The line being judged: each field as a pointer, then a length.
fld_dev:        resq 2
fld_mount:      resq 2
fld_fs:         resq 2
fld_source:     resq 2
; Unescaped copies, each NUL-terminated for the kernel.
mount_buf:      resb STRING_MAX + 8
mount_len:      resq 1
source_buf:     resb STRING_MAX + 8
source_len:     resq 1
fs_buf:         resb STRING_MAX + 8
fs_len:         resq 1
sys_path:       resb SYS_PATH_MAX
rot_bytes:      resb 8
dev_major:      resd 1
dev_minor:      resd 1

section .text
global probe_run

; probe_run() -> eax = result code, edx = errno or 0.
; Sends one DRIVE record per drive. On OK the caller sends FINISHED. Fails
; with MOUNTINFO when mountinfo cannot be read (errno set) or has a line
; of an unknown shape (errno 0); DRIVE_ERROR when statfs fails on a
; drive; INTERNAL at a limit. Clobbers rax, rcx, rdx, rsi, rdi, r8 to r11.
probe_run:
    push rbx
    push r12
    push r13
    call read_mountinfo
    test eax, eax
    jnz .return
    lea r12, [mi_buf]               ; r12 = this line
    mov r13, r12
    add r13, [mi_len]               ; r13 = end of the text
.line:
    cmp r12, r13
    jae .done
    mov rdi, r12
    mov rcx, r13
    sub rcx, r12
    mov al, 10
    repne scasb                     ; rdi = past the newline, or the end
    mov rbx, rdi                    ; rbx = next line
    mov rsi, rdi
    jne .last_line                  ; no newline: the line runs to the end
    dec rsi                         ; the line ends before its newline
.last_line:
    mov rdi, r12
    call probe_line
    test eax, eax
    jnz .return
    mov r12, rbx
    jmp .line
.done:
    xor eax, eax
    xor edx, edx
.return:
    pop r13
    pop r12
    pop rbx
    ret

; read_mountinfo() -> eax = OK, MOUNTINFO or INTERNAL; edx = errno or 0.
; Reads /proc/self/mountinfo whole into mi_buf and sets mi_len. INTERNAL
; when it fills mi_buf. Clobbers as probe_run.
read_mountinfo:
    push rbx
    push r12
    sub rsp, 8
    lea rdi, [mountinfo_path]
    mov esi, O_RDONLY | O_CLOEXEC
    call sys_open
    cmp rax, -MAX_ERRNO
    jae .open_failed
    mov ebx, eax                    ; ebx = fd
    xor r12d, r12d                  ; r12 = bytes read
.read:
    mov edx, MOUNTINFO_MAX
    sub rdx, r12
    jz .full
    mov edi, ebx
    lea rsi, [mi_buf]
    add rsi, r12
    call sys_read
    cmp rax, -MAX_ERRNO
    jae .read_failed
    test rax, rax
    jz .end_of_file
    add r12, rax
    jmp .read
.read_failed:
    cmp rax, -EINTR
    je .read
    neg eax
    mov r12d, eax                   ; r12 = errno
    mov edi, ebx
    call sys_close
    mov eax, RESULT_MOUNTINFO
    mov edx, r12d
    jmp .return
.full:
    mov edi, ebx
    call sys_close
    mov eax, RESULT_INTERNAL
    xor edx, edx
    jmp .return
.end_of_file:
    mov [mi_len], r12
    mov edi, ebx
    call sys_close                  ; the text is read; a failed close loses nothing
    xor eax, eax
    xor edx, edx
    jmp .return
.open_failed:
    neg eax
    mov edx, eax
    mov eax, RESULT_MOUNTINFO
.return:
    add rsp, 8
    pop r12
    pop rbx
    ret

; probe_line(rdi = line start, rsi = line end, before any newline)
;   -> eax = result code, edx = errno or 0, as probe_run.
; Judges one mountinfo line and sends its DRIVE record if it is a drive.
; The line is: id parent major:minor root mount-point options
; [optional fields...] - fs-type source super-options.
; Clobbers as probe_run.
probe_line:
    push rbx
    push r13
    sub rsp, 8
    call next_field                 ; mount id
    call next_field                 ; parent id
    call next_field
    mov [fld_dev], rax
    mov [fld_dev + 8], rdx
    call next_field                 ; root
    call next_field
    mov [fld_mount], rax
    mov [fld_mount + 8], rdx
    call next_field                 ; mount options
.optional:                          ; optional fields, ended by a lone "-"
    call next_field
    test rdx, rdx
    jz .malformed                   ; the line ended with no "-"
    cmp rdx, 1
    jne .optional
    cmp byte [rax], '-'
    jne .optional
    call next_field
    mov [fld_fs], rax
    mov [fld_fs + 8], rdx
    call next_field
    mov [fld_source], rax
    mov [fld_source + 8], rdx

    mov rdi, [fld_mount]
    mov rsi, [fld_mount + 8]
    lea rdx, [mount_buf]
    call unescape
    test rax, rax
    js .skip
    mov [mount_len], rax
    mov rdi, [fld_source]
    mov rsi, [fld_source + 8]
    lea rdx, [source_buf]
    call unescape
    test rax, rax
    js .skip
    mov [source_len], rax
    mov rdi, [fld_fs]
    mov rsi, [fld_fs + 8]
    lea rdx, [fs_buf]
    call unescape
    test rax, rax
    js .skip
    mov [fs_len], rax

    mov rdi, [fld_dev]
    mov rsi, [fld_dev + 8]
    call parse_dev
    test rax, rax
    js .malformed
    cmp dword [dev_major], 0
    jne .have_device
    ; Major 0 is no block device; btrfs shows that for every mount, so
    ; its source names the device. A relative source is not a path.
    cmp byte [source_buf], '/'
    jne .skip
    lea rdi, [source_buf]
    lea rsi, [stat_buf]
    call sys_stat
    cmp rax, -MAX_ERRNO
    jae .skip
    mov eax, [stat_buf + kernel_stat.mode]
    and eax, S_IFMT
    cmp eax, S_IFBLK
    jne .skip
    mov rax, [stat_buf + kernel_stat.rdev]
    mov rcx, rax
    shr rcx, 8
    and ecx, DEV_MAJOR_LOW_MASK
    mov rdx, rax
    shr rdx, 32
    and edx, ~DEV_MAJOR_LOW_MASK
    or ecx, edx
    mov [dev_major], ecx
    mov ecx, eax
    and ecx, DEV_MINOR_LOW_MASK
    mov rdx, rax
    shr rdx, 12
    and edx, ~DEV_MINOR_LOW_MASK
    or ecx, edx
    mov [dev_minor], ecx

.have_device:
    lea rdi, [mount_buf]
    lea rsi, [stat_buf]
    call sys_stat
    cmp rax, -MAX_ERRNO
    jae .skip
    mov eax, [stat_buf + kernel_stat.mode]
    and eax, S_IFMT
    cmp eax, S_IFDIR
    jne .skip                       ; a file bind-mounted, e.g. in a container

    mov eax, [dev_major]
    shl rax, 32
    mov ecx, [dev_minor]
    or rax, rcx                     ; rax = this device's key
    lea rdi, [devices]
    mov rcx, [devices_count]
.seen:
    test rcx, rcx
    jz .new_device
    cmp [rdi], rax
    je .skip                        ; listed already, at an earlier mount
    add rdi, 8
    dec rcx
    jmp .seen
.new_device:
    mov rcx, [devices_count]
    cmp rcx, DEVICES_MAX
    jae .internal
    mov [rdi], rax                  ; rdi is devices + count * 8
    inc rcx
    mov [devices_count], rcx

    lea rdi, [mount_buf]
    lea rsi, [statfs_buf]
    call sys_statfs
    cmp rax, -MAX_ERRNO
    jae .statfs_failed
    call media_of
    mov ebx, eax                    ; ebx = media

    mov r13d, REASON_FS_NOT_EXT4    ; r13d = reason, in § 4.7's order
    cmp qword [fs_len], fs_ext4_len
    jne .judged
    mov eax, [fs_buf]
    cmp eax, [fs_ext4]
    jne .judged
    mov r13d, REASON_READ_ONLY
    test qword [statfs_buf + kernel_statfs.flags], ST_RDONLY
    jnz .judged
    mov r13d, REASON_SSD
    cmp ebx, MEDIA_SSD
    je .judged
    mov r13d, REASON_MEDIA_UNKNOWN
    cmp ebx, MEDIA_UNKNOWN
    je .judged
    mov r13d, REASON_NONE
.judged:

    mov edi, REC_DRIVE
    mov esi, drive_size
    call rec_start
    mov rcx, [statfs_buf + kernel_statfs.blocks]
    mov [rax + drive.total_blocks], rcx
    mov rcx, [statfs_buf + kernel_statfs.bfree]
    mov [rax + drive.free_blocks], rcx
    mov rcx, [statfs_buf + kernel_statfs.bsize]
    mov [rax + drive.block_size], ecx
    mov ecx, [dev_major]
    mov [rax + drive.dev_major], ecx
    mov ecx, [dev_minor]
    mov [rax + drive.dev_minor], ecx
    mov [rax + drive.media], bl
    mov byte [rax + drive.verdict], VERDICT_DEFRAG
    test r13d, r13d
    jz .verdict_set
    mov byte [rax + drive.verdict], VERDICT_REPORT_ONLY
.verdict_set:
    mov [rax + drive.reason], r13w
    test qword [statfs_buf + kernel_statfs.flags], ST_RDONLY
    jz .flags_set
    mov dword [rax + drive.mount_flags], MOUNT_READ_ONLY
.flags_set:
    lea rdi, [mount_buf]
    mov rsi, [mount_len]
    call rec_string
    test rax, rax
    jnz .internal
    lea rdi, [source_buf]
    mov rsi, [source_len]
    call rec_string
    test rax, rax
    jnz .internal
    lea rdi, [fs_buf]
    mov rsi, [fs_len]
    call rec_string
    test rax, rax
    jnz .internal
    call rec_send

.skip:                              ; judged; listed or left out
    xor eax, eax
    xor edx, edx
    jmp .return
.malformed:
    mov eax, RESULT_MOUNTINFO
    xor edx, edx
    jmp .return
.statfs_failed:
    neg eax
    mov edx, eax
    mov eax, RESULT_DRIVE_ERROR
    jmp .return
.internal:
    mov eax, RESULT_INTERNAL
    xor edx, edx
.return:
    add rsp, 8
    pop r13
    pop rbx
    ret

; next_field(rdi = cursor, rsi = line end) -> rax = field start,
;   rdx = field length, rdi = past the field and its space.
; Fields are split by single spaces. At the line end the field is empty.
; Keeps rsi. Clobbers nothing else.
next_field:
    mov rax, rdi
.scan:
    cmp rdi, rsi
    jae .end
    cmp byte [rdi], ' '
    je .space
    inc rdi
    jmp .scan
.space:
    mov rdx, rdi
    sub rdx, rax
    inc rdi
    ret
.end:
    mov rdx, rdi
    sub rdx, rax
    ret

; unescape(rdi = bytes, rsi = count, rdx = buffer of STRING_MAX + 1)
;   -> rax = bytes written, or -1 when more than STRING_MAX.
; Copies a mountinfo field, turning each backslash and three octal digits
; back into the byte the kernel escaped (a space is \040), then a NUL.
; Clobbers rcx, rsi, rdi, r8 to r10.
unescape:
    add rsi, rdi                    ; rsi = end of the field
    xor ecx, ecx                    ; rcx = bytes written
.next:
    cmp rdi, rsi
    jae .done
    movzx eax, byte [rdi]
    cmp al, '\'
    jne .store
    lea r8, [rdi + 4]
    cmp r8, rsi
    ja .store                       ; too short for an escape: keep it
    movzx r8d, byte [rdi + 1]
    sub r8d, '0'
    cmp r8d, 7
    ja .store
    movzx r9d, byte [rdi + 2]
    sub r9d, '0'
    cmp r9d, 7
    ja .store
    movzx r10d, byte [rdi + 3]
    sub r10d, '0'
    cmp r10d, 7
    ja .store
    shl r8d, 6
    shl r9d, 3
    lea eax, [r8 + r9]
    add eax, r10d                   ; the low byte is the escaped byte
    add rdi, 3
.store:
    cmp rcx, STRING_MAX
    jae .too_long
    mov [rdx + rcx], al
    inc rcx
    inc rdi
    jmp .next
.done:
    mov byte [rdx + rcx], 0
    mov rax, rcx
    ret
.too_long:
    mov rax, -1
    ret

; parse_dev(rdi = field, rsi = length) -> rax = 0, or -1 when it is not
; MAJOR:MINOR in decimal. Sets dev_major and dev_minor.
; Clobbers rcx, rdx, rsi, rdi.
parse_dev:
    sub rsp, 8
    add rsi, rdi                    ; rsi = end of the field
    call parse_decimal
    test rax, rax
    js .bad
    mov [dev_major], eax
    cmp rdi, rsi
    jae .bad
    cmp byte [rdi], ':'
    jne .bad
    inc rdi
    call parse_decimal
    test rax, rax
    js .bad
    cmp rdi, rsi
    jne .bad
    mov [dev_minor], eax
    xor eax, eax
    add rsp, 8
    ret
.bad:
    mov rax, -1
    add rsp, 8
    ret

; parse_decimal(rdi = cursor, rsi = end) -> rax = the value, rdi = past
; its digits; or rax = -1 when there are no digits or it exceeds 32 bits.
; Clobbers rcx, rdx.
parse_decimal:
    xor eax, eax
    mov rdx, rdi                    ; rdx = first digit
.digit:
    cmp rdi, rsi
    jae .end
    movzx ecx, byte [rdi]
    sub ecx, '0'
    cmp ecx, 9
    ja .end
    imul rax, rax, 10
    add rax, rcx
    mov rcx, rax
    shr rcx, 32
    jnz .bad
    inc rdi
    jmp .digit
.end:
    cmp rdi, rdx
    je .bad
    ret
.bad:
    mov rax, -1
    ret

; media_of() -> eax = MEDIA_SSD, MEDIA_SPINNING or MEDIA_UNKNOWN for the
; device in dev_major and dev_minor. Tries the device's own rotational
; file, then its disk's. Clobbers as probe_run.
media_of:
    sub rsp, 8
    lea rdx, [rot_disk]
    mov ecx, rot_disk_len
    call rotational
    cmp eax, -1
    jne .judge
    lea rdx, [rot_part]
    mov ecx, rot_part_len
    call rotational
.judge:
    mov edx, eax
    mov eax, MEDIA_SSD
    cmp edx, '0'
    je .return
    mov eax, MEDIA_SPINNING
    cmp edx, '1'
    je .return
    mov eax, MEDIA_UNKNOWN
.return:
    add rsp, 8
    ret

; rotational(rdx = path suffix, ecx = its length with its NUL)
;   -> eax = the first byte of /sys/dev/block/MAJ:MIN<suffix> when the
;   file holds one byte and a newline, else -1. Clobbers as probe_run.
rotational:
    push rbx
    push r12
    sub rsp, 8
    mov rbx, rdx
    mov r12d, ecx
    lea rdi, [sys_path]
    lea rsi, [sys_prefix]
    mov ecx, sys_prefix_len
    rep movsb
    mov esi, [dev_major]
    call write_decimal
    mov byte [rdi], ':'
    inc rdi
    mov esi, [dev_minor]
    call write_decimal
    mov rsi, rbx
    mov ecx, r12d
    rep movsb
    lea rdi, [sys_path]
    mov esi, O_RDONLY | O_CLOEXEC
    call sys_open
    cmp rax, -MAX_ERRNO
    jae .unreadable
    mov ebx, eax                    ; ebx = fd
.read:
    mov edi, ebx
    lea rsi, [rot_bytes]
    mov edx, 3                      ; one more than expected, to see extra
    call sys_read
    cmp rax, -EINTR
    je .read
    mov r12, rax                    ; r12 = bytes read, or -errno
    mov edi, ebx
    call sys_close                  ; read-only; a failed close loses nothing
    cmp r12, 2
    jne .unreadable
    cmp byte [rot_bytes + 1], 10
    jne .unreadable
    movzx eax, byte [rot_bytes]
    jmp .return
.unreadable:
    mov eax, -1
.return:
    add rsp, 8
    pop r12
    pop rbx
    ret

; write_decimal(rdi = destination, esi = value) -> rdi = past the digits.
; Builds the digits in the red zone, which this leaf may use.
; Clobbers rax, rcx, rdx, r8, r9.
write_decimal:
    mov eax, esi
    mov ecx, 10
    lea r8, [rsp - 1]               ; digits go downward from here
    mov r9, r8                      ; r9 = the last digit
.divide:
    xor edx, edx
    div ecx
    add dl, '0'
    mov [r8], dl
    dec r8
    test eax, eax
    jnz .divide
.copy:
    inc r8
    mov dl, [r8]
    mov [rdi], dl
    inc rdi
    cmp r8, r9
    jne .copy
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
