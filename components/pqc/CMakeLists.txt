# ML-KEM-768 (FIPS 203) and ML-DSA-65 (FIPS 204), portable C from
# pq-code-package/mlkem-native and mldsa-native, built as one unit each.
idf_component_register(
    SRCS "mlkem/mlkem_native.c" "mldsa/mldsa_native.c" "pqc_rng.c"
    INCLUDE_DIRS "mlkem" "mldsa"
    PRIV_REQUIRES esp_hw_support)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    MLK_CONFIG_PARAMETER_SET=768 MLK_CONFIG_NAMESPACE_PREFIX=mlkem
    MLD_CONFIG_PARAMETER_SET=65  MLD_CONFIG_NAMESPACE_PREFIX=mldsa)

if(PQC_REDUCE_RAM)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC MLD_CONFIG_REDUCE_RAM)
endif()

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error)
