# SCloud+-256, SM3 family only. In one image with the SHAKE and AES families
# (suites/scloudplus_256) the submission's SM3 helper cannot allocate its
# buffers and then returns wrong hashes; alone, it has the heap it needs.
set(SC_LEVELS 256)
set(SC_FAMILIES sm3)
set(SC_ARENA 57344)
include(${CMAKE_CURRENT_LIST_DIR}/../../scloudplus/pqc_suite/suite.cmake)
