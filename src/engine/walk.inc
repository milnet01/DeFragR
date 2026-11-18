; Walk: what walk_next returns in eax, and walk_start's modes (walk.asm). Included by walk.asm
; and by main.asm, its one caller.
%ifndef WALK_INC
%define WALK_INC

WALK_FILE           equ 0           ; edx = the open file; the caller closes it.
                                    ; ecx = its folder, open; the walk closes it
WALK_SKIPPED        equ 1           ; edx = errno, ecx = FILE_SKIPPED.stage
WALK_END            equ 2           ; every file has been visited
WALK_ERROR          equ 3           ; edx = errno; the top folder could not be read

; walk_start's edx: how walk_next opens each file.
WALK_TO_READ        equ 0           ; for reading only, as report needs
WALK_TO_MOVE        equ 1           ; for reading and writing, past the cache,
                                    ; as defrag's moves and checksums need

%endif
