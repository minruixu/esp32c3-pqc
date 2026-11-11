include(${CMAKE_CURRENT_LIST_DIR}/prims.cmake)
# WHOLE_ARCHIVE: the optimized backends in opt/ replace weak defaults in
# prims.c. Without it the linker never pulls an object out of the archive
# just to override a weak symbol, and every suite would silently keep the
# reference code. Unused functions are still dropped by --gc-sections.
idf_component_register(SRCS ${PRIMS_SRCS} ${PRIMS_DEVICE_SRCS}
                       INCLUDE_DIRS ${PRIMS_INCS}
                       PRIV_REQUIRES hal soc esp_hw_support
                       WHOLE_ARCHIVE)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error)
