# Shared by every suite that builds code from third_party/pqmagic.
# Sets PQM_DIR, PQM_COMMON, PQM_HASH_SRCS (hash layer), PQM_HASH_DEFS (add to
# SUITE_DEFS) and PQM_INCS.
set(PQM_COMMON ${CMAKE_CURRENT_LIST_DIR})
set(PQM_DIR ${PQM_COMMON}/../../third_party/pqmagic)

set(PQM_HASH_DEFS)
if(PQM_UPSTREAM_HASH)
    # Host-only cross-check: PQMagic's own SM3 and Keccak instead of the
    # project's primitives. Outputs must be identical.
    set(PQM_HASH_DEFS PQM_UPSTREAM_HASH)
    set(PQM_HASH_SRCS
        ${PQM_DIR}/hash/sm3/x86-64/sm3.c
        ${PQM_DIR}/hash/keccak/fips202.c
        ${PQM_DIR}/hash/sm3/sm3_extended.c)
else()
    set(PQM_HASH_SRCS
        ${PQM_COMMON}/port/pqm_sm3.c
        ${PQM_COMMON}/port/pqm_sm3_extended.c
        ${PQM_COMMON}/port/pqm_fips202.c)
endif()
if(PQM_NO_LEAN_SM3)
    # Measurement aid: the optimized variant without the lean SM3 paths of
    # port/pqm_sm3.c, to see what they gain on top of a faster compression.
    list(APPEND PQM_HASH_DEFS PQM_NO_LEAN_SM3)
endif()
list(APPEND PQM_HASH_SRCS
    ${PQM_DIR}/hash/sm3/mgf_sm3.c
    ${PQM_COMMON}/kemsig.c)
set(PQM_INCS ${PQM_COMMON} ${PQM_DIR} ${PQM_DIR}/include ${PQM_DIR}/utils)
