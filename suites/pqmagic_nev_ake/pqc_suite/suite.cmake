# NEV-AKE (authenticated key exchange, NGCC round-1 candidate; reference
# implementation shipped with PQMagic): all nine parameter sets.
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${SUITE_DIR}/../../pqmagic_common/pqmagic.cmake)

set(SUITE_SRCS ${SUITE_DIR}/suite.c ${PQM_COMMON}/kemsig.c)
foreach(mode 512_769_c 1024_769_c 2048_769_c 512_1409 1024_1409 2048_1409 512_769 1024_769 2048_769)
    list(APPEND SUITE_SRCS ${SUITE_DIR}/nev_ake_${mode}.c)
endforeach()
set(SUITE_INCS ${SUITE_DIR} ${PQM_INCS})
set(SUITE_DEFS KEMSIG_ARENA=49152 ${PQM_HASH_DEFS})
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS)
