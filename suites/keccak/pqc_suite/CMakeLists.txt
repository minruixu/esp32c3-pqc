include(${CMAKE_CURRENT_LIST_DIR}/suite.cmake)
idf_component_register(SRCS ${SUITE_SRCS} ${SUITE_DEVICE_SRCS}
                       INCLUDE_DIRS ${SUITE_INCS}
                       PRIV_REQUIRES bench prims esp_hw_support freertos)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${SUITE_DEFS})
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error ${SUITE_COPTS})
