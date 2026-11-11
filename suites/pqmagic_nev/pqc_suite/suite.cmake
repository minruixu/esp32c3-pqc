# NEV (NGCC round-1 candidate; reference implementation shipped with
# PQMagic): all twelve parameter sets, hashing with SHA-3/SHAKE as PQMagic's
# library build does. suites/pqmagic_nev_sm3 builds the SM3 configuration.
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${SUITE_DIR}/../../pqmagic_common/pqmagic.cmake)

set(SUITE_SRCS ${SUITE_DIR}/suite.c ${PQM_COMMON}/kemsig.c)
foreach(mode 512_769_c 1024_769_c 2048_769_c 512_1409 1024_1409 2048_1409
             512_3329 1024_3329 2048_3329 512_769 1024_769 2048_769)
    list(APPEND SUITE_SRCS ${SUITE_DIR}/nev_${mode}.c)
endforeach()
set(SUITE_INCS ${SUITE_DIR} ${PQM_INCS})
set(SUITE_DEFS ${PQM_HASH_DEFS})
if(PQM_NEV_SM3)
    list(APPEND SUITE_DEFS PQM_HASH_SM3)
endif()
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS)
