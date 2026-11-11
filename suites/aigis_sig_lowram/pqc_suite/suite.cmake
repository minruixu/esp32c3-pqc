# Aigis-sig with the low-stack code as its "opt" variant: the public matrix
# is expanded one row at a time (suites/aigis_sig/pqc_suite/aigis_sig_lowram.inc).
# Same sources as suites/aigis_sig, where "opt" is the upstream code with the
# optimized primitives.
set(AIGIS_SIG_LOWRAM ON)
include(${CMAKE_CURRENT_LIST_DIR}/../../aigis_sig/pqc_suite/suite.cmake)
