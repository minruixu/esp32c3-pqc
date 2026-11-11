#include "bench.h"

void app_main(void)
{
    bench_begin();
    suite_main();
    bench_end();
}
