# PQMagic's SLH-DSA (SPHINCS+ structure) instantiated with SM3:
# slh-dsa-sm3-128f and -128s, with the simple and the robust tweakable hash.
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${SUITE_DIR}/../../pqmagic_common/pqmagic.cmake)

set(SUITE_SRCS ${SUITE_DIR}/suite.c ${PQM_HASH_SRCS}
    ${SUITE_DIR}/slhdsa_sm3_128f_simple.c ${SUITE_DIR}/slhdsa_sm3_128s_simple.c
    ${SUITE_DIR}/slhdsa_sm3_128f_robust.c ${SUITE_DIR}/slhdsa_sm3_128s_robust.c)
set(SUITE_INCS ${SUITE_DIR} ${PQM_INCS})
set(SUITE_DEFS KEMSIG_ARENA=20480 ${PQM_HASH_DEFS})
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS)
