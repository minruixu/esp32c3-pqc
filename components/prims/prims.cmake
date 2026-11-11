# Source list of the shared primitives, used by the device build
# (CMakeLists.txt next to this file) and by the host build (host/).
set(PRIMS_DIR ${CMAKE_CURRENT_LIST_DIR})
set(PRIMS_SRCS
    ${PRIMS_DIR}/prims.c
    ${PRIMS_DIR}/fips202.c
    ${PRIMS_DIR}/sha2.c
    ${PRIMS_DIR}/sm3.c)
set(PRIMS_INCS ${PRIMS_DIR}/include)
# Optimized backends, device only: every .c and .S file in opt/ is built.
file(GLOB PRIMS_DEVICE_SRCS CONFIGURE_DEPENDS ${PRIMS_DIR}/opt/*.c ${PRIMS_DIR}/opt/*.S)
