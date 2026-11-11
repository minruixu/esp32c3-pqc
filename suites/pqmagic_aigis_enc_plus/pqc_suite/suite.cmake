# Aigis-Enc+ (NGCC round-1 candidate; reference implementation shipped with
# PQMagic), parameter sets with ring dimension 512, 1024 and 2048.
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${SUITE_DIR}/../../pqmagic_common/pqmagic.cmake)

set(SUITE_SRCS ${SUITE_DIR}/suite.c ${PQM_COMMON}/kemsig.c
    ${SUITE_DIR}/aigis_enc_plus_1.c ${SUITE_DIR}/aigis_enc_plus_2.c ${SUITE_DIR}/aigis_enc_plus_3.c)
set(SUITE_INCS ${SUITE_DIR} ${PQM_INCS})
set(SUITE_DEFS ${PQM_HASH_DEFS})
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS)
