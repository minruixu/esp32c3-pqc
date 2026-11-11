include(${CMAKE_CURRENT_LIST_DIR}/suite.cmake)
idf_component_register(SRCS ${SUITE_SRCS} ${SUITE_DEVICE_SRCS}
                       INCLUDE_DIRS ${SUITE_INCS}
                       PRIV_REQUIRES bench prims heap
                       WHOLE_ARCHIVE)  # the hash shim is referenced only from the candidate library
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${SUITE_DEFS})
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error ${SUITE_COPTS})

# The candidate's code: a plain static library next to the component, so it
# does not see this project's headers and only the objects that are needed
# get linked.
ngcc_add_candidate_library()
target_link_libraries(ngcc_cand PRIVATE idf::newlib)
target_link_libraries(${COMPONENT_LIB} PRIVATE ngcc_cand)
