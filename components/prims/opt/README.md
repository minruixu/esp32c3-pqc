Optimized ESP32-C3 backends of the shared primitives. Every `.c` and `.S`
file here is built into the device image (not into the host build). A file
that defines `pqc_keccakf1600_opt`, `pqc_sha256_compress_opt` or
`pqc_sm3_compress_opt` replaces the weak default in `../prims.c`.
