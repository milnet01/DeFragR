# DeFragR: the NASM engine and the Qt 6 window, built from one tree.
# The parts and what each may depend on: docs/design.md.
cmake_minimum_required(VERSION 3.28)

project(DeFragR
    VERSION 0.2.0
    DESCRIPTION "Defragmenter for ext4 drives"
    LANGUAGES CXX ASM_NASM)

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    message(FATAL_ERROR "DeFragR's engine is x86-64 assembly; this machine is ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# A plain `cmake -S . -B build` otherwise compiles unoptimised. A type
# given on the command line or in the CMAKE_BUILD_TYPE environment
# variable wins; multi-config generators choose per build instead.
get_property(DEFRAGR_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT DEFRAGR_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
        "Debug, Release, RelWithDebInfo or MinSizeRel" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# No C++ modules here. CMake scans every C++23 source for them by default,
# which costs time and stops ccache from caching any compile.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# The oldest Qt the window is built against: 6.4 is what Ubuntu 24.04
# LTS packages (6.4.2, checked 2026-11-15). CI builds against newer.
set(DEFRAGR_QT_MIN 6.4)

include(CTest)

add_subdirectory(src/engine)
add_subdirectory(src/app)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
