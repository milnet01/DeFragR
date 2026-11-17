# The window: C++23, Qt 6 Widgets and Qt D-Bus.
find_package(Qt6 ${DEFRAGR_QT_MIN} REQUIRED COMPONENTS Widgets DBus LinguistTools)
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
    model/DriveMap.cpp
    model/Messages.cpp
    model/Report.cpp)
target_compile_options(defragr-model PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(defragr-model PUBLIC defragr-engine-link)

# Languages: Qt Gui only, so its test needs no widgets. The translations
# are built into it, under :/i18n.
add_library(defragr-languages STATIC ui/Languages.cpp)
target_include_directories(defragr-languages PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(defragr-languages PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(defragr-languages PUBLIC Qt6::Gui)
file(GLOB DEFRAGR_TS_FILES CONFIGURE_DEPENDS translations/defragr_*.ts)
qt_add_lrelease(defragr-languages
    TS_FILES ${DEFRAGR_TS_FILES}
    QM_FILES_OUTPUT_VARIABLE DEFRAGR_QM_FILES)
qt_add_resources(defragr-languages translations
    PREFIX /i18n
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${DEFRAGR_QM_FILES})

# Theme: Qt Gui only, so its test paints without widgets or a screen.
add_library(defragr-theme STATIC ui/Theme.cpp)
target_include_directories(defragr-theme PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(defragr-theme PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(defragr-theme PUBLIC Qt6::Gui)

# Map view: Qt Widgets, apart from the window so its test can paint it
# offscreen.
add_library(defragr-map-view STATIC ui/DriveMapView.cpp)
target_compile_options(defragr-map-view PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(defragr-map-view PUBLIC defragr-model defragr-theme Qt6::Widgets)

# Refreshes translations/ from the source, through the script the
# translations_current test runs. Qt's own lupdate targets are not used:
# under Qt 6.12 they scanned every target, and filed some words under a
# name the window never looks up.
add_custom_target(update-translations
    COMMAND ${CMAKE_COMMAND} -E env LUPDATE=$<TARGET_FILE:Qt6::lupdate>
            ${PROJECT_SOURCE_DIR}/scripts/update-translations.sh
    VERBATIM)

add_executable(defragr
    main.cpp
    ui/MainWindow.cpp
    ui/ReportView.cpp)
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

# The window title names the build, so a tester can tell which one runs.
add_custom_target(defragr-build-id
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
            -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/generated/BuildId.h
            -DVERSION=${PROJECT_VERSION}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/build-id.cmake
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/generated/BuildId.h
    VERBATIM)
add_dependencies(defragr defragr-build-id)
target_include_directories(defragr PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_compile_options(defragr PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(defragr PRIVATE defragr-model defragr-languages defragr-theme defragr-map-view
    Qt6::Widgets Qt6::DBus)
