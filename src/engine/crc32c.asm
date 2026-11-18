; Checksum: CRC32C (Castagnoli), the fingerprint the engine takes of a
; file before and after moving it (docs/design.md § Safety). Uses the
; crc32 instruction, which needs a CPU with SSE4.2; on one without, the
; instruction faults.
;
; selftest-crc32c checks it against published values
; (docs/specs/DFR-0002-engine-protocol.md § 4.11).

bits 64
default rel

%include "records.inc"

extern rec_hello
extern rec_finish

; One self-test case: crc32c_update(0, the length bytes at vector_bytes +
; offset) must give crc. An offset, not an address: the engine is
; linked with no absolute addresses in its data.
struc crc_vector
    .offset     resd 1
    .length     resd 1
    .crc        resd 1
    .reserved   resd 1
endstruc

section .rodata
; "123456789" is the standard check string; 0xE3069283 is CRC32C's check
; value. It starts one byte past an 8-byte boundary, so the 8-byte steps
; read unaligned.
align 8
vector_bytes:       db 0
check_string:       db "123456789"
check_string_len    equ $ - check_string
CHECK_CRC           equ 0xE3069283
CHECK_SPLIT         equ 5               ; where the chained test cuts it

; RFC 3720 (iSCSI) appendix B.4: four 32-byte buffers and a 48-byte read
; command, each with its CRC32C.
align 8
zeros_32:           times 32 db 0x00
ones_32:            times 32 db 0xFF
%assign i 0
rising_32:
%rep 32
                    db i
%assign i i + 1
%endrep
falling_32:
%rep 32
%assign i i - 1
                    db i
%endrep
read_pdu_48:        db 0x01, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                    db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                    db 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00
                    db 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x18
                    db 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                    db 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
read_pdu_48_len     equ $ - read_pdu_48

align 8
vectors:
    istruc crc_vector
        at crc_vector.offset, dd check_string - vector_bytes
        at crc_vector.length, dd 0
        at crc_vector.crc,    dd 0          ; no bytes: the start value
    iend
    istruc crc_vector
        at crc_vector.offset, dd check_string - vector_bytes
        at crc_vector.length, dd check_string_len
        at crc_vector.crc,    dd CHECK_CRC
    iend
    istruc crc_vector
        at crc_vector.offset, dd zeros_32 - vector_bytes
        at crc_vector.length, dd 32
        at crc_vector.crc,    dd 0x8A9136AA
    iend
    istruc crc_vector
        at crc_vector.offset, dd ones_32 - vector_bytes
        at crc_vector.length, dd 32
        at crc_vector.crc,    dd 0x62A8AB43
    iend
    istruc crc_vector
        at crc_vector.offset, dd rising_32 - vector_bytes
        at crc_vector.length, dd 32
        at crc_vector.crc,    dd 0x46DD794E
    iend
    istruc crc_vector
        at crc_vector.offset, dd falling_32 - vector_bytes
        at crc_vector.length, dd 32
        at crc_vector.crc,    dd 0x113FDB5C
    iend
    istruc crc_vector
        at crc_vector.offset, dd read_pdu_48 - vector_bytes
        at crc_vector.length, dd read_pdu_48_len
        at crc_vector.crc,    dd 0xD9963A56
    iend
vectors_end:

section .text
global crc32c_update
global crc32c_selftest

; crc32c_update(edi = CRC so far, rsi = bytes, rdx = how many)
;   -> eax = the CRC32C of everything so far.
; Start with 0. Pass each call the value the last one returned, and bytes
; given in pieces give the same CRC as one call over them all. Clobbers
; rsi and rdx; touches no memory but the bytes it reads.
crc32c_update:
    mov eax, edi
    not eax                         ; the running value is kept inverted
    cmp rdx, 8
    jb .bytes
.eights:
    crc32 rax, qword [rsi]
    add rsi, 8
    sub rdx, 8
    cmp rdx, 8
    jae .eights
.bytes:
    test rdx, rdx
    jz .done
.byte:
    crc32 eax, byte [rsi]
    inc rsi
    dec rdx
    jnz .byte
.done:
    not eax
    ret

; crc32c_selftest() -> does not return. The selftest-crc32c stream (§ 4.11):
; HELLO, then FINISHED with OK when crc32c_update gives every vector's
; published value, and the check string's in two pieces; INTERNAL when one
; differs.
crc32c_selftest:
    push rbx
    mov edi, CMD_SELFTEST_CRC32C
    xor esi, esi
    call rec_hello

    lea rbx, [vectors]
.vector:
    lea rsi, [vector_bytes]
    mov eax, [rbx + crc_vector.offset]
    add rsi, rax
    mov edx, [rbx + crc_vector.length]
    xor edi, edi
    call crc32c_update
    cmp eax, [rbx + crc_vector.crc]
    jne .wrong
    add rbx, crc_vector_size
    lea rax, [vectors_end]
    cmp rbx, rax
    jb .vector

    ; The check string in two pieces, the second continuing the first.
    xor edi, edi
    lea rsi, [check_string]
    mov edx, CHECK_SPLIT
    call crc32c_update
    mov edi, eax
    lea rsi, [check_string + CHECK_SPLIT]
    mov edx, check_string_len - CHECK_SPLIT
    call crc32c_update
    cmp eax, CHECK_CRC
    jne .wrong

    mov edi, RESULT_OK
    xor esi, esi
    call rec_finish
.wrong:
    mov edi, RESULT_INTERNAL
    xor esi, esi
    call rec_finish

section .note.GNU-stack noalloc noexec nowrite progbits
