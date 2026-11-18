; Extents: the run extents_map writes (extents.asm). Included by
; extents.asm and by move.asm, its one caller.
%ifndef EXTENTS_INC
%define EXTENTS_INC

; Where a run of a file's blocks lies, in blocks.
struc file_run
    .logical    resq 1              ; the file's block
    .physical   resq 1              ; the drive's block
    .length     resq 1
endstruc

%endif
