# ML-DSA-44/65/87 with MLD_CONFIG_REDUCE_RAM (variant "lowram"): the sources
# of suites/mldsa built with PQC_MLDSA_LOWRAM. A separate image, because the
# reference and optimized builds already fill the one of suites/mldsa.
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(MLDSA_DIR ${SUITE_DIR}/../../mldsa/pqc_suite)
set(TP ${SUITE_DIR}/../../../third_party)

set(SUITE_SRCS
    ${MLDSA_DIR}/suite.c
    ${MLDSA_DIR}/mldsa_opt.c
    ${MLDSA_DIR}/../../mlkem/pqc_suite/c3_sponge.c
    ${CMAKE_CURRENT_LIST_DIR}/../../mlkem/pqc_suite/c3_prof.c)
set(SUITE_INCS ${MLDSA_DIR} ${MLDSA_DIR}/../../mlkem/pqc_suite ${TP}/mldsa-native)
set(SUITE_DEFS MLD_CONFIG_MULTILEVEL_BUILD PQC_MLDSA_LOWRAM)
if(PQC_PROFILE)
    list(APPEND SUITE_DEFS PQC_PROFILE)
endif()
set(SUITE_COPTS)
if(PQC_COPT)        # experiment: extra compiler options, e.g. -D PQC_COPT=-O3
    list(APPEND SUITE_COPTS ${PQC_COPT})
endif()
set(SUITE_DEVICE_SRCS ${MLDSA_DIR}/mldsa_opt_asm.S ${MLDSA_DIR}/mldsa_c3_rv32imc.S)
