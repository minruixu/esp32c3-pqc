include(${CMAKE_CURRENT_LIST_DIR}/suite.cmake)
# components/prims/opt holds sha256_hw.h, the register interface of the SHA
# accelerator, which the device-only sources of this suite use directly.
idf_component_register(SRCS ${SUITE_SRCS} ${SUITE_DEVICE_SRCS}
                       INCLUDE_DIRS ${SUITE_INCS} ${CMAKE_CURRENT_LIST_DIR}/../../../components/prims/opt
                       PRIV_REQUIRES bench prims esp_hw_support hal soc)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${SUITE_DEFS})
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error ${SUITE_COPTS})
