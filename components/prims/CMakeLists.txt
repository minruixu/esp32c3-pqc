include(${CMAKE_CURRENT_LIST_DIR}/prims.cmake)
idf_component_register(SRCS ${PRIMS_SRCS} ${PRIMS_DEVICE_SRCS}
                       INCLUDE_DIRS ${PRIMS_INCS}
                       PRIV_REQUIRES hal soc esp_hw_support)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error)
