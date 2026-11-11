# ML-DSA-44 and ML-DSA-65 (FIPS 204) from mldsa-native. ML-DSA-87 needs about
# 115 KB of stack for a signature and is built as its own image
# (suites/mldsa87), ML-DSA with reduced RAM use as suites/mldsa_lowram.
# Shared by the device build (CMakeLists.txt) and the host build (host/).
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(TP ${SUITE_DIR}/../../../third_party)

set(SUITE_SRCS
    ${SUITE_DIR}/suite.c
    ${SUITE_DIR}/mldsa_ref.c      # variant "ref": upstream C
    ${SUITE_DIR}/mldsa_opt.c      # variant "opt": with the ESP32-C3 backends
    ${SUITE_DIR}/../../mlkem/pqc_suite/c3_sponge.c
    ${CMAKE_CURRENT_LIST_DIR}/../../mlkem/pqc_suite/c3_prof.c)   # word-oriented Keccak sponge used by "opt"
set(SUITE_INCS ${SUITE_DIR} ${SUITE_DIR}/../../mlkem/pqc_suite ${TP}/mldsa-native)
set(SUITE_DEFS MLD_CONFIG_MULTILEVEL_BUILD)
if(PQC_PROFILE)
    list(APPEND SUITE_DEFS PQC_PROFILE)
endif()
if(PQC_MLDSA_UPSTREAM_NTT)   # experiment: mldsa-native's own RV32IM NTT in "opt"
    list(APPEND SUITE_DEFS PQC_MLDSA_UPSTREAM_NTT)
endif()
set(SUITE_COPTS)
if(PQC_COPT)        # experiment: extra compiler options, e.g. -D PQC_COPT=-O3
    list(APPEND SUITE_COPTS ${PQC_COPT})
endif()
set(SUITE_DEVICE_SRCS ${SUITE_DIR}/mldsa_opt_asm.S ${SUITE_DIR}/mldsa_c3_rv32imc.S)
