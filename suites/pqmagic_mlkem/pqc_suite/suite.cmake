# PQMagic's ML-KEM-512/768/1024 with SM3 in place of SHA-3/SHAKE, and the
# same code with SHAKE as a FIPS 203 reference point.
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${SUITE_DIR}/../../pqmagic_common/pqmagic.cmake)

set(SUITE_SRCS ${SUITE_DIR}/suite.c ${PQM_HASH_SRCS})
foreach(mode 512 768 1024)
    foreach(hash sm3 shake)
        list(APPEND SUITE_SRCS ${SUITE_DIR}/mlkem${mode}_${hash}.c)
    endforeach()
endforeach()
set(SUITE_INCS ${SUITE_DIR} ${PQM_INCS})
set(SUITE_DEFS ${PQM_HASH_DEFS})
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS)
