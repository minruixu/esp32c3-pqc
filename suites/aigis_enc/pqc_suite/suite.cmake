# Aigis-enc, parameter sets 1-4, with SM3 and with SHAKE (PQMagic).
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${SUITE_DIR}/../../pqmagic_common/pqmagic.cmake)

set(SUITE_SRCS ${SUITE_DIR}/suite.c ${PQM_HASH_SRCS})
foreach(mode 1 2 3 4)
    foreach(hash sm3 shake)
        list(APPEND SUITE_SRCS ${SUITE_DIR}/aigis_enc_${mode}_${hash}.c)
    endforeach()
endforeach()
set(SUITE_INCS ${SUITE_DIR} ${PQM_INCS})
set(SUITE_DEFS ${PQM_HASH_DEFS})
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS)
