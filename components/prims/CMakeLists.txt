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

# A suite that is short of RAM can leave optimized backends out of its image
# (about 4 KB for keccak, 2 KB for sha256, 9 KB for sm3): one name per line in
# suites/<suite>/pqc_suite/prims_off.txt. PQC_PRIMS_OPT then selects the
# reference code for those primitives.
set(_prims_off "${CMAKE_SOURCE_DIR}/suites/${PQC_SUITE}/pqc_suite/prims_off.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_prims_off}")
if(EXISTS "${_prims_off}")
    file(STRINGS "${_prims_off}" _prims_off_names)
    foreach(_name ${_prims_off_names})
        target_compile_definitions(${COMPONENT_LIB} PRIVATE PQC_PRIMS_OFF_${_name})
    endforeach()
endif()
