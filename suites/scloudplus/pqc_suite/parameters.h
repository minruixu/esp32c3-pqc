/* SCloud+ core code includes "parameters.h" and expects the build to point
 * at one security level. Here the level comes from PQM_SC_LEVEL, so that one
 * image can hold several levels. */
#if PQM_SC_LEVEL == 128
#include "Reference_Implementation/Scloudplus-128/kem/parameters.h"
#elif PQM_SC_LEVEL == 192
#include "Reference_Implementation/Scloudplus-192/kem/parameters.h"
#elif PQM_SC_LEVEL == 256
#include "Reference_Implementation/Scloudplus-256/kem/parameters.h"
#elif PQM_SC_LEVEL == 384
#include "Reference_Implementation/Scloudplus-384/kem/parameters.h"
#elif PQM_SC_LEVEL == 512
#include "Reference_Implementation/Scloudplus-512/kem/parameters.h"
#else
#error "PQM_SC_LEVEL must be 128, 192, 256, 384 or 512"
#endif
