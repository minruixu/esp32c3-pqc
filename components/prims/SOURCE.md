# Sources

- `fips202.c`, `include/fips202.h`, `sha2.c`, `include/sha2.h`: PQClean
  `common/` (public domain, see the headers of the files), from
  https://github.com/PQClean/PQClean. Changed here so that the hash contexts
  hold their state inline instead of on the heap, and so that Keccak-f[1600]
  and the SHA-256 compression function go through `pqc_prims.h`.
- `prims.c`, `sm3.c` and the `pqc_*.h` headers were written for this project.
