/* GmSSL's tests/kybertest.c, unchanged, with its main() renamed. The whole
 * program runs on the host; the board runs the quick subset chosen below. */
#define main gmssl_kybertest_main
#include "tests/kybertest.c"
#undef main

int gmssl_kybertest_main(void);

int gmssl_kybertest_quick(void)
{
    return gmssl_kybertest_main(); /* the whole program is quick */
}
