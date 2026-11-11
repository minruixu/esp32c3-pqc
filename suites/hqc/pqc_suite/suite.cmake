# HQC-1/3/5 from the official reference implementation (release v5.1.0).
# Shared by the device build (CMakeLists.txt) and the host build (host/).
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(HQC_SRC ${SUITE_DIR}/../../../third_party/hqc/src)

set(SUITE_INCS
    ${SUITE_DIR}            # also holds the empty immintrin.h
    ${HQC_SRC}
    ${HQC_SRC}/common ${HQC_SRC}/ref
    # Only so that by-name includes of the per-level headers resolve; the
    # level actually used is chosen in hqc_unit_*.inc.
    ${HQC_SRC}/common/hqc-1 ${HQC_SRC}/ref/hqc-1)
set(SUITE_COPTS)
# Task stacks in bytes: measured use plus a margin (see NOTES.md).
set(SUITE_DEFS HQC_STACK_REF1=80000 HQC_STACK_OPT1=28000 HQC_STACK_OPT3=52000 HQC_STACK_OPT5=80000)

if(HQC_C3_NO_ASM)
    # Use the C version of the multiplication inner loop on the device too.
    list(APPEND SUITE_DEFS HQC_C3_NO_ASM=1)
endif()
if(HQC_HOST_OPT)
    # Host build: run the optimized C code instead of the reference code.
    list(APPEND SUITE_DEFS HQC_HOST_OPT=1)
endif()

if(HQC_REF3_ONLY)
    # Image "hqc3ref": the reference code of HQC-3 on a static stack.
    list(APPEND SUITE_DEFS HQC_REF3_ONLY=1 HQC_BIGSTACK_BYTES=140000)
    set(SUITE_SRCS
        ${SUITE_DIR}/suite.c ${SUITE_DIR}/hqc_glue.c ${SUITE_DIR}/hqc3_ref.c)
    set(SUITE_DEVICE_SRCS ${SUITE_DIR}/hqc_bigstack.c)
else()
    set(SUITE_SRCS
        ${SUITE_DIR}/suite.c ${SUITE_DIR}/hqc_glue.c
        ${SUITE_DIR}/hqc1_ref.c ${SUITE_DIR}/hqc3_ref.c ${SUITE_DIR}/hqc5_ref.c
        ${SUITE_DIR}/hqc1_opt.c ${SUITE_DIR}/hqc3_opt.c ${SUITE_DIR}/hqc5_opt.c)
    set(SUITE_DEVICE_SRCS ${SUITE_DIR}/hqc_c3_mul.S ${SUITE_DIR}/hqc_probe.c ${SUITE_DIR}/hqc_selftest.c)
    if(HQC_HOST_OPT)
        list(APPEND SUITE_SRCS ${SUITE_DIR}/hqc_selftest.c)
    endif()
endif()
