# Micro-benchmarks and self-tests of the hash primitives: SHA-256 (portable C
# and the ESP32-C3 accelerator), SHA-512, SM3 and Keccak-f[1600].
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(ROOT ${SUITE_DIR}/../../..)
set(SLH ${ROOT}/third_party/slhdsa-c)

set(SUITE_SRCS
    ${SUITE_DIR}/suite.c
    ${SLH}/sha2_256.c
    ${SLH}/sha2_512.c
    ${SLH}/sha3_f1600.c)
set(SUITE_INCS ${SUITE_DIR} ${SLH})
set(SUITE_DEFS SLH_PQC_PRIMS)
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS ${SUITE_DIR}/hw_probe.c)
