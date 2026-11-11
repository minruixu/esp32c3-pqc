# ML-KEM-512/768/1024 (FIPS 203) from mlkem-native.
# Shared by the device build (CMakeLists.txt) and the host build (host/).
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(TP ${SUITE_DIR}/../../../third_party)

set(SUITE_SRCS
    ${SUITE_DIR}/suite.c
    ${SUITE_DIR}/mlkem_ref.c      # variant "ref": upstream C
    ${SUITE_DIR}/mlkem_opt.c      # variant "opt": with the ESP32-C3 backends
    ${SUITE_DIR}/c3_sponge.c      # word-oriented Keccak sponge used by "opt"
    ${SUITE_DIR}/c3_prof.c)       # sampling profiler, only with -D PQC_PROFILE=1
set(SUITE_INCS ${SUITE_DIR} ${TP}/mlkem-native)
set(SUITE_DEFS MLK_CONFIG_MULTILEVEL_BUILD)
if(PQC_PROFILE)
    list(APPEND SUITE_DEFS PQC_PROFILE)
endif()
set(SUITE_COPTS)
if(PQC_COPT)        # experiment: extra compiler options, e.g. -D PQC_COPT=-O3
    list(APPEND SUITE_COPTS ${PQC_COPT})
endif()
set(SUITE_DEVICE_SRCS ${SUITE_DIR}/mlkem_arith_rv32imc.S)
