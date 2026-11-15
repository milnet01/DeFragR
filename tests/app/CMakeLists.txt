# Window tests: Qt Test, no root, no display.
find_package(Qt6 ${DEFRAGR_QT_MIN} REQUIRED COMPONENTS Test)
qt_standard_project_setup()

# The decoder alone: INV-1 to INV-9 (docs/specs/DFR-0002-engine-protocol.md).
add_executable(tst_records tst_records.cpp)
target_compile_options(tst_records PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_records PRIVATE defragr-records Qt6::Test)
add_test(NAME tst_records COMMAND tst_records)

# The built engine through the decoder: INV-10 and INV-11.
add_executable(tst_records_conformance tst_records_conformance.cpp)
target_compile_options(tst_records_conformance PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_records_conformance PRIVATE defragr-records Qt6::Test)
add_dependencies(tst_records_conformance defragr-engine)
add_test(NAME tst_records_conformance COMMAND tst_records_conformance)
set_tests_properties(tst_records_conformance PROPERTIES
    ENVIRONMENT "DEFRAGR_ENGINE=$<TARGET_FILE:defragr-engine>")

# Qt Test otherwise compiles both tests with their absolute source and
# build folders (QT_TESTCASE_SOURCEDIR, QT_TESTCASE_BUILDDIR), for
# QFINDTESTDATA, which no test here uses. Those paths change with every
# pre-push worktree, so ccache missed on both tests every push. A Qt too
# old to know the property keeps the paths, and only ccache is slower.
set_target_properties(tst_records tst_records_conformance PROPERTIES
    QT_SKIP_DEFAULT_TESTCASE_DIRS TRUE)
