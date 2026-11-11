# LMS and HSS (RFC 8554, RFC 9858, NIST SP 800-208), implementation in lms.c.
# Shared by the device build (CMakeLists.txt) and the host build (host/).
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})

set(SUITE_SRCS
    ${SUITE_DIR}/suite.c
    ${SUITE_DIR}/lms.c)
set(SUITE_INCS ${SUITE_DIR})
set(SUITE_DEFS)
# For the measurements in journal/experiments/lms.jsonl (optimization-journal branch):
#   -D LMS_HW_CHAIN=off   chains through the general SHA-256 backend
#   -D LMS_HW_CHAIN=loop  the hardware chain as a loop over n/4 words
if(LMS_HW_CHAIN STREQUAL "off")
    set(SUITE_DEFS LMS_HW_CHAIN_OFF=1)
elseif(LMS_HW_CHAIN STREQUAL "loop")
    set(SUITE_DEFS LMS_HW_CHAIN_LOOP=1)
endif()
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS ${SUITE_DIR}/lms_hw.c)   # SHA accelerator, device only
