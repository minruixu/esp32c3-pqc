# Builds one suite for the build machine: cmake -S host -B build/host/<suite> -D PQC_SUITE=<suite>
cmake_minimum_required(VERSION 3.16)
project(pqc_host C ASM)

set(PQC_SUITE "mlkem" CACHE STRING "Suite to build")
set(PQC_HOST 1)
set(ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
include(${ROOT}/components/prims/prims.cmake)
include(${ROOT}/suites/${PQC_SUITE}/pqc_suite/suite.cmake)

add_executable(suite_host
    ${SUITE_SRCS} ${PRIMS_SRCS}
    ${ROOT}/components/bench/bench_common.c
    ${CMAKE_CURRENT_LIST_DIR}/bench_host.c)
target_include_directories(suite_host PRIVATE
    ${SUITE_INCS} ${PRIMS_INCS}
    ${ROOT}/components/bench/include ${ROOT}/components/bench)
target_compile_definitions(suite_host PRIVATE ${SUITE_DEFS} PQC_HOST=1 PQC_SUITE_NAME="${PQC_SUITE}")
target_compile_options(suite_host PRIVATE -O2 -g ${SUITE_COPTS})
