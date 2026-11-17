# The map demo: a recorded report's map with scripted changes, to watch
# the motion by eye. Built, never run by CTest.
add_executable(defragr-map-demo map-demo.cpp)
target_compile_options(defragr-map-demo PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(defragr-map-demo PRIVATE
    DEFRAGR_DEMO_RECORDING="${PROJECT_SOURCE_DIR}/tests/fixtures/image-report/report.bin")
target_link_libraries(defragr-map-demo PRIVATE defragr-map-view)
