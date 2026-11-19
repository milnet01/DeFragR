; Records: encoding outgoing records, the engine's half of the protocol
; (docs/specs/DFR-0002-engine-protocol.md). One record is built at a time
; in rec_buf, then written whole to standard output.
;
; Building a record: rec_start, then store fields at the payload pointer
; it returns, using records.inc's struc offsets; then rec_string for each
; string in order; then rec_send.
;
; MAP_RUNS is not built that way. rec_map_run gathers runs, one batch per
; kind, and sends a batch when it is full; rec_map_flush sends the rest.

bits 64
default rel

%include "kernel.inc"
%include "records.inc"

extern sys_write
extern sys_exit

REC_BUF_SIZE equ rec_header_size + PAYLOAD_MAX
; One kind's batch: a MAP_RUNS payload, its count the runs gathered so far.
MAP_BATCH_SIZE equ map_runs_size + MAP_RUNS_MAX * map_run_size

section .bss
alignb 8
rec_buf:    resb REC_BUF_SIZE
rec_end:    resq 1              ; bytes used in rec_buf, header included
map_batches: resb MAP_KINDS * MAP_BATCH_SIZE    ; kind 1's first

section .rodata
; Self-test (st_) strings: the values DFR-0002 § 4.9 fixes, odd bytes
; included. The 0xFF is a byte that is not UTF-8, not a lost end marker.
st_mount_point:     db "/mnt/t", 0xFF
st_mount_point_len  equ $ - st_mount_point
st_fs_type:         db "ext4"
st_fs_type_len      equ $ - st_fs_type
st_path_frag:       db "a/b"
st_path_frag_len    equ $ - st_path_frag
st_path_skip:       db "c"
st_path_skip_len    equ $ - st_path_skip
st_path_defragged:  db "d/e"
st_path_defragged_len equ $ - st_path_defragged

section .text
global rec_start
global rec_string
global rec_send
global rec_hello
global rec_finish
global rec_skipped
global rec_map_run
global rec_map_flush
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
; EAGAIN is not retried: the window's QProcess gives the engine a blocking
; pipe (its fd 1 has no O_NONBLOCK, measured 2026-11-18, DFR-0053), so
; only a caller that made it non-blocking can see it.
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

; rec_skipped(edi = errno, esi = stage, rdx = path, rcx = its length)
;   -> rax = 0, or -1 when the path does not fit a record string; then
;   nothing is sent. Sends FILE_SKIPPED (§ 4.6, § 4.12). Clobbers as
;   rec_send.
rec_skipped:
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 8
    mov ebx, edi
    mov r12d, esi
    mov r13, rdx
    mov r14, rcx
    mov edi, REC_FILE_SKIPPED
    mov esi, file_skipped_size
    call rec_start
    mov [rax + file_skipped.errno], ebx
    mov [rax + file_skipped.stage], r12w
    mov rdi, r13
    mov rsi, r14
    call rec_string
    test rax, rax
    jnz .return                     ; -1
    call rec_send
.return:
    add rsp, 8
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; rec_map_run(edi = a MAP_ kind, 1 to MAP_KINDS, rsi = first block,
;   rdx = blocks) -> rax = 0. Adds the run to its kind's batch, joined onto
;   the batch's last run when it starts where that one ends. A run of 0
;   blocks is dropped. Sends the batch first when it is full. Clobbers as
;   rec_send.
rec_map_run:
    push rbx
    push r12
    push r13
    test rdx, rdx
    jz .done
    mov r12, rsi                    ; r12 = first block
    mov r13, rdx                    ; r13 = blocks
    lea eax, [rdi - 1]
    imul rbx, rax, MAP_BATCH_SIZE
    lea rcx, [map_batches]
    add rbx, rcx                    ; rbx = this kind's batch
    mov [rbx + map_runs.kind], edi
    mov ecx, [rbx + map_runs.count]
    test ecx, ecx
    jz .append
    lea eax, [rcx - 1]
    imul eax, eax, map_run_size
    lea rax, [rbx + map_runs_size + rax]    ; rax = the last run
    mov rdx, [rax + map_run.start]
    add rdx, [rax + map_run.length]
    cmp rdx, r12
    jne .room
    add [rax + map_run.length], r13
    jmp .done
.room:
    cmp ecx, MAP_RUNS_MAX
    jb .append
    mov rdi, rbx
    call send_batch
    xor ecx, ecx
.append:                            ; ecx = runs in the batch
    imul ecx, ecx, map_run_size
    lea rax, [rbx + map_runs_size + rcx]
    mov [rax + map_run.start], r12
    mov [rax + map_run.length], r13
    inc dword [rbx + map_runs.count]
.done:
    xor eax, eax
    pop r13
    pop r12
    pop rbx
    ret

; rec_map_flush() -> rax = 0. Sends every batch holding a run. Clobbers
; as rec_send.
rec_map_flush:
    push rbx
    push r12
    sub rsp, 8
    lea rbx, [map_batches]
    mov r12d, MAP_KINDS
.batch:
    cmp dword [rbx + map_runs.count], 0
    je .empty
    mov rdi, rbx
    call send_batch
.empty:
    add rbx, MAP_BATCH_SIZE
    dec r12d
    jnz .batch
    xor eax, eax
    add rsp, 8
    pop r12
    pop rbx
    ret

; send_batch(rdi = a batch holding a run) -> rax = 0. Sends it as one
; MAP_RUNS and empties it. Clobbers as rec_send.
send_batch:
    push rbx
    mov rbx, rdi
    mov esi, [rbx + map_runs.count]
    imul esi, esi, map_run_size
    add esi, map_runs_size          ; esi = payload size, 8-aligned for rec_start
    mov edi, REC_MAP_RUNS
    call rec_start                  ; keeps rsi
    mov ecx, esi                    ; the whole payload, from the batch
    mov rdi, rax
    mov rsi, rbx
    rep movsb
    call rec_send
    mov dword [rbx + map_runs.count], 0
    pop rbx
    ret

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
    mov dword [rax + drive.mount_flags], 0xE1E2E3E5  ; E5, as § 4.9 fixes
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

    mov edi, 0x0A0B0C0D
    mov esi, 0x0E0F
    lea rdx, [st_path_skip]
    mov ecx, st_path_skip_len
    call rec_skipped
    test rax, rax
    jnz .internal

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

    mov edi, REC_REPORT_PROGRESS
    mov esi, report_progress_size
    call rec_start
    mov rcx, 0xE1E2E3E4E5E6E7E8
    mov [rax + report_progress.names], rcx
    mov rcx, 0xF1F2F3F4F5F6F7F8
    mov [rax + report_progress.estimate], rcx
    call rec_send

    mov edi, REC_MAP_RUNS
    mov esi, map_runs_size + 2 * map_run_size
    call rec_start
    mov dword [rax + map_runs.kind], 0x13141516
    mov dword [rax + map_runs.count], 2
    mov rcx, 0x3132333435363738
    mov [rax + map_runs_size + map_run.start], rcx
    mov rcx, 0x4142434445464748
    mov [rax + map_runs_size + map_run.length], rcx
    mov rcx, 0x5152535455565758
    mov [rax + map_runs_size + map_run_size + map_run.start], rcx
    mov rcx, 0x6162636465666768
    mov [rax + map_runs_size + map_run_size + map_run.length], rcx
    call rec_send

    mov edi, REC_RUN_STATE
    mov esi, run_state_size
    call rec_start
    mov dword [rax + run_state.state], 0xF1F2F3F4
    call rec_send

    mov edi, REC_FILE_DEFRAGGED
    mov esi, file_defragged_size
    call rec_start
    mov dword [rax + file_defragged.extents_before], 0x21222324
    mov dword [rax + file_defragged.extents_after], 0x25262728
    mov rcx, 0x2A2B2C2D2E2F3031
    mov [rax + file_defragged.moved_blocks], rcx
    lea rdi, [st_path_defragged]
    mov esi, st_path_defragged_len
    call rec_string
    test rax, rax
    jnz .internal
    call rec_send

    mov edi, REC_BLOCKS_MOVED
    mov esi, blocks_moved_size + 3 * map_run_size
    call rec_start
    mov dword [rax + blocks_moved.from_count], 1
    mov dword [rax + blocks_moved.to_count], 2
    lea rdx, [rax + blocks_moved_size]
    mov rcx, 0x7172737475767778
    mov [rdx + map_run.start], rcx
    mov rcx, 0x8182838485868788
    mov [rdx + map_run.length], rcx
    add rdx, map_run_size
    mov rcx, 0x9192939495969798
    mov [rdx + map_run.start], rcx
    mov rcx, 0xA1A2A3A4A5A6A7A8
    mov [rdx + map_run.length], rcx
    add rdx, map_run_size
    mov rcx, 0xB1B2B3B4B5B6B7B8
    mov [rdx + map_run.start], rcx
    mov rcx, 0xC1C2C3C4C5C6C7C8
    mov [rdx + map_run.length], rcx
    call rec_send

    mov edi, REC_MAP_DONE
    mov esi, map_done_size
    call rec_start
    call rec_send

    mov edi, REC_DEFRAG_PROGRESS
    mov esi, defrag_progress_size
    call rec_start
    mov rcx, 0xD1D2D3D4D5D6D7D8
    mov [rax + defrag_progress.files_done], rcx
    mov rcx, 0xD9DADBDCDDDEDFE0
    mov [rax + defrag_progress.files_total], rcx
    mov rcx, 0xC9CACBCCCDCECFD0
    mov [rax + defrag_progress.blocks_done], rcx
    mov rcx, 0xB9BABBBCBDBEBFC0
    mov [rax + defrag_progress.blocks_total], rcx
    call rec_send

    mov edi, RESULT_OK
    xor esi, esi
    call rec_finish
.internal:                          ; a fixed string did not fit
    mov edi, RESULT_INTERNAL
    xor esi, esi
    call rec_finish

section .note.GNU-stack noalloc noexec nowrite progbits
