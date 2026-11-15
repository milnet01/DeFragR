# The window: C++23, Qt 6 Widgets and Qt D-Bus.
find_package(Qt6 ${DEFRAGR_QT_MIN} REQUIRED COMPONENTS Widgets DBus)
qt_standard_project_setup()

add_executable(defragr
    main.cpp)

target_compile_options(defragr PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(defragr PRIVATE Qt6::Widgets Qt6::DBus)
