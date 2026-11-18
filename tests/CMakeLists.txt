# Engine checks that need no root: its binary has the shape docs/design.md
# and the assembly standard require. Whether it runs and speaks the
# protocol is tst_records_conformance's, in app/.

add_test(NAME engine_binary_shape
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/engine/binary-shape.sh
            $<TARGET_FILE:defragr-engine>)

# The throwaway test machine image tests run in (ADR-0003). Skipped where
# the host cannot boot it, unless DEFRAGR_VM_REQUIRED=1, as the gate sets.
add_test(NAME vm_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vm/test-vm.sh
            ${CMAKE_CURRENT_BINARY_DIR}/vm)
set_tests_properties(vm_smoke PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)

# Image tests, in the test machine. image_build checks the fragmented
# images the others are built on.
add_test(NAME image_build
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/image/test-build.sh
            ${CMAKE_CURRENT_BINARY_DIR}/image)
set_tests_properties(image_build PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)

# The engine's report on a fragmented image, against filefrag: DFR-0008.
# report-check decodes the engine's output on the host.
find_package(Qt6 ${DEFRAGR_QT_MIN} REQUIRED COMPONENTS Core)
add_executable(report-check image/report-check.cpp)
target_compile_options(report-check PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(report-check PRIVATE defragr-engine-link)
# hold-lease runs in the test machine, which has no C library, so it is
# built and linked as the engine is (src/engine/CMakeLists.txt).
add_executable(hold-lease image/hold-lease.asm)
target_compile_options(hold-lease PRIVATE -w+all -w-reloc-rel-dword -w+error)
set_target_properties(hold-lease PROPERTIES LINKER_LANGUAGE ASM_NASM)
set(CMAKE_ASM_NASM_LINK_EXECUTABLE
    "ld -static -nostdlib --fatal-warnings <LINK_FLAGS> <OBJECTS> -o <TARGET>")
add_test(NAME image_report
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/image/test-report.sh
            ${CMAKE_CURRENT_BINARY_DIR}/image-report
            $<TARGET_FILE:defragr-engine> $<TARGET_FILE:report-check>
            $<TARGET_FILE:hold-lease>)
set_tests_properties(image_report PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)

add_subdirectory(fake-engine)
add_subdirectory(app)
add_subdirectory(map-demo)
