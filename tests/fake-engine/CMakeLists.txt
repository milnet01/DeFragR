# The fake engine, which plays back the recordings in tests/fixtures/.
# Standard C++ only: it stands in for the engine, not the window.
add_executable(defragr-fake-engine fake-engine.cpp)
target_compile_options(defragr-fake-engine PRIVATE -Wall -Wextra -Wpedantic)
