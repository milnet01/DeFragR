# Writes BuildId.h: the version and the commit the window was built from,
# marked -modified when the tree had uncommitted changes. Run on every
# build by src/app/CMakeLists.txt; the header is replaced only when its
# text changes, so an unchanged id rebuilds nothing.
#
# Takes -DSOURCE_DIR, -DOUTPUT and -DVERSION, and -DBUILD_ID, which, when
# not empty, is the id used instead of git's.

set(build "unknown")
find_program(GIT git)
if(BUILD_ID)
    set(build ${BUILD_ID})
elseif(GIT)
    execute_process(
        COMMAND ${GIT} -C ${SOURCE_DIR} describe --always --dirty=-modified
        OUTPUT_VARIABLE described
        RESULT_VARIABLE result
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(result EQUAL 0)
        set(build ${described})
    endif()
endif()

file(WRITE ${OUTPUT}.new
    "#pragma once\n"
    "#define DEFRAGR_VERSION \"${VERSION}\"\n"
    "#define DEFRAGR_BUILD \"${build}\"\n")
file(COPY_FILE ${OUTPUT}.new ${OUTPUT} ONLY_IF_DIFFERENT)
file(REMOVE ${OUTPUT}.new)
