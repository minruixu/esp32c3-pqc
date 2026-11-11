# One firmware image per suite: -D PQC_SUITE=<name> picks suites/<name>.
# Normally driven by tools/pqc; see docs/PORTING.md.
cmake_minimum_required(VERSION 3.16)

set(PQC_SUITE "mlkem" CACHE STRING "Suite to build (a directory under suites/)")
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/suites/${PQC_SUITE}/pqc_suite/CMakeLists.txt")
    message(FATAL_ERROR "Unknown suite '${PQC_SUITE}': suites/${PQC_SUITE}/pqc_suite not found")
endif()
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/suites/${PQC_SUITE}")
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pqc_bench)
