# XMSS and XMSS^MT (RFC 8391, NIST SP 800-208): the reference implementation
# from third_party/xmss-reference, with the hash layer of hash_opt.c.
# Shared by the device build (CMakeLists.txt) and the host build (host/).
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(XMSS_REF ${SUITE_DIR}/../../../third_party/xmss-reference)

set(SUITE_SRCS
    ${SUITE_DIR}/suite.c
    ${SUITE_DIR}/hash_ref.c
    ${SUITE_DIR}/hash_opt.c
    ${SUITE_DIR}/core_fast.c
    ${SUITE_DIR}/core_slow.c
    ${XMSS_REF}/params.c
    ${XMSS_REF}/hash_address.c
    ${XMSS_REF}/wots.c
    ${XMSS_REF}/xmss_commons.c
    ${XMSS_REF}/utils.c)
set(SUITE_INCS ${SUITE_DIR} ${SUITE_DIR}/shim ${XMSS_REF})
set(SUITE_DEFS)
# For the measurements in journal/experiments/xmss.jsonl:
#   -D XMSS_HW_HASH=off   PRF, F and H through the general SHA-256 backend
if(XMSS_HW_HASH STREQUAL "off")
    set(SUITE_DEFS XMSS_HW_HASH_OFF=1)
endif()
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS ${SUITE_DIR}/hash_hw.c)   # SHA accelerator, device only
