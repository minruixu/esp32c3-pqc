# SCloud+-256 (SHAKE and AES families). Same sources as suites/scloudplus;
# its keys and working matrices need the image to itself. The SM3 family is
# in suites/scloudplus_256_sm3: here it would not have enough heap.
set(SC_LEVELS 256)
set(SC_FAMILIES shake aes)
set(SC_ARENA 57344)
include(${CMAKE_CURRENT_LIST_DIR}/../../scloudplus/pqc_suite/suite.cmake)
