; Walk: what walk_next returns in eax (walk.asm). Included by walk.asm
; and by main.asm, its one caller.
%ifndef WALK_INC
%define WALK_INC

WALK_FILE           equ 0           ; edx = the open file; the caller closes it
WALK_SKIPPED        equ 1           ; edx = errno, ecx = FILE_SKIPPED.stage
WALK_END            equ 2           ; every file has been visited
WALK_ERROR          equ 3           ; edx = errno; the top folder could not be read

%endif
