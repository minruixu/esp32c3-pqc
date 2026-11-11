/* GmSSL's tests/lmstest.c, unchanged, with its main() renamed. The whole
 * program runs on the host; the board runs the quick subset chosen below. */
#define main gmssl_lmstest_main
#include "tests/lmstest.c"
#undef main

int gmssl_lmstest_main(void);

int gmssl_lmstest_quick(void)
{
    if (test_sm3_hss_kat() != 1) return 1;
    if (test_sm3_lmots() != 1) return 2;
    if (test_lms_signature_size() != 1) return 3;
    if (test_hss_signature_size() != 1) return 4;
    return 0;
}
