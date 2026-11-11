idf_component_register(SRCS "main.c" "kem_test.c" "dsa_test.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES pqc esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable -Wno-unused-const-variable)
