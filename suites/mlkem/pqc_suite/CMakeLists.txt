include(${CMAKE_CURRENT_LIST_DIR}/suite.cmake)
idf_component_register(SRCS ${SUITE_SRCS} ${SUITE_DEVICE_SRCS}
                       INCLUDE_DIRS ${SUITE_INCS}
                       PRIV_REQUIRES bench prims freertos riscv esp_system)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${SUITE_DEFS})
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error ${SUITE_COPTS})
# The C code of the optimized variant is built with -O3: measured 1.5 % (key
# generation) to 4 % (encapsulation, decapsulation) faster than -O2 on the
# ESP32-C3. The reference variant keeps the project default.
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/mlkem_opt.c ${CMAKE_CURRENT_LIST_DIR}/c3_sponge.c
                            PROPERTIES COMPILE_OPTIONS "-O3")
