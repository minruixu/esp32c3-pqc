# ML-KEM-512/768/1024 (FIPS 203) from mlkem-native.
# Shared by the device build (CMakeLists.txt) and the host build (host/).
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(TP ${SUITE_DIR}/../../../third_party)

set(SUITE_SRCS
    ${SUITE_DIR}/suite.c
    ${SUITE_DIR}/mlkem_all.c)
set(SUITE_INCS ${SUITE_DIR} ${TP}/mlkem-native)
set(SUITE_DEFS MLK_CONFIG_NAMESPACE_PREFIX=mlkem MLK_CONFIG_MULTILEVEL_BUILD)
set(SUITE_COPTS)          # extra compiler options
set(SUITE_DEVICE_SRCS)    # device-only sources (assembly, hardware drivers)
