# The window: C++23, Qt 6 Widgets and Qt D-Bus.
find_package(Qt6 ${DEFRAGR_QT_MIN} REQUIRED COMPONENTS Widgets DBus)
message(STATUS "Qt ${Qt6_VERSION} (floor ${DEFRAGR_QT_MIN})")
qt_standard_project_setup()

# Engine link and the model parts: Qt Core only, each a library of its
# own, so the window tests link them without the window or a screen.
add_library(defragr-engine-link STATIC
    engine/EngineProcess.cpp
    engine/Records.cpp)
target_include_directories(defragr-engine-link PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(defragr-engine-link PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(defragr-engine-link PUBLIC Qt6::Core)

add_library(defragr-model STATIC
    model/DriveList.cpp
    model/Messages.cpp)
target_compile_options(defragr-model PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(defragr-model PUBLIC defragr-engine-link)

add_executable(defragr
    main.cpp
    ui/MainWindow.cpp)
# Beside the engine, where EngineProcess looks for it when
# DEFRAGR_ENGINE is unset; src/engine/CMakeLists.txt does the same.
set_target_properties(defragr PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# The window icon. The PNG, because Qt draws an SVG icon only through the
# qt6-svg plugin, and a missing plugin gives a blank icon with no error.
# packaging/defragr.svg is the same icon, scalable, for the desktop entry.
qt_add_resources(defragr icons
    PREFIX /
    BASE ${PROJECT_SOURCE_DIR}/packaging
    FILES ${PROJECT_SOURCE_DIR}/packaging/defragr.png)

target_compile_options(defragr PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(defragr PRIVATE defragr-model Qt6::Widgets Qt6::DBus)
