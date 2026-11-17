; Entry: the engine's _start. Reads the arguments, chooses the command,
; and ends every run with FINISHED and its result as the exit status
; (docs/specs/DFR-0002-engine-protocol.md § 4.3). Checks run in § 4.7's
; order: arguments, admin rights, mount point, drive rules.
%if __?NASM_VERSION_ID?__ < 0x03020000
%error "NASM 3.02 or later required"
%endif

bits 64
default rel

%include "kernel.inc"
%include "records.inc"
%include "walk.inc"

extern sys_geteuid
extern sys_close
extern sys_ignore_sigpipe
extern rec_start
extern rec_string
extern rec_send
extern rec_hello
extern rec_finish
extern rec_selftest
extern probe_run
extern probe_find_mount
extern unescape
extern walk_start
extern walk_next
extern walk_path
extern walk_path_len
extern walk_names
extern walk_estimate
extern extents_count

section .rodata
arg_probe:      db "probe", 0
arg_report:     db "report", 0
arg_selftest:   db "selftest-records", 0

; A report sends REPORT_PROGRESS each time the walk has read this many
; more names. A cold report on /mnt/Games (2026-11-16: about 3.2 million
; used inodes, over ten minutes) reads a few thousand names a second, so
; there this is at most about one a second.
PROGRESS_EVERY  equ 4096

section .bss
alignb 8
summary:        resb report_summary_size   ; report's running totals
progress_due:   resq 1                  ; walk_names at the next REPORT_PROGRESS
mount_path:     resb STRING_MAX + 8     ; report's argument, decoded

section .text
global _start

; The kernel enters here with rsp a multiple of 16, argc at [rsp] and the
; argv pointers after it. Nothing returns from _start; every path ends in
; rec_finish or rec_selftest, which exit.
_start:
    mov r12, [rsp]                  ; argc
    lea r13, [rsp + 8]              ; argv

    ; Before any write: a closed pipe must give EPIPE, not kill us.
    call sys_ignore_sigpipe
    cmp rax, -MAX_ERRNO
    jae .setup_failed

    cmp r12, 2
    jb .unknown
    mov rbx, [r13 + 8]              ; argv[1]

    mov rdi, rbx
    lea rsi, [arg_probe]
    call str_equal
    test eax, eax
    jnz .probe

    mov rdi, rbx
    lea rsi, [arg_report]
    call str_equal
    test eax, eax
    jnz .report

    mov rdi, rbx
    lea rsi, [arg_selftest]
    call str_equal
    test eax, eax
    jnz .selftest

.unknown:
    mov edi, CMD_UNKNOWN
    xor esi, esi
    call rec_hello
    mov edi, RESULT_USAGE
    jmp .finish

.probe:
    mov edi, CMD_PROBE
    xor esi, esi
    call rec_hello
    mov edi, RESULT_USAGE
    cmp r12, 2
    jne .finish
    call probe_run
    mov edi, eax
    mov esi, edx
    call rec_finish

.report:
    ; HELLO waits for the block size, so each failure here sends it.
    mov edi, RESULT_USAGE
    cmp r12, 3
    jne .report_failed
    call sys_geteuid
    mov edi, RESULT_NOT_ROOT
    test eax, eax
    jnz .report_failed
    mov rdi, [r13 + 16]             ; argv[2], the mount point
    call report_run
.report_failed:                     ; edi = result
    mov ebx, edi
    mov edi, CMD_REPORT
    xor esi, esi
    call rec_hello
    mov edi, ebx
    jmp .finish

.selftest:
    cmp r12, 2
    jne .selftest_usage
    call rec_selftest
.selftest_usage:
    mov edi, CMD_SELFTEST
    xor esi, esi
    call rec_hello
    mov edi, RESULT_USAGE
    jmp .finish

.setup_failed:
    neg eax
    mov ebx, eax                    ; errno
    mov edi, CMD_UNKNOWN
    xor esi, esi
    call rec_hello
    mov edi, RESULT_INTERNAL
    mov esi, ebx
    call rec_finish

.finish:                            ; edi = result
    xor esi, esi
    call rec_finish

; report_run(rdi = NUL-terminated mount point, escaped) -> does not return.
; The rest of a report stream (DFR-0002 § 4.6), from the mount-point
; check on: HELLO with the block size, a FRAGMENTED_FILE for each file in
; two or more pieces, a FILE_SKIPPED for each it could not measure, then
; REPORT_SUMMARY and FINISHED. REPORT_PROGRESS comes straight after HELLO,
; every PROGRESS_EVERY names, and straight before REPORT_SUMMARY. A
; failure before the walk starts sends HELLO with block size 0 and
; FINISHED with its result.
report_run:
    sub rsp, 8
    ; The argument is escaped as mountinfo writes it (DFR-0002 § 4.3), so
    ; a name that is not UTF-8 survives Qt's text arguments.
    ; The check and the walk both take the decoded path. A \000 ends it
    ; early, the same as being given the shorter path.
    mov rsi, rdi
.measure:
    cmp byte [rsi], 0
    je .measured
    inc rsi
    jmp .measure
.measured:
    sub rsi, rdi                    ; rsi = the argument's length
    lea rdx, [mount_path]
    call unescape
    test rax, rax
    mov eax, RESULT_NOT_A_MOUNT     ; mov leaves the flags alone
    mov edx, 0
    js .failed                      ; longer than any mount point
    lea r14, [mount_path]           ; r14 = the mount point
    mov rdi, r14
    call probe_find_mount
    test eax, eax
    jnz .failed
    mov rdi, r14
    mov rsi, rcx                    ; its mount id
    call walk_start
    test eax, eax
    jnz .failed
    mov r15d, edx                   ; r15 = the block size
    mov edi, CMD_REPORT
    mov esi, r15d
    call rec_hello

.next:                              ; the first pass sends names 0
    mov rax, [walk_names]
    cmp rax, [progress_due]
    jb .walk
    call send_progress
.walk:
    call walk_next
    cmp eax, WALK_FILE
    je .file
    cmp eax, WALK_SKIPPED
    je .skipped
    cmp eax, WALK_END
    je .end
    mov edi, RESULT_DRIVE_ERROR     ; WALK_ERROR: the top folder failed
    mov esi, edx
    call rec_finish

.file:
    mov ebx, edx                    ; ebx = the file
    mov edi, ebx
    mov esi, r15d
    call extents_count
    mov r12, rax                    ; r12 = pieces, or -errno
    mov r13, rdx                    ; r13 = blocks
    mov edi, ebx
    call sys_close                  ; read-only; a failed close loses nothing
    cmp r12, -MAX_ERRNO
    jae .unmeasured
    inc qword [summary + report_summary.files]
    add [summary + report_summary.extents], r12
    add [summary + report_summary.file_blocks], r13
    cmp r12, 2
    jb .next
    inc qword [summary + report_summary.fragmented_files]
    mov edi, REC_FRAGMENTED_FILE
    mov esi, fragmented_file_size
    call rec_start
    mov ecx, 0xFFFFFFFF
    cmp r12, rcx
    cmova r12, rcx                  ; the field is 32 bits
    mov [rax + fragmented_file.extents], r12d
    mov [rax + fragmented_file.size_blocks], r13
    lea rdi, [walk_path]
    mov rsi, [walk_path_len]
    call rec_string
    test rax, rax
    jnz .internal
    call rec_send
    jmp .next

.unmeasured:
    neg r12
    mov edx, r12d
    mov ecx, STAGE_EXTENTS
.skipped:                           ; edx = errno, ecx = stage
    mov ebx, edx
    mov r12d, ecx
    inc qword [summary + report_summary.skipped]
    mov edi, REC_FILE_SKIPPED
    mov esi, file_skipped_size
    call rec_start
    mov [rax + file_skipped.errno], ebx
    mov [rax + file_skipped.stage], r12w
    lea rdi, [walk_path]
    mov rsi, [walk_path_len]
    call rec_string
    test rax, rax
    jnz .internal
    call rec_send
    jmp .next

.end:
    call send_progress              ; every name, for the last time
    mov edi, REC_REPORT_SUMMARY
    mov esi, report_summary_size
    call rec_start
    mov rdi, rax
    lea rsi, [summary]
    mov ecx, report_summary_size
    rep movsb
    call rec_send
    mov edi, RESULT_OK
    xor esi, esi
    call rec_finish

.failed:                            ; eax = result, edx = errno; no HELLO yet
    mov ebx, eax
    mov r12d, edx
    mov edi, CMD_REPORT
    xor esi, esi
    call rec_hello
    mov edi, ebx
    mov esi, r12d
    call rec_finish
.internal:                          ; a path did not fit its record
    mov edi, RESULT_INTERNAL
    xor esi, esi
    call rec_finish

; send_progress() -> rax = 0. Sends REPORT_PROGRESS with the walk's
; counts, and makes the next due PROGRESS_EVERY names on. Clobbers as
; rec_send.
send_progress:
    sub rsp, 8
    mov edi, REC_REPORT_PROGRESS
    mov esi, report_progress_size
    call rec_start
    mov rcx, [walk_names]
    mov [rax + report_progress.names], rcx
    add rcx, PROGRESS_EVERY
    mov [progress_due], rcx
    mov rcx, [walk_estimate]
    mov [rax + report_progress.estimate], rcx
    call rec_send
    add rsp, 8
    ret

; str_equal(rdi = NUL-terminated string, rsi = NUL-terminated string)
;   -> eax = 1 when equal, else 0. Clobbers rcx, rdx, rdi, rsi.
str_equal:
.next:
    movzx ecx, byte [rdi]
    movzx edx, byte [rsi]
    cmp ecx, edx
    jne .differ
    test ecx, ecx
    jz .same
    inc rdi
    inc rsi
    jmp .next
.same:
    mov eax, 1
    ret
.differ:
    xor eax, eax
    ret

; Mark the stack non-executable; without this note ld links silently.
section .note.GNU-stack noalloc noexec nowrite progbits
