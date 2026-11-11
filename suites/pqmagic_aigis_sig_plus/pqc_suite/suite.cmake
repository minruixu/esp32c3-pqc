# Aigis-Sig+ (NGCC round-1 candidate; reference implementation shipped with
# PQMagic), parameter sets I, II and III.
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${SUITE_DIR}/../../pqmagic_common/pqmagic.cmake)

set(SUITE_SRCS ${SUITE_DIR}/suite.c ${PQM_COMMON}/kemsig.c
    ${SUITE_DIR}/aigis_sig_plus_1.c ${SUITE_DIR}/aigis_sig_plus_2.c ${SUITE_DIR}/aigis_sig_plus_3.c)
set(SUITE_INCS ${SUITE_DIR} ${PQM_INCS})
set(SUITE_DEFS KEMSIG_ARENA=32768 ${PQM_HASH_DEFS})
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS)
