/* GmSSL's tests/xmsstest.c, unchanged, with its main() renamed. The whole
 * program runs on the host; the board runs the quick subset chosen below. */
#define main gmssl_xmsstest_main
#include "tests/xmsstest.c"
#undef main

int gmssl_xmsstest_main(void);

int gmssl_xmsstest_quick(void)
{
    if (test_xmss_adrs() != 1) return 1;
    if (test_xmss_build_root() != 1) return 2;
    if (test_xmss_private_key_size() != 1) return 3;
    if (test_xmss_signature_size() != 1) return 4;
    if (test_xmssmt_index_to_bytes() != 1) return 5;
    if (test_xmssmt_signature_size() != 1) return 6;
    if (test_xmssmt_private_key_size() != 1) return 7;
    return 0;
}
