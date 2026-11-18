# The engine: x86-64 NASM, no C library, statically linked.
# It depends on the kernel and nothing else (docs/design.md).
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

# NASM 3.02 is the floor (the assembly standard, languages/asm.md). This
# file enforces it: configure stops on an older assembler. README.md,
# scripts/local-ci.sh and .github/workflows/ci.yml repeat the number;
# change them with it.
set(DEFRAGR_NASM_MIN 3.02)
execute_process(COMMAND ${CMAKE_ASM_NASM_COMPILER} -v
    OUTPUT_VARIABLE _nasm_v RESULT_VARIABLE _nasm_rc)
string(REGEX MATCH "version ([0-9]+\\.[0-9]+(\\.[0-9]+)?)" _ "${_nasm_v}")
if(NOT _nasm_rc EQUAL 0 OR NOT CMAKE_MATCH_1)
    message(FATAL_ERROR "Could not read the NASM version from ${CMAKE_ASM_NASM_COMPILER}")
elseif(CMAKE_MATCH_1 VERSION_LESS DEFRAGR_NASM_MIN)
    message(FATAL_ERROR "NASM ${DEFRAGR_NASM_MIN} or later required; found ${CMAKE_MATCH_1}")
endif()

add_executable(defragr-engine
    extents.asm
    main.asm
    probe.asm
    records.asm
    sys.asm
    walk.asm)
# kernel.inc, records.inc and walk.inc are included by name from this folder.
target_include_directories(defragr-engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# Every NASM warning, as an error: the C++ side builds with warnings as
# errors in the gate, and the engine is held to the same bar. Less one:
# reloc-rel-dword fires on every `default rel` reference to another
# section, which ld resolves in a static program; it is noise here.
target_compile_options(defragr-engine PRIVATE -w+all -w-reloc-rel-dword -w+error)

# Link with ld directly, so no C runtime, start files or default
# library can be pulled in by a compiler driver.
set_target_properties(defragr-engine PROPERTIES LINKER_LANGUAGE ASM_NASM)
# Beside the window, which looks for its engine there.
set_target_properties(defragr-engine PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
install(TARGETS defragr-engine RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
set(CMAKE_ASM_NASM_LINK_EXECUTABLE
    "ld -static -nostdlib --fatal-warnings <LINK_FLAGS> <OBJECTS> -o <TARGET>")
