# Registers the ESP-IDF component of a suite from the variables its
# suite.cmake set. Included by the CMakeLists.txt of the suites that use the
# helpers in this directory.
idf_component_register(SRCS ${SUITE_SRCS} ${SUITE_DEVICE_SRCS}
                       INCLUDE_DIRS ${SUITE_INCS}
                       PRIV_REQUIRES bench prims heap hal soc esp_hw_support)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${SUITE_DEFS})
# uint32_t is "unsigned long" with this compiler and "unsigned int" in much
# upstream code; GCC 14 would reject passing one pointer type for the other.
# The suites compile upstream code as it is; its warnings are not ours to fix.
# -fno-strict-aliasing: with this toolchain int32_t is "long" and uint32_t is
# "unsigned long". Upstream code often passes int* or unsigned int* where the
# callee takes int32_t* or uint32_t*; under strict aliasing GCC may then drop
# a store it believes nobody reads (Aigis-Sig+ crashed that way).
target_compile_options(${COMPONENT_LIB} PRIVATE -w -fpermissive -fno-strict-aliasing ${SUITE_COPTS})

# The optimized primitives are always in the image: components/prims is
# linked with WHOLE_ARCHIVE.
