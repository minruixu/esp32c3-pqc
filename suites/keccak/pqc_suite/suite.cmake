# Keccak-f[1600] micro-benchmark and the instruction cost model of the core.
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(TP ${SUITE_DIR}/../../../third_party)

set(SUITE_SRCS
    ${SUITE_DIR}/suite.c
    ${SUITE_DIR}/mlkn_keccak.c)
set(SUITE_INCS ${SUITE_DIR} ${TP}/mlkem-native)
set(SUITE_DEFS)
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS
    ${SUITE_DIR}/cpumodel.S
    ${SUITE_DIR}/pqrv_keccak_loop.S
    ${SUITE_DIR}/pqrv_keccak_unroll.S)
