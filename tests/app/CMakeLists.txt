# Window tests: Qt Test, no root, no display. Gui is for QTextDocument,
# which reads markup as a tooltip does, and needs no display. D-Bus is for
# linking the placement library, whose test never calls it.
find_package(Qt6 ${DEFRAGR_QT_MIN} REQUIRED COMPONENTS Gui DBus Test LinguistTools)
qt_standard_project_setup()

# The decoder alone: INV-1 to INV-9 (docs/specs/DFR-0002-engine-protocol.md).
add_executable(tst_records tst_records.cpp)
target_compile_options(tst_records PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_records PRIVATE defragr-engine-link Qt6::Test)
add_test(NAME tst_records COMMAND tst_records)

# The developers' log: DFR-0111.
add_executable(tst_runlog tst_runlog.cpp)
target_compile_options(tst_runlog PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_runlog PRIVATE defragr-engine-link Qt6::Test)
add_test(NAME tst_runlog COMMAND tst_runlog)

# The built engine through the decoder: INV-10 and INV-11.
add_executable(tst_records_conformance tst_records_conformance.cpp)
target_compile_options(tst_records_conformance PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_records_conformance PRIVATE defragr-engine-link Qt6::Test)
add_dependencies(tst_records_conformance defragr-engine)
add_test(NAME tst_records_conformance COMMAND tst_records_conformance)
set_tests_properties(tst_records_conformance PROPERTIES
    ENVIRONMENT "DEFRAGR_ENGINE=$<TARGET_FILE:defragr-engine>")

# The built engine's probe against this machine: DFR-0004.
add_executable(tst_probe tst_probe.cpp)
target_compile_options(tst_probe PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_probe PRIVATE defragr-engine-link Qt6::Test)
add_dependencies(tst_probe defragr-engine)
add_test(NAME tst_probe COMMAND tst_probe)
set_tests_properties(tst_probe PROPERTIES
    ENVIRONMENT "DEFRAGR_ENGINE=$<TARGET_FILE:defragr-engine>")

# The engine link's ways a run can end, against the real engine and the
# fakes: DFR-0005.
set(DEFRAGR_TEST_ENGINES
    "DEFRAGR_ENGINE=$<TARGET_FILE:defragr-engine>;DEFRAGR_FAKES=${CMAKE_CURRENT_SOURCE_DIR}/fakes")
add_executable(tst_engineprocess tst_engineprocess.cpp)
target_compile_options(tst_engineprocess PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_engineprocess PRIVATE defragr-engine-link Qt6::Test)
add_dependencies(tst_engineprocess defragr-engine)
add_test(NAME tst_engineprocess COMMAND tst_engineprocess)
set_tests_properties(tst_engineprocess PROPERTIES ENVIRONMENT "${DEFRAGR_TEST_ENGINES}")

# The drive list and its actions: DFR-0005.
add_executable(tst_drivelist tst_drivelist.cpp)
target_compile_options(tst_drivelist PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_drivelist PRIVATE defragr-model Qt6::Gui Qt6::Test)
add_dependencies(tst_drivelist defragr-engine)
add_test(NAME tst_drivelist COMMAND tst_drivelist)
set_tests_properties(tst_drivelist PROPERTIES ENVIRONMENT "${DEFRAGR_TEST_ENGINES}")

# Map runs sorted into map cells: DFR-0014.
add_executable(tst_drivemap tst_drivemap.cpp)
target_compile_options(tst_drivemap PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_drivemap PRIVATE defragr-model Qt6::Test)
add_test(NAME tst_drivemap COMMAND tst_drivemap)

# The message table: DFR-0005.
add_executable(tst_messages tst_messages.cpp)
target_compile_options(tst_messages PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_messages PRIVATE defragr-model Qt6::Test)
add_test(NAME tst_messages COMMAND tst_messages)

# The report and its wording, from a fake engine: DFR-0009.
add_executable(tst_report tst_report.cpp)
target_compile_options(tst_report PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_report PRIVATE defragr-model defragr-languages Qt6::Test)
add_test(NAME tst_report COMMAND tst_report)
set_tests_properties(tst_report PROPERTIES
    ENVIRONMENT "DEFRAGR_FAKES=${CMAKE_CURRENT_SOURCE_DIR}/fakes")

# One defrag of a folder, from a fake engine: DFR-0018.
add_executable(tst_runstate tst_runstate.cpp)
target_compile_options(tst_runstate PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_runstate PRIVATE defragr-model defragr-languages Qt6::Test)
add_test(NAME tst_runstate COMMAND tst_runstate)
set_tests_properties(tst_runstate PROPERTIES
    ENVIRONMENT "DEFRAGR_FAKES=${CMAKE_CURRENT_SOURCE_DIR}/fakes")

# A recorded run, played back by the fake engine: DFR-0012.
add_executable(tst_replay tst_replay.cpp)
target_compile_options(tst_replay PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_replay PRIVATE defragr-engine-link Qt6::Test)
add_dependencies(tst_replay defragr-fake-engine)
add_test(NAME tst_replay COMMAND tst_replay)
set_tests_properties(tst_replay PROPERTIES ENVIRONMENT
    "DEFRAGR_ENGINE=$<TARGET_FILE:defragr-fake-engine>;DEFRAGR_RECORDING=${PROJECT_SOURCE_DIR}/tests/fixtures/image-report;DEFRAGR_FAKES=${CMAKE_CURRENT_SOURCE_DIR}/fakes")

# The window in the desktop's language: DFR-0066. A one-sentence Hebrew
# file stands in for the translations, which come with DFR-0067.
add_executable(tst_languages tst_languages.cpp)
target_compile_options(tst_languages PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_languages PRIVATE defragr-model defragr-languages Qt6::Test)
qt_add_lrelease(tst_languages
    TS_FILES fixtures/defragr_he.ts
    QM_FILES_OUTPUT_VARIABLE tst_languages_qm)
qt_add_resources(tst_languages hebrew
    PREFIX /i18n
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${tst_languages_qm})
add_test(NAME tst_languages COMMAND tst_languages)
set_tests_properties(tst_languages PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# The window's own buttons, pressed offscreen against a fake engine and a
# stand-in pkexec: DFR-0063.
add_executable(tst_mainwindow tst_mainwindow.cpp)
target_compile_options(tst_mainwindow PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_mainwindow PRIVATE defragr-window defragr-languages Qt6::Test)
add_test(NAME tst_mainwindow COMMAND tst_mainwindow)
set_tests_properties(tst_mainwindow PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;DEFRAGR_FAKES=${CMAKE_CURRENT_SOURCE_DIR}/fakes")

# The window's size and position kept between runs, offscreen: DFR-0109.
add_executable(tst_placement tst_placement.cpp)
target_compile_options(tst_placement PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_placement PRIVATE defragr-placement Qt6::Test)
add_test(NAME tst_placement COMMAND tst_placement)
set_tests_properties(tst_placement PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Every theme's contrast, map patterns and text floor: DFR-0013.
add_executable(tst_theme tst_theme.cpp)
target_compile_options(tst_theme PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_theme PRIVATE defragr-theme Qt6::Test)
add_test(NAME tst_theme COMMAND tst_theme)
set_tests_properties(tst_theme PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# The map painted, faded and swept, on the test's own clock: DFR-0014.
add_executable(tst_drivemapview tst_drivemapview.cpp)
target_compile_options(tst_drivemapview PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_drivemapview PRIVATE defragr-map-view Qt6::Test)
add_test(NAME tst_drivemapview COMMAND tst_drivemapview)
set_tests_properties(tst_drivemapview PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# The translation files match the source, and English has every plural
# form: DFR-0066.
add_test(NAME translations_current
    COMMAND ${PROJECT_SOURCE_DIR}/scripts/update-translations.sh
            --check ${CMAKE_CURRENT_BINARY_DIR}/translations-check)
set_tests_properties(translations_current PROPERTIES
    ENVIRONMENT "LUPDATE=$<TARGET_FILE:Qt6::lupdate>")

# Qt Test otherwise compiles every test with their absolute source and
# build folders (QT_TESTCASE_SOURCEDIR, QT_TESTCASE_BUILDDIR), for
# QFINDTESTDATA, which no test here uses. Those paths change with every
# pre-push worktree, so ccache missed on every test every push. A Qt too
# old to know the property keeps the paths, and only ccache is slower.
set_target_properties(tst_records tst_records_conformance tst_probe tst_engineprocess
    tst_drivelist tst_drivemap tst_messages tst_report tst_runstate tst_replay tst_languages tst_theme tst_drivemapview tst_mainwindow
    tst_placement PROPERTIES
    QT_SKIP_DEFAULT_TESTCASE_DIRS TRUE)
