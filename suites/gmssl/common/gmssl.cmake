# GmSSL's post-quantum algorithms, one image per algorithm because all four
# together do not fit into the RAM of the ESP32-C3. A suite sets GMSSL_ALG to
# lms, xmss, sphincs or kyber and includes this file.
set(GM_DIR ${CMAKE_CURRENT_LIST_DIR})
set(GMSSL ${GM_DIR}/../../../third_party/gmssl)

set(SUITE_SRCS
    ${GM_DIR}/suite.c
    ${GM_DIR}/gmssl_port.c
    ${GM_DIR}/gmssl_sm3_ref.c
    ${GMSSL}/src/hex.c)
if(GMSSL_ALG STREQUAL "lms")
    list(APPEND SUITE_SRCS ${GM_DIR}/t_lms.c ${GMSSL}/src/lms.c)
    set(SUITE_DEFS GMSSL_WITH_LMS=1)
elseif(GMSSL_ALG STREQUAL "xmss")
    list(APPEND SUITE_SRCS ${GM_DIR}/t_xmss.c ${GMSSL}/src/xmss.c)
    set(SUITE_DEFS GMSSL_WITH_XMSS=1)
elseif(GMSSL_ALG STREQUAL "sphincs")
    list(APPEND SUITE_SRCS ${GM_DIR}/t_sphincs.c ${GMSSL}/src/sphincs.c ${GMSSL}/src/sm3_hmac.c)
    set(SUITE_DEFS GMSSL_WITH_SPHINCS=1)
    set(GMSSL_DEVICE_SRCS ${GM_DIR}/bigstack.c)   # static stack for the 131 KB local tree
elseif(GMSSL_ALG STREQUAL "kyber")
    list(APPEND SUITE_SRCS ${GM_DIR}/t_kyber.c ${GM_DIR}/k_kyber.c ${GM_DIR}/gmssl_sha3_ref.c)
    set(SUITE_DEFS GMSSL_WITH_KYBER=1)
else()
    message(FATAL_ERROR "GMSSL_ALG must be lms, xmss, sphincs or kyber")
endif()
set(SUITE_INCS ${GM_DIR} ${GMSSL}/include ${GMSSL})
set(SUITE_COPTS -Wno-unused-function -Wno-unused-variable)
if(NOT PQC_HOST)
    # GmSSL is not warning-free with the ESP-IDF compiler settings.
    list(APPEND SUITE_COPTS -Wno-format -Wno-maybe-uninitialized -Wno-array-parameter)
endif()
set(SUITE_DEVICE_SRCS ${GMSSL_DEVICE_SRCS})
