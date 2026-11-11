# FN-DSA (Falcon) 512 and 1024 from c-fn-dsa.
# Shared by the device build (CMakeLists.txt) and the host build (host/).
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(FNDSA_DIR ${SUITE_DIR}/../../../third_party/c-fn-dsa)

set(FNDSA_LIB_SRCS
    ${FNDSA_DIR}/codec.c ${FNDSA_DIR}/mq.c ${FNDSA_DIR}/sha3.c ${FNDSA_DIR}/util.c
    ${FNDSA_DIR}/kgen.c ${FNDSA_DIR}/kgen_fxp.c ${FNDSA_DIR}/kgen_gauss.c
    ${FNDSA_DIR}/kgen_mp31.c ${FNDSA_DIR}/kgen_ntru.c ${FNDSA_DIR}/kgen_poly.c
    ${FNDSA_DIR}/kgen_zint31.c
    ${FNDSA_DIR}/sign.c ${FNDSA_DIR}/sign_core.c ${FNDSA_DIR}/sign_fpoly.c
    ${FNDSA_DIR}/sign_fpr.c ${FNDSA_DIR}/sign_sampler.c
    ${FNDSA_DIR}/vrfy.c)

set(SUITE_SRCS
    ${SUITE_DIR}/suite.c
    ${SUITE_DIR}/fndsa_glue.c
    ${FNDSA_LIB_SRCS})
set(SUITE_INCS ${SUITE_DIR} ${FNDSA_DIR})
# The Keccak permutation comes from pqc_prims.h (see fndsa_glue.c).
set(SUITE_DEFS FNDSA_EXTERNAL_KECCAK=1)
set(SUITE_COPTS)
# ESP32-C3 build of the signing code (32-bit emulated floating point).
set(SUITE_DEVICE_SRCS ${SUITE_DIR}/fndsa_sign_c3.c)
if(FNDSA_C3_COPTS AND NOT CMAKE_SCRIPT_MODE_FILE)
    # Experiment: other compiler options for that file only, for example
    # -D FNDSA_C3_COPTS=-O3 (see NOTES.md).
    set_source_files_properties(${SUITE_DIR}/fndsa_sign_c3.c PROPERTIES COMPILE_OPTIONS "${FNDSA_C3_COPTS}")
endif()
