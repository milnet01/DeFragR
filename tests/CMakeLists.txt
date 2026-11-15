# Engine checks that need no root: its binary has the shape docs/design.md
# and the assembly standard require. Whether it runs and speaks the
# protocol is tst_records_conformance's, in app/.

add_test(NAME engine_binary_shape
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/engine/binary-shape.sh
            $<TARGET_FILE:defragr-engine>)

add_subdirectory(app)
