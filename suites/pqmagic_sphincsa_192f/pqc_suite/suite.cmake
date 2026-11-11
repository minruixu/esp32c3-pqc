# SPHINCS-alpha 192f (SHA-2 and SHAKE). Same sources as
# suites/pqmagic_sphincsa; its 128 KB encoding table needs the heap of the image to itself.
set(SPX_SETS sha2_192f shake_192f)
set(SPX_ARENA 35840)
include(${CMAKE_CURRENT_LIST_DIR}/../../pqmagic_sphincsa/pqc_suite/suite.cmake)
