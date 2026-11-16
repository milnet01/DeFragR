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

add_subdirectory(app)
