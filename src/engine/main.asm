; Entry: the engine's _start. DFR-0003 replaces this with argument
; dispatch and the protocol's first and last records. Until then it
; exits 0, so the build and its checks have a real program to test.
%if __?NASM_VERSION_ID?__ < 0x03020000
%error "NASM 3.02 or later required"
%endif

bits 64
default rel

%define SYS_exit 60

section .text
global _start
_start:
    mov eax, SYS_exit
    xor edi, edi
    syscall

; Mark the stack non-executable; without this note ld links silently.
section .note.GNU-stack noalloc noexec nowrite progbits
