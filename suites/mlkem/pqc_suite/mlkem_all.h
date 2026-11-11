/* Public API of all three ML-KEM parameter sets: mlkem512_*, mlkem768_*, mlkem1024_*. */
#ifndef MLKEM_ALL_H
#define MLKEM_ALL_H

#define MLK_CONFIG_PARAMETER_SET 512
#include <mlkem_native.h>
#undef MLK_CONFIG_PARAMETER_SET
#undef MLK_H

#define MLK_CONFIG_PARAMETER_SET 768
#include <mlkem_native.h>
#undef MLK_CONFIG_PARAMETER_SET
#undef MLK_H

#define MLK_CONFIG_PARAMETER_SET 1024
#include <mlkem_native.h>
#undef MLK_CONFIG_PARAMETER_SET
#undef MLK_H

#endif
