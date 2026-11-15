; Records: encoding outgoing records, the engine's half of the protocol
; (docs/specs/DFR-0002-engine-protocol.md). One record is built at a time
; in rec_buf, then written whole to standard output.
;
; Building a record: rec_start, then store fields at the payload pointer
; it returns, using records.inc's struc offsets; then rec_string for each
; string in order; then rec_send.
%if __?NASM_VERSION_ID?__ < 0x03020000
%error "NASM 3.02 or later required"
%endif

bits 64
default rel

%include "kernel.inc"
%include "records.inc"

extern sys_write
extern sys_exit

REC_BUF_SIZE equ rec_header_size + PAYLOAD_MAX

section .bss
alignb 8
rec_buf:    resb REC_BUF_SIZE
rec_end:    resq 1              ; bytes used in rec_buf, header included

section .rodata
st_mount_point:     db "/mnt/t", 0xFF
st_mount_point_len  equ $ - st_mount_point
st_fs_type:         db "ext4"
st_fs_type_len      equ $ - st_fs_type
st_path_frag:       db "a/b"
st_path_frag_len    equ $ - st_path_frag
st_path_skip:       db "c"
st_path_skip_len    equ $ - st_path_skip

section .text
global rec_start
global rec_string
global rec_send
global rec_hello
global rec_finish
global rec_selftest

; rec_start(edi = record type, esi = fixed-part size, a multiple of 8)
;   -> rax = pointer to the zeroed fixed part.
; Starts a new record, discarding any unsent one. Clobbers rcx, rdx, rdi.
rec_start:
    mov edx, edi
    lea rdi, [rec_buf]
    lea ecx, [rsi + rec_header_size]
    mov [rec_end], rcx
    xor eax, eax
    rep stosb
    lea rax, [rec_buf]
    mov [rax + rec_header.type], dx
    add rax, rec_header_size
    ret

; rec_string(rdi = bytes, rsi = byte count) -> rax = 0, or -1 when the
; count exceeds STRING_MAX or the string would not fit in the payload.
; Appends a u32 count, the bytes, and zeros up to the next multiple of 8
; (§ 4.1). On -1 the record is unchanged. Clobbers rcx, rdx, rdi, rsi, r8.
rec_string:
    cmp rsi, STRING_MAX
    ja .too_long
    lea rdx, [rsi + 4 + RECORD_ALIGN - 1]
    and rdx, -RECORD_ALIGN          ; rdx = count field + bytes + padding
    mov r8, [rec_end]
    lea rax, [r8 + rdx]
    cmp rax, REC_BUF_SIZE
    ja .too_long
    mov [rec_end], rax
    lea rax, [rec_buf]
    add r8, rax                     ; r8 = where the string goes
    mov [r8], esi
    mov rcx, rsi
    mov rax, rsi                    ; keep the count for the padding
    mov rsi, rdi
    lea rdi, [r8 + 4]
    rep movsb                       ; rdi now ends the bytes
    lea rcx, [rdx - 4]
    sub rcx, rax                    ; padding bytes
    xor eax, eax
    rep stosb
    ret
.too_long:
    mov rax, -1
    ret

; rec_send() -> rax = 0. Sets the header's length and writes the record
; whole, repeating partial writes and retrying on EINTR (§ 6). Does not
; return when a write fails, since no FINISHED could follow: exits with
; CANCELLED on EPIPE (the window has gone), INTERNAL on any other error.
; Clobbers rax, rcx, rdx, rdi, rsi, r8 to r11.
rec_send:
    push r12
    push r13
    sub rsp, 8
    lea r12, [rec_buf]
    mov r13, [rec_end]
    lea rax, [r13 - rec_header_size]
    mov [r12 + rec_header.length], eax
.write:
    test r13, r13
    jz .done
    mov edi, STDOUT_FD
    mov rsi, r12
    mov rdx, r13
    call sys_write
    cmp rax, -MAX_ERRNO
    jae .failed
    test rax, rax
    jz .broken                      ; no progress on a non-empty write
    add r12, rax
    sub r13, rax
    jmp .write
.failed:
    cmp rax, -EINTR
    je .write
    cmp rax, -EPIPE
    jne .broken
    mov edi, RESULT_CANCELLED
    call sys_exit
.broken:
    mov edi, RESULT_INTERNAL
    call sys_exit
.done:
    xor eax, eax
    add rsp, 8
    pop r13
    pop r12
    ret

; rec_hello(edi = HELLO.command, esi = HELLO.block_size) -> rax = 0.
; Sends the HELLO record (§ 4.4). Clobbers as rec_send.
rec_hello:
    sub rsp, 8
    mov r8d, edi
    mov r9d, esi
    mov edi, REC_HELLO
    mov esi, hello_size
    call rec_start
    mov word [rax + hello.major], PROTOCOL_MAJOR
    mov word [rax + hello.minor], PROTOCOL_MINOR
    mov [rax + hello.command], r8d
    mov [rax + hello.block_size], r9d
    call rec_send
    add rsp, 8
    ret

; rec_finish(edi = result code, esi = errno or 0) -> does not return.
; Sends FINISHED (§ 4.4) and exits with the result as the exit status.
rec_finish:
    sub rsp, 8
    mov ebx, edi                    ; never returns, so rbx need not survive
    mov r12d, esi
    mov edi, REC_FINISHED
    mov esi, finished_size
    call rec_start
    mov [rax + finished.result], ebx
    mov [rax + finished.errno], r12d
    call rec_send
    mov edi, ebx
    call sys_exit

; rec_selftest() -> does not return. The whole selftest-records stream:
; one record of every engine type with the values § 4.9 fixes, then exits 0.
rec_selftest:
    push rbx
    mov edi, CMD_SELFTEST
    mov esi, 0x01020304
    call rec_hello

    mov edi, REC_DRIVE
    mov esi, drive_size
    call rec_start
    mov rcx, 0x1112131415161718
    mov [rax + drive.total_blocks], rcx
    mov rcx, 0x2122232425262728
    mov [rax + drive.free_blocks], rcx
    mov dword [rax + drive.block_size], 0x31323334
    mov dword [rax + drive.dev_major], 0x41424344
    mov dword [rax + drive.dev_minor], 0x51525354
    mov byte [rax + drive.media], MEDIA_SPINNING
    mov byte [rax + drive.verdict], VERDICT_REPORT_ONLY
    mov word [rax + drive.reason], 0xD1D2
    mov dword [rax + drive.mount_flags], 0xE1E2E3E5
    lea rdi, [st_mount_point]
    mov esi, st_mount_point_len
    call rec_string
    test rax, rax
    jnz .internal
    xor edi, edi                    ; source: empty
    xor esi, esi
    call rec_string
    test rax, rax
    jnz .internal
    lea rdi, [st_fs_type]
    mov esi, st_fs_type_len
    call rec_string
    test rax, rax
    jnz .internal
    call rec_send

    mov edi, REC_FRAGMENTED_FILE
    mov esi, fragmented_file_size
    call rec_start
    mov dword [rax + fragmented_file.extents], 0x61626364
    mov rcx, 0x7172737475767778
    mov [rax + fragmented_file.size_blocks], rcx
    lea rdi, [st_path_frag]
    mov esi, st_path_frag_len
    call rec_string
    test rax, rax
    jnz .internal
    call rec_send

    mov edi, REC_FILE_SKIPPED
    mov esi, file_skipped_size
    call rec_start
    mov dword [rax + file_skipped.errno], 0x0A0B0C0D
    mov word [rax + file_skipped.stage], 0x0E0F
    lea rdi, [st_path_skip]
    mov esi, st_path_skip_len
    call rec_string
    test rax, rax
    jnz .internal
    call rec_send

    mov edi, REC_REPORT_SUMMARY
    mov esi, report_summary_size
    call rec_start
    mov rbx, rax
    mov rax, 0x8182838485868788
    mov [rbx + report_summary.files], rax
    mov rax, 0x9192939495969798
    mov [rbx + report_summary.fragmented_files], rax
    mov rax, 0xA1A2A3A4A5A6A7A8
    mov [rbx + report_summary.extents], rax
    mov rax, 0xB1B2B3B4B5B6B7B8
    mov [rbx + report_summary.file_blocks], rax
    mov rax, 0xC1C2C3C4C5C6C7C8
    mov [rbx + report_summary.skipped], rax
    call rec_send

    mov edi, REC_RUN_STATE
    mov esi, run_state_size
    call rec_start
    mov dword [rax + run_state.state], 0xF1F2F3F4
    call rec_send

    mov edi, RESULT_OK
    xor esi, esi
    call rec_finish
.internal:                          ; a fixed string did not fit
    mov edi, RESULT_INTERNAL
    xor esi, esi
    call rec_finish

section .note.GNU-stack noalloc noexec nowrite progbits
