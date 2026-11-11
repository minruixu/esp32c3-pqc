# SCloud+ (unstructured-LWE KEM, NGCC round-1 candidate), reference tier, from
# downloads/scloudplus (run tools/fetch/scloudplus.sh first; the repository
# has no license file, so its code is not part of this project).
# SC_LEVELS and SC_FAMILIES pick what one image holds.
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(SC_SUITE_DIR ${SUITE_DIR})
get_filename_component(SC_ROOT ${SUITE_DIR}/../../.. ABSOLUTE)
set(SC_SRC ${SC_ROOT}/downloads/scloudplus/Implementations)
set(SC_PORT ${SC_ROOT}/downloads/scloudplus_port)
set(SC_COMMON ${SC_ROOT}/suites/pqmagic_common)
if(NOT EXISTS ${SC_PORT}/hash_aes_shake.c)
    message(FATAL_ERROR "SCloud+ sources missing: run tools/fetch/scloudplus.sh")
endif()
if(NOT DEFINED SC_LEVELS)
    set(SC_LEVELS 128 192)
endif()
if(NOT DEFINED SC_FAMILIES)
    set(SC_FAMILIES sm3 shake aes)
endif()

set(SUITE_SRCS ${SUITE_DIR}/suite.c ${SC_COMMON}/kemsig.c ${SC_COMMON}/hwaes.c
    ${SC_SRC}/_shared/api_pkc/drng.c)
if(NOT DEFINED SC_ARENA)
    set(SC_ARENA 40960)
endif()
set(SUITE_DEFS KEMSIG_ARENA=${SC_ARENA})
foreach(level ${SC_LEVELS})
    foreach(fam ${SC_FAMILIES})
        list(APPEND SUITE_SRCS ${SUITE_DIR}/sc_${level}_${fam}.c)
        list(APPEND SUITE_DEFS SC_HAVE_${level}_${fam})
    endforeach()
endforeach()
set(SUITE_INCS ${SUITE_DIR} ${SC_COMMON} ${SC_PORT} ${SC_SRC}
    ${SC_SRC}/_shared/scloudplus_core/include ${SC_SRC}/_shared/scloudplus_core/common
    ${SC_SRC}/_shared/scloudplus_core/ref ${SC_SRC}/_shared/api_pkc)
if(PQM_UPSTREAM_HASH)
    list(APPEND SUITE_DEFS PQM_UPSTREAM_HASH)
endif()
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS)
