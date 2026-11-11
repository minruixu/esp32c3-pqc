# PQMagic's ML-DSA with SM3 in place of SHAKE, and the same code with SHAKE
# as a FIPS 204 reference point. PQM_MLDSA_MODES picks the parameter sets:
# this suite builds 44 and 65; suites/pqmagic_mldsa87 builds 87, whose
# stack needs the image to itself.
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${SUITE_DIR}/../../pqmagic_common/pqmagic.cmake)
if(NOT DEFINED PQM_MLDSA_MODES)
    set(PQM_MLDSA_MODES 44 65)
endif()

set(SUITE_SRCS ${SUITE_DIR}/suite.c ${PQM_HASH_SRCS})
set(SUITE_DEFS ${PQM_HASH_DEFS})
foreach(mode ${PQM_MLDSA_MODES})
    list(APPEND SUITE_DEFS PQM_MLDSA_${mode})
    foreach(hash sm3 shake)
        list(APPEND SUITE_SRCS ${SUITE_DIR}/mldsa${mode}_${hash}.c)
    endforeach()
endforeach()
set(SUITE_INCS ${SUITE_DIR} ${PQM_INCS})
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS)
