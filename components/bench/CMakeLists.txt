idf_component_register(SRCS "bench_common.c" "bench_esp.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES prims
                       PRIV_REQUIRES esp_timer hal soc heap esp_system esp_rom)
target_compile_definitions(${COMPONENT_LIB} PRIVATE PQC_SUITE_NAME="${PQC_SUITE}")
# Count the heap memory the algorithms take through malloc and friends.
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=malloc" "-Wl,--wrap=calloc" "-Wl,--wrap=realloc" "-Wl,--wrap=free")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error)
