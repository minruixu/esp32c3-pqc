# PQMagic's ML-DSA-87 (SM3 and SHAKE). Same sources as suites/pqmagic_mldsa;
# the reference code needs about 123 KB of stack, more than the largest heap
# block of the RAM image, so this image reserves its own memory for it.
set(PQM_MLDSA_MODES 87)
include(${CMAKE_CURRENT_LIST_DIR}/../../pqmagic_mldsa/pqc_suite/suite.cmake)
list(APPEND SUITE_DEFS KEMSIG_EXTRA_HEAP=133120)
