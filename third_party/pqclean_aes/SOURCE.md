# Source

`aes.c` and `aes.h` are `common/aes.c` and `common/aes.h` of PQClean
(https://github.com/PQClean/PQClean, commit 0586a824, 2026-08-04), unchanged.
The code is the constant-time bitsliced AES of BearSSL by Thomas Pornin, MIT
license; the license text is at the top of `aes.c`.

Used as the software AES-256-CTR of the LAC port (`suites/lac`).
