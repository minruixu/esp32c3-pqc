# SPHINCS-alpha (PQMagic) with the simple tweakable hash. SPX_SETS picks the
# parameter sets of the image: this suite builds 128f and 128s for SM3, SHA-2
# and SHAKE; suites/pqmagic_sphincsa_192f builds 192f. The other sets need a
# table of 330 KB or more even in compact form and do not fit in RAM.
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${SUITE_DIR}/../../pqmagic_common/pqmagic.cmake)
if(NOT DEFINED SPX_SETS)
    set(SPX_SETS sm3_128f sm3_128s sha2_128f sha2_128s shake_128f shake_128s)
    set(SPX_ARENA 20480)
endif()

set(SUITE_SRCS ${SUITE_DIR}/suite.c ${PQM_HASH_SRCS} ${SUITE_DIR}/sphincsa_dp.c)
set(SUITE_DEFS KEMSIG_ARENA=${SPX_ARENA} ${PQM_HASH_DEFS})
foreach(set ${SPX_SETS})
    list(APPEND SUITE_SRCS ${SUITE_DIR}/sphincsa_${set}.c)
    list(APPEND SUITE_DEFS SPX_HAVE_${set})
endforeach()
set(SUITE_INCS ${SUITE_DIR} ${PQM_INCS})
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS)
