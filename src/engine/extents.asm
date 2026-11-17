; Extents: how many pieces a file is in, read with FS_IOC_FIEMAP. The
; engine side of S3's figures. Calls nothing but sys.asm (docs/design.md).
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
; flushed, so the drive is not written to.

bits 64
default rel

%include "kernel.inc"

extern sys_ioctl

EXTENTS_PER_CALL equ 256

section .bss
alignb 8
fm_buf:     resb fiemap_size + fiemap_extent_size * EXTENTS_PER_CALL
prev:       resb fiemap_extent_size     ; the extent before this one

section .text
global extents_count

; extents_count(edi = an open regular file, esi = the block size, not 0)
;   -> rax = the file's pieces, or -errno when FIEMAP fails; rdx = the
;   blocks its extents cover, each extent's bytes rounded up to a block.
; An inline-data file counts its one block. Clobbers rcx, rsi, rdi,
; r8 to r11.
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

section .note.GNU-stack noalloc noexec nowrite progbits
