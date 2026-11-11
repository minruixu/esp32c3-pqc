/* HQC-5, symbols prefixed hqc5o_: the ESP32-C3 build on the device.
 * The host build puts the reference code here instead, so that
 * `tools/pqc check` compares the device's optimized code with an
 * independent implementation. Build the host with -D HQC_HOST_OPT=1 to run
 * the optimized C code there too (useful when changing it). */
#define HQC_LEVEL 5
#define HQC_NS(x) hqc5o_##x
#if defined(PQC_HOST) && !defined(HQC_HOST_OPT)
#include "hqc_unit_ref.inc"
#else
#include "hqc_unit_opt.inc"
#endif
