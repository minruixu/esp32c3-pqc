# SCloud+-256 (SM3, SHAKE and AES families). Same sources as suites/scloudplus;
# its keys and working matrices need the image to itself.
set(SC_LEVELS 256)
set(SC_ARENA 57344)
include(${CMAKE_CURRENT_LIST_DIR}/../../scloudplus/pqc_suite/suite.cmake)
