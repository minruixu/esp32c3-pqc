# Reference code of HQC-3 only, run on a large static stack (132 KB are
# needed, more than any heap block). The sources and all settings are those
# of suites/hqc.
set(HQC_REF3_ONLY 1)
include(${CMAKE_CURRENT_LIST_DIR}/../../hqc/pqc_suite/suite.cmake)
