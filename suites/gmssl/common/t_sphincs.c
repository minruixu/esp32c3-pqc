/* GmSSL's tests/sphincstest.c, unchanged, with its main() renamed. The whole
 * program runs on the host; the board runs the quick subset chosen below. */
#define main gmssl_sphincstest_main
#include "tests/sphincstest.c"
#undef main

int gmssl_sphincstest_main(void);

int gmssl_sphincstest_quick(void)
{
    if (test_sphincs_wots_derive_sk() != 1) return 1;
    if (test_sphincs_wots_chain() != 1) return 2;
    if (test_sphincs_wots_sk_to_pk() != 1) return 3;
    if (test_sphincs_wots_pk_to_root() != 1) return 4;
    if (test_sphincs_wots_sign() != 1) return 5;
    if (test_sphincs_wots_sig_to_pk() != 1) return 6;
    if (test_sphincs_wots_sign_verify() != 1) return 7;
    return 0;
}
