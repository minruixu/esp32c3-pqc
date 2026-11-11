# NEV with SM3 as the hash (the configuration of the NGCC submission's
# known-answer tests, USE_ICCS upstream). Same sources as suites/pqmagic_nev.
set(PQM_NEV_SM3 ON)
include(${CMAKE_CURRENT_LIST_DIR}/../../pqmagic_nev/pqc_suite/suite.cmake)
