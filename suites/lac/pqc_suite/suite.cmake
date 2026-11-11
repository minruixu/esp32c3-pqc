# LAC (LAC-v3b: Light, 128, 192, 256), from downloads/lac-intel32 (run
# tools/fetch/lac.sh first; the repository has no license, so its code is not
# part of this project).
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(LAC_ROOT ${SUITE_DIR}/../../.. ABSOLUTE)
set(LAC_PORT ${LAC_ROOT}/downloads/lac_port)
set(LAC_COMMON ${LAC_ROOT}/suites/pqmagic_common)
if(NOT EXISTS ${LAC_PORT}/lac_param.h)
    message(FATAL_ERROR "LAC sources missing: run tools/fetch/lac.sh")
endif()

set(SUITE_SRCS ${SUITE_DIR}/suite.c ${SUITE_DIR}/lac_port.c
    ${SUITE_DIR}/lac_light.c ${SUITE_DIR}/lac_128.c ${SUITE_DIR}/lac_192.c ${SUITE_DIR}/lac_256.c
    ${LAC_COMMON}/kemsig.c ${LAC_COMMON}/hwaes.c ${LAC_ROOT}/third_party/pqclean_aes/aes.c)
set(SUITE_INCS ${SUITE_DIR} ${LAC_COMMON} ${LAC_PORT} ${LAC_ROOT}/third_party)
set(SUITE_DEFS)
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS)
