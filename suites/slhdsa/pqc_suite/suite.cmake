# SLH-DSA (FIPS 205), all 12 parameter sets, from slhdsa-c.
# Shared by the device build (CMakeLists.txt) and the host build (host/).
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(ROOT ${SUITE_DIR}/../../..)
set(SLH ${ROOT}/third_party/slhdsa-c)

set(SUITE_SRCS
    ${SUITE_DIR}/suite.c
    ${SUITE_DIR}/slh_glue.c
    ${SLH}/slh_dsa.c
    ${SLH}/slh_sha2.c
    ${SLH}/slh_shake.c
    ${SLH}/sha2_256.c
    ${SLH}/sha2_512.c
    ${SLH}/sha3_api.c
    ${SLH}/sha3_f1600.c)
set(SUITE_INCS ${SUITE_DIR} ${SLH})
set(SUITE_DEFS SLH_PQC_PRIMS)
set(SUITE_COPTS)
# Device only, the "opt" variant: SHA-256 on the hardware accelerator, and
# the SHAKE functions written directly on the Keccak state.
set(SUITE_DEVICE_SRCS ${SUITE_DIR}/slh_sha2_c3.c ${SUITE_DIR}/slh_shake_fast.c)
