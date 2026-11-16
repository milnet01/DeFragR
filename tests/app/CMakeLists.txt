# Window tests: Qt Test, no root, no display.
find_package(Qt6 ${DEFRAGR_QT_MIN} REQUIRED COMPONENTS Test)
qt_standard_project_setup()

# The decoder alone: INV-1 to INV-9 (docs/specs/DFR-0002-engine-protocol.md).
add_executable(tst_records tst_records.cpp)
target_compile_options(tst_records PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_records PRIVATE defragr-engine-link Qt6::Test)
add_test(NAME tst_records COMMAND tst_records)

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
target_link_libraries(tst_drivelist PRIVATE defragr-model Qt6::Test)
add_dependencies(tst_drivelist defragr-engine)
add_test(NAME tst_drivelist COMMAND tst_drivelist)
set_tests_properties(tst_drivelist PROPERTIES ENVIRONMENT "${DEFRAGR_TEST_ENGINES}")

# The message table: DFR-0005.
add_executable(tst_messages tst_messages.cpp)
target_compile_options(tst_messages PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_messages PRIVATE defragr-model Qt6::Test)
add_test(NAME tst_messages COMMAND tst_messages)

# The report and its wording, from a fake engine: DFR-0009.
add_executable(tst_report tst_report.cpp)
target_compile_options(tst_report PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_report PRIVATE defragr-model Qt6::Test)
add_test(NAME tst_report COMMAND tst_report)
set_tests_properties(tst_report PROPERTIES
    ENVIRONMENT "DEFRAGR_FAKES=${CMAKE_CURRENT_SOURCE_DIR}/fakes")

# Qt Test otherwise compiles every test with their absolute source and
# build folders (QT_TESTCASE_SOURCEDIR, QT_TESTCASE_BUILDDIR), for
# QFINDTESTDATA, which no test here uses. Those paths change with every
# pre-push worktree, so ccache missed on every test every push. A Qt too
# old to know the property keeps the paths, and only ccache is slower.
set_target_properties(tst_records tst_records_conformance tst_probe tst_engineprocess
    tst_drivelist tst_messages tst_report PROPERTIES
    QT_SKIP_DEFAULT_TESTCASE_DIRS TRUE)
