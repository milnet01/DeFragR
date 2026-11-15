# Engine checks that need no root: it runs, and its binary has the shape
# docs/design.md and the assembly standard require.
add_test(NAME engine_runs COMMAND defragr-engine)

add_test(NAME engine_binary_shape
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/engine/binary-shape.sh
            $<TARGET_FILE:defragr-engine>)
