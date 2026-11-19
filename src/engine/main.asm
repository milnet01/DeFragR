; Entry: the engine's _start. Reads the arguments, chooses the command,
; and ends every run with FINISHED and its result as the exit status
; (docs/specs/DFR-0002-engine-protocol.md § 4.3). Checks run in § 4.7's
; order: arguments, admin rights, mount point, drive rules, defrag's
; folder.

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
extern rec_skipped
extern rec_map_flush
extern rec_selftest
extern crc32c_selftest
extern probe_run
extern probe_find_mount
extern unescape
extern walk_start
extern walk_enter
extern walk_next
extern walk_path
extern walk_path_len
extern walk_names
extern walk_estimate
extern extents_count
extern extents_space
extern move_setup
extern move_file

section .rodata
arg_probe:      db "probe", 0
arg_report:     db "report", 0
arg_defrag:     db "defrag", 0
arg_map:        db "map", 0
arg_selftest:   db "selftest-records", 0
arg_selftest_crc32c: db "selftest-crc32c", 0

; A report sends REPORT_PROGRESS each time the walk has read this many
; more names. A cold report on /mnt/Games (2026-11-16: about 3.2 million
; used inodes, over ten minutes) reads a few thousand names a second, so
; there this is at most about one a second.
PROGRESS_EVERY  equ 4096

section .bss
alignb 8
summary:        resb report_summary_size   ; report's running totals
progress_due:   resq 1                  ; walk_names at the next REPORT_PROGRESS
mount_path:     resb STRING_MAX + 8     ; the mount point argument, decoded
folder_path:    resb STRING_MAX + 8     ; defrag's folder argument, decoded
folder_len:     resq 1                  ; its length, up to any \000
mount_arg:      resq 1                  ; defrag's mount point argument, as given
map_wanted:     resq 1                  ; not 0: defrag sends the drive's map first

section .text
global _start

; The kernel enters here with rsp a multiple of 16, argc at [rsp] and the
; argv pointers after it. Nothing returns from _start; every path ends in
; rec_finish, rec_selftest or crc32c_selftest, which exit.
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
    lea rsi, [arg_defrag]
    call str_equal
    test eax, eax
    jnz .defrag

    mov rdi, rbx
    lea rsi, [arg_selftest]
    call str_equal
    test eax, eax
    jnz .selftest

    mov rdi, rbx
    lea rsi, [arg_selftest_crc32c]
    call str_equal
    test eax, eax
    jnz .selftest_crc32c

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

.defrag:
    ; HELLO waits for the block size, so each failure here sends it.
    mov edi, RESULT_USAGE
    xor ebx, ebx                    ; ebx = 1 when the map is asked for
    cmp r12, 4
    je .defrag_counted
    cmp r12, 5
    jne .defrag_failed
    mov rdi, [r13 + 32]             ; argv[4]: "map", or it is no defrag
    lea rsi, [arg_map]
    call str_equal
    mov edi, RESULT_USAGE
    test eax, eax
    jz .defrag_failed
    mov ebx, 1
.defrag_counted:
    call sys_geteuid
    mov edi, RESULT_NOT_ROOT
    test eax, eax
    jnz .defrag_failed
    mov rdi, [r13 + 16]             ; argv[2], the mount point
    mov rsi, [r13 + 24]             ; argv[3], the folder
    mov edx, ebx
    call defrag_run
.defrag_failed:                     ; edi = result
    mov ebx, edi
    mov edi, CMD_DEFRAG
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

.selftest_crc32c:
    cmp r12, 2
    jne .selftest_crc32c_usage
    call crc32c_selftest
.selftest_crc32c_usage:
    mov edi, CMD_SELFTEST_CRC32C
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
; check on: HELLO with the block size; then, interleaved, a
; FRAGMENTED_FILE for each file in two or more pieces, a FILE_SKIPPED for
; each it could not measure, the map's MAP_RUNS and REPORT_PROGRESS; then
; REPORT_SUMMARY and FINISHED. The free and filesystem runs are gathered
; before any file and the files' runs during the walk; each kind's batch
; goes out when it fills, and the rest at the end. REPORT_PROGRESS comes
; straight after HELLO, every PROGRESS_EVERY names, and straight before
; REPORT_SUMMARY. A failure before the walk starts sends HELLO with block
; size 0 and FINISHED with its result.
report_run:
    sub rsp, 8
    mov esi, WALK_TO_READ
    call drive_open
    test eax, eax
    jnz .failed
    mov r15d, edx                   ; r15 = the block size
    mov r14d, ecx                   ; r14 = the top folder, open during the walk
    mov edi, CMD_REPORT
    mov esi, r15d
    call rec_hello
    call send_progress              ; names 0, first after HELLO
    ; The map's free and filesystem runs. A drive that cannot give them
    ; sends none, and the report goes on (DFR-0002 § 4.6).
    mov edi, r14d
    mov esi, r15d
    call extents_space

.next:
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
    xor edx, edx                    ; count only
    call extents_count
    mov r12, rax                    ; r12 = pieces, or -errno
    mov r13, rdx                    ; r13 = blocks
    cmp r12, -MAX_ERRNO
    jae .close
    ; Its map runs, now its pieces say which kind they are. A failure here
    ; skips the file as a failed count would; runs already sent stand
    ; (DFR-0002 § 4.6).
    mov edx, MAP_WHOLE_FILE
    mov eax, MAP_FRAGMENTED_FILE
    cmp r12, 2
    cmovae edx, eax
    mov edi, ebx
    mov esi, r15d
    call extents_count
    cmp rax, -MAX_ERRNO
    jb .close
    mov r12, rax
.close:
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
    inc qword [summary + report_summary.skipped]
    mov edi, edx
    mov esi, ecx
    lea rdx, [walk_path]
    mov rcx, [walk_path_len]
    call rec_skipped
    test rax, rax
    jnz .internal
    jmp .next

.end:
    call rec_map_flush
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

; drive_open(rdi = NUL-terminated mount point, escaped, esi = a WALK_TO_
;   mode) -> eax = OK, NOT_A_MOUNT, MOUNTINFO or DRIVE_ERROR, edx = errno
;   or 0; on OK, edx = the block size, ecx = the top folder, open, r8d =
;   the drive's reason (DFR-0002 § 4.7) and r9 = its mount id, with the
;   walk started there. The mount-point check of report and defrag.
; The argument is escaped as mountinfo writes it (DFR-0002 § 4.3), so a
; name that is not UTF-8 survives Qt's text arguments. The check and the
; walk both take the decoded path. A \000 ends it early, the same as
; being given the shorter path. Clobbers rax, rcx, rdx, rsi, rdi, r8 to
; r11.
drive_open:
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 8
    mov ebx, esi                    ; ebx = the walk's mode
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
    js .return                      ; longer than any mount point
    lea r14, [mount_path]           ; r14 = the mount point
    mov rdi, r14
    call probe_find_mount
    test eax, eax
    jnz .return
    mov r12, rcx                    ; r12 = its mount id
    mov r13d, r8d                   ; r13d = its reason
    mov rdi, r14
    mov rsi, r12
    mov edx, ebx
    call walk_start
    mov r8d, r13d
    mov r9, r12
.return:
    add rsp, 8
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; defrag_run(rdi = NUL-terminated mount point, rsi = NUL-terminated
;   folder, both escaped, edx = not 0 to send the drive's map first)
;   -> does not return. A failure before HELLO sends HELLO with block
;   size 0 and FINISHED with its result.
; The rest of a defrag stream (DFR-0002 § 4.12), from the mount-point
; check on: the drive rules, the folder, HELLO with the block size, the
; map when asked for, then each file's records from move_file, or a
; FILE_SKIPPED where the walk could not reach one, then FINISHED.
; The map needs a walk of the whole drive, so with it the folder is
; checked first, the drive walked to read, and then the mount point and
; the folder are opened again to move: the drive may have changed in
; between, and those checks then end the run after HELLO. The folder
; opened for the first check stays open until exit, which closes it.
defrag_run:
    push rbx
    mov [mount_arg], rdi
    mov [map_wanted], rdx
    mov rbx, rsi                    ; rbx = the folder
    mov esi, WALK_TO_MOVE
    call drive_open
    test eax, eax
    jnz .failed
    mov r15d, edx                   ; r15 = the block size
    mov r14, r9                     ; r14 = the mount id
    ; The top folder stays open until exit, which closes it.
    mov eax, RESULT_REFUSED
    xor edx, edx
    test r8d, r8d
    jnz .failed
    mov rdi, rbx
    mov rsi, rbx
.measure:
    cmp byte [rsi], 0
    je .measured
    inc rsi
    jmp .measure
.measured:
    sub rsi, rdi
    lea rdx, [folder_path]
    call unescape
    mov edx, EINVAL
    test rax, rax
    js .no_folder                   ; longer than any path
    lea rdi, [folder_path]
    mov rsi, rdi
.measure_decoded:                   ; a \000 ends it early
    cmp byte [rsi], 0
    je .measured_decoded
    inc rsi
    jmp .measure_decoded
.measured_decoded:
    sub rsi, rdi
    mov [folder_len], rsi
    call walk_enter
    test rax, rax
    jz .entered
    neg eax
    mov edx, eax
.no_folder:
    mov eax, RESULT_NO_FOLDER
    jmp .failed
.entered:
    cmp qword [map_wanted], 0
    jne .with_map
    mov edi, r15d
    mov rsi, r14
    call move_setup
    mov edi, CMD_DEFRAG
    mov esi, r15d
    call rec_hello
    jmp .next

.with_map:
    mov rdi, [mount_arg]
    mov esi, WALK_TO_READ
    call drive_open
    test eax, eax
    jnz .failed
    mov r15d, edx                   ; r15 = the block size
    mov r14d, ecx                   ; r14 = the top folder, open during the walk
    mov edi, CMD_DEFRAG
    mov esi, r15d
    call rec_hello
    call send_map
    ; Back to the folder, as at the start, now to move.
    mov rdi, [mount_arg]
    mov esi, WALK_TO_MOVE
    call drive_open
    test eax, eax
    jnz .finish_with
    mov r15d, edx
    mov r14, r9                     ; r14 = the mount id
    mov eax, RESULT_REFUSED
    xor edx, edx
    test r8d, r8d
    jnz .finish_with
    lea rdi, [folder_path]
    mov rsi, [folder_len]
    call walk_enter
    test rax, rax
    jz .moving
    neg eax
    mov edx, eax
    mov eax, RESULT_NO_FOLDER
.finish_with:                       ; eax = result, edx = errno; HELLO is sent
    mov edi, eax
    mov esi, edx
    call rec_finish
.moving:
    mov edi, r15d
    mov rsi, r14
    call move_setup

.next:
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
    mov edi, edx
    mov esi, ecx                    ; its folder
    lea rdx, [walk_path]
    mov rcx, [walk_path_len]
    call move_file
    mov r12d, eax                   ; r12d = OK, or the run's result
    mov edi, ebx
    call sys_close                  ; its data is synced; a close adds nothing
    test r12d, r12d
    jz .next
    mov edi, r12d
    xor esi, esi
    call rec_finish
.skipped:                           ; edx = errno, ecx = stage
    mov edi, edx
    mov esi, ecx
    lea rdx, [walk_path]
    mov rcx, [walk_path_len]
    call rec_skipped
    test rax, rax
    jz .next
    mov edi, RESULT_INTERNAL        ; a path did not fit its record
    xor esi, esi
    call rec_finish
.end:
    mov edi, RESULT_OK
    xor esi, esi
    call rec_finish

.failed:                            ; eax = result, edx = errno; no HELLO yet
    mov ebx, eax
    mov r12d, edx
    mov edi, CMD_DEFRAG
    xor esi, esi
    call rec_hello
    mov edi, ebx
    mov esi, r12d
    call rec_finish

; send_map(r14d = the top folder, open, r15d = the block size), with the
;   walk started at the top folder to read. Returns once the map is sent.
; The drive's map at the head of a defrag stream (DFR-0002 § 4.12):
; REPORT_PROGRESS and MAP_RUNS as a report sends them (§ 4.6), then
; MAP_DONE. A file or folder it cannot read sends nothing, so its blocks
; are in no run. The top folder failing to read ends the run with
; DRIVE_ERROR. Clobbers rax, rbx, rcx, rdx, rsi, rdi, r8 to r11.
send_map:
    sub rsp, 8
    call send_progress              ; names 0, first after HELLO
    mov edi, r14d
    mov esi, r15d
    call extents_space
.next:
    mov rax, [walk_names]
    cmp rax, [progress_due]
    jb .walk
    call send_progress
.walk:
    call walk_next
    cmp eax, WALK_FILE
    je .file
    cmp eax, WALK_SKIPPED
    je .next
    cmp eax, WALK_END
    je .end
    mov edi, RESULT_DRIVE_ERROR     ; WALK_ERROR: the top folder failed
    mov esi, edx
    call rec_finish
.file:
    mov ebx, edx                    ; ebx = the file
    mov edi, ebx
    mov esi, r15d
    xor edx, edx                    ; count only
    call extents_count
    cmp rax, -MAX_ERRNO
    jae .close
    ; Its runs, now its pieces say which kind they are.
    mov edx, MAP_WHOLE_FILE
    mov ecx, MAP_FRAGMENTED_FILE
    cmp rax, 2
    cmovae edx, ecx
    mov edi, ebx
    mov esi, r15d
    call extents_count
.close:
    mov edi, ebx
    call sys_close                  ; read-only; a failed close loses nothing
    jmp .next
.end:
    call rec_map_flush
    call send_progress              ; every name, for the last time
    mov edi, REC_MAP_DONE
    mov esi, map_done_size
    call rec_start
    call rec_send
    add rsp, 8
    ret

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
