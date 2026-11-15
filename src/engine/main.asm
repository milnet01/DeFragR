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

extern sys_geteuid
extern sys_ignore_sigpipe
extern rec_hello
extern rec_finish
extern rec_selftest
extern probe_run

section .rodata
arg_probe:      db "probe", 0
arg_report:     db "report", 0
arg_selftest:   db "selftest-records", 0

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
    mov edi, CMD_REPORT
    xor esi, esi
    call rec_hello
    mov edi, RESULT_USAGE
    cmp r12, 3
    jne .finish
    call sys_geteuid
    mov edi, RESULT_NOT_ROOT
    test eax, eax
    jnz .finish
    ; The mount-point check and the walk are DFR-0008's; until then a
    ; report cannot finish OK.
    mov edi, RESULT_INTERNAL
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
