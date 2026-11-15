# The window: C++23, Qt 6 Widgets and Qt D-Bus.
find_package(Qt6 ${DEFRAGR_QT_MIN} REQUIRED COMPONENTS Widgets DBus)
message(STATUS "Qt ${Qt6_VERSION} (floor ${DEFRAGR_QT_MIN})")
qt_standard_project_setup()

# Engine link: the record decoder. A library of its own, so the window
# tests link it without the window.
add_library(defragr-records STATIC
    engine/Records.cpp)
target_include_directories(defragr-records PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(defragr-records PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(defragr-records PUBLIC Qt6::Core)

add_executable(defragr
    main.cpp)

target_compile_options(defragr PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(defragr PRIVATE Qt6::Widgets Qt6::DBus)
