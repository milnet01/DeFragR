; Records: the engine's constants for the engine-window protocol.
; The one home of the layout is docs/specs/DFR-0002-engine-protocol.md;
; every type, code and offset here is written from it. The window's half
; is src/app/engine/Records.h, and selftest-records proves they agree.
%ifndef RECORDS_INC
%define RECORDS_INC

PROTOCOL_MAJOR      equ 1
PROTOCOL_MINOR      equ 3

; § 4.1 framing.
struc rec_header
    .type       resw 1
    .flags      resw 1
    .length     resd 1
endstruc
PAYLOAD_MAX         equ 65536
STRING_MAX          equ 4096
RECORD_ALIGN        equ 8

; Record types (§ 4.4 to § 4.6).
REC_HELLO           equ 0x0001
REC_FINISHED        equ 0x0002
REC_DRIVE           equ 0x0010
REC_FRAGMENTED_FILE equ 0x0020
REC_FILE_SKIPPED    equ 0x0021
REC_REPORT_SUMMARY  equ 0x0022
REC_REPORT_PROGRESS equ 0x0023
REC_MAP_RUNS        equ 0x0024
REC_RUN_STATE       equ 0x0030

; Fixed parts. Each size is a multiple of 8, so strings start aligned.
struc hello
    .major      resw 1
    .minor      resw 1
    .command    resd 1
    .block_size resd 1
    .reserved   resd 1
endstruc

struc finished
    .result     resd 1
    .errno      resd 1
endstruc

struc drive
    .total_blocks   resq 1
    .free_blocks    resq 1
    .block_size     resd 1
    .dev_major      resd 1
    .dev_minor      resd 1
    .media          resb 1
    .verdict        resb 1
    .reason         resw 1
    .mount_flags    resd 1
    .reserved       resd 1
endstruc

struc fragmented_file
    .extents        resd 1
    .reserved       resd 1
    .size_blocks    resq 1
endstruc

struc file_skipped
    .errno          resd 1
    .stage          resw 1
    .reserved       resw 1
endstruc

struc report_summary
    .files              resq 1
    .fragmented_files   resq 1
    .extents            resq 1
    .file_blocks        resq 1
    .skipped            resq 1
endstruc

struc report_progress
    .names          resq 1
    .estimate       resq 1
endstruc

struc map_runs
    .kind           resd 1
    .count          resd 1
endstruc                            ; count map_run entries follow
struc map_run
    .start          resq 1
    .length         resq 1
endstruc
MAP_RUNS_MAX        equ (PAYLOAD_MAX - map_runs_size) / map_run_size

struc run_state
    .state          resd 1
    .reserved       resd 1
endstruc

; HELLO.command (§ 4.4).
CMD_UNKNOWN         equ 0
CMD_PROBE           equ 1
CMD_REPORT          equ 2
CMD_SELFTEST_CRC32C equ 254
CMD_SELFTEST        equ 255

; DRIVE.media, DRIVE.verdict and DRIVE.mount_flags (§ 4.5).
MEDIA_UNKNOWN       equ 0
MEDIA_SSD           equ 1
MEDIA_SPINNING      equ 2
VERDICT_REPORT_ONLY equ 1
VERDICT_DEFRAG      equ 2
MOUNT_READ_ONLY     equ 1

; MAP_RUNS.kind (§ 4.6).
MAP_FREE            equ 1
MAP_FILESYSTEM      equ 2
MAP_WHOLE_FILE      equ 3
MAP_FRAGMENTED_FILE equ 4
MAP_KINDS           equ 4

; FILE_SKIPPED.stage (§ 4.6).
STAGE_OPEN          equ 1
STAGE_STAT          equ 2
STAGE_EXTENTS       equ 3
STAGE_READ_FOLDER   equ 4
STAGE_PATH_TOO_LONG equ 5

; Reason codes: DRIVE.reason (§ 4.7).
REASON_NONE         equ 0
REASON_FS_NOT_EXT4  equ 1
REASON_READ_ONLY    equ 2
REASON_SSD          equ 3
REASON_MEDIA_UNKNOWN equ 4

; Result codes: FINISHED.result and the exit status (§ 4.7).
RESULT_OK           equ 0
RESULT_USAGE        equ 1
RESULT_NOT_ROOT     equ 2
RESULT_NOT_A_MOUNT  equ 3
RESULT_MOUNTINFO    equ 4
RESULT_DRIVE_ERROR  equ 5
RESULT_REFUSED      equ 6
RESULT_CANCELLED    equ 7
RESULT_BAD_COMMAND  equ 8
RESULT_INTERNAL     equ 9

%endif
