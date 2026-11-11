# Summary

One row per algorithm: `reference → optimized (speedup)`, or a single time where the two
do not differ. ESP32-C3 at 160 MHz, images loaded into RAM. The last column is the largest
stack plus heap any of the three operations needs in the optimized variant; keys, ciphertexts
and signatures held by the caller come on top. Full tables: [RESULTS.md](RESULTS.md).

### NIST standards and selections

**ML-KEM (FIPS 203)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| mlkem512 | 4.91 ms → 2.80 ms (1.8×) | 5.70 ms → 2.96 ms (1.9×) | 7.13 ms → 3.70 ms (1.9×) | 10.9 KB |
| mlkem768 | 8.21 ms → 4.48 ms (1.8×) | 9.03 ms → 4.85 ms (1.9×) | 11.0 ms → 5.90 ms (1.9×) | 15.7 KB |
| mlkem1024 | 12.4 ms → 7.02 ms (1.8×) | 13.5 ms → 7.45 ms (1.8×) | 16.1 ms → 8.81 ms (1.8×) | 21.8 KB |

**ML-DSA (FIPS 204)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| mldsa44 | 19.2 ms → 10.5 ms (1.8×) | 67.0 ms → 32.2 ms (2.1×) | 20.0 ms → 10.7 ms (1.9×) | 45.5 KB |
| mldsa65 | 33.1 ms → 18.6 ms (1.8×) | 94.4 ms → 45.2 ms (2.1×) | 33.4 ms → 18.2 ms (1.8×) | 68.5 KB |
| mldsa87 | 56.0 ms → 31.6 ms (1.8×) | 128 ms → 62.2 ms (2.1×) | 56.3 ms → 31.3 ms (1.8×) | 111.5 KB |

**ML-DSA, low-RAM build of mldsa-native**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| mldsa44 | 10.8 ms | 66.6 ms | 11.0 ms | 11.5 KB |
| mldsa65 | 19.2 ms | 103 ms | 18.7 ms | 13.5 KB |
| mldsa87 | 32.5 ms | 143 ms | 32.3 ms | 15.8 KB |

**SLH-DSA (FIPS 205)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| slhdsa-sha2-128f | 130 ms → 5.64 ms (23.1×) | 3057 ms → 143 ms (21.4×) | 177 ms → 9.79 ms (18.1×) | 4.2 KB |
| slhdsa-sha2-128s | 8298 ms → 360 ms (23.0×) | 63.5 s → 2890 ms (22.0×) | 60.6 ms → 3.57 ms (17.0×) | 4.3 KB |
| slhdsa-sha2-192f | 195 ms → 16.0 ms (12.2×) | 5587 ms → 1013 ms (5.5×) | 309 ms → 56.5 ms (5.5×) | 5.4 KB |
| slhdsa-sha2-192s | 12.5 s → 1022 ms (12.2×) | 130.5 s → 28.9 s (4.5×) | 112 ms → 32.8 ms (3.4×) | 5.4 KB |
| slhdsa-sha2-256f | 523 ms → 51.8 ms (10.1×) | 11.7 s → 2303 ms (5.1×) | 322 ms → 69.5 ms (4.6×) | 6.4 KB |
| slhdsa-sha2-256s | 8373 ms → 828 ms (10.1×) | 123.2 s → 35.2 s (3.5×) | 173 ms → 47.7 ms (3.6×) | 6.4 KB |
| slhdsa-shake-128f | 546 ms → 363 ms (1.5×) | 12.8 s → 8482 ms (1.5×) | 760 ms → 506 ms (1.5×) | 4.1 KB |
| slhdsa-shake-128s | 34.9 s → 23.3 s (1.5×) | 265.8 s → 176.7 s (1.5×) | 272 ms → 181 ms (1.5×) | 4.1 KB |
| slhdsa-shake-192f | 801 ms → 531 ms (1.5×) | 20.7 s → 13.7 s (1.5×) | 1112 ms → 739 ms (1.5×) | 4.1 KB |
| slhdsa-shake-192s | 51.3 s → 34.0 s (1.5×) | 463.1 s → 306.2 s (1.5×) | 380 ms → 252 ms (1.5×) | 4.1 KB |
| slhdsa-shake-256f | 2124 ms → 1402 ms (1.5×) | 42.8 s → 28.2 s (1.5×) | 1113 ms → 736 ms (1.5×) | 4.1 KB |
| slhdsa-shake-256s | 34.0 s → 22.4 s (1.5×) | 408.3 s → 267.9 s (1.5×) | 561 ms → 370 ms (1.5×) | 4.1 KB |

**FN-DSA / Falcon (FIPS 206 draft)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| fndsa512 | 522 ms | 289 ms → 219 ms (1.3×) | 4.10 ms → 3.21 ms (1.3×) | 2.2 KB |
| fndsa1024 | 1690 ms | 623 ms → 473 ms (1.3×) | 8.23 ms → 6.50 ms (1.3×) | 2.3 KB |

**HQC (selected 2025, reference v5.1.0)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| hqc1 | 281 ms → 4.29 ms (65.4×) | 561 ms → 9.11 ms (61.5×) | 1132 ms → 22.3 ms (50.8×) | 19.8 KB |
| hqc3 | 849 ms → 10.4 ms (81.8×) | 1695 ms → 22.7 ms (74.8×) | 3405 ms → 50.6 ms (67.3×) | 38.3 KB |
| hqc5 | 19.4 ms | 43.3 ms | 95.7 ms | 60.0 KB |

**LMS and HSS (SP 800-208)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| hss-sha256-h5w8-h5w8 | 21.9 s → 1275 ms (17.2×) | 441 ms → 25.9 ms (17.1×) | 379 ms → 22.3 ms (17.0×) | 2.1 KB |
| kat-gmssl_sm3_hss |  |  | 617 ms → 390 ms (1.6×) | 1.5 KB |
| kat-rfc8554_tc1 |  |  | 1048 ms → 61.9 ms (16.9×) | 1.3 KB |
| kat-rfc8554_tc2 |  |  | 419 ms → 21.4 ms (19.6×) | 1.2 KB |
| kat-rfc9858_tc1 |  |  | 386 ms → 14.4 ms (26.8×) | 1.2 KB |
| kat-rfc9858_tc2 |  |  | 1659 ms → 814 ms (2.0×) | 1.2 KB |
| kat-rfc9858_tc3 |  |  | 2262 ms → 1112 ms (2.0×) | 1.2 KB |
| kat-rfc9858_tc4 |  |  | 56.2 ms → 4.40 ms (12.8×) | 1.1 KB |
| lms-sha256-m24-h5-w8 | 8234 ms → 299 ms (27.5×) | 125 ms → 4.69 ms (26.7×) | 139 ms → 5.14 ms (26.9×) | 1.9 KB |
| lms-sha256-m24-h10-w4 | 33.0 s → 2189 ms (15.1×) | 34.3 ms → 2.48 ms (13.8×) | 16.5 ms → 1.27 ms (13.0×) | 2.4 KB |
| lms-sha256-m32-h5-w1 | 804 ms → 152 ms (5.3×) | 15.8 ms → 3.00 ms (5.3×) | 10.8 ms → 2.50 ms (4.3×) | 1.8 KB |
| lms-sha256-m32-h5-w2 | 737 ms → 95.7 ms (7.7×) | 13.0 ms → 1.96 ms (6.6×) | 11.3 ms → 1.77 ms (6.4×) | 1.8 KB |
| lms-sha256-m32-h5-w4 | 1374 ms → 87.8 ms (15.7×) | 21.6 ms → 1.66 ms (13.0×) | 23.2 ms → 1.68 ms (13.8×) | 1.9 KB |
| lms-sha256-m32-h5-w8 | 10.9 s → 473 ms (23.0×) | 161 ms → 7.19 ms (22.4×) | 209 ms → 9.21 ms (22.7×) | 1.9 KB |
| lms-sha256-m32-h10-w4 | 44.0 s → 3376 ms (13.0×) | 45.5 ms → 3.75 ms (12.2×) | 20.7 ms → 1.85 ms (11.2×) | 2.4 KB |
| lms-sha256-m32-h10-w8 | 20.2 s | 13.5 ms | 11.2 ms | 2.4 KB |
| lms-shake-m24-h5-w4 | 4404 ms → 2169 ms (2.0×) | 72.7 ms → 35.8 ms (2.0×) | 62.7 ms → 31.1 ms (2.0×) | 1.9 KB |
| lms-shake-m32-h5-w4 | 5816 ms → 2873 ms (2.0×) | 95.6 ms → 47.1 ms (2.0×) | 86.4 ms → 43.0 ms (2.0×) | 1.9 KB |
| lms-sm3-m32-h5-w8 | 12.0 s → 7573 ms (1.6×) | 182 ms → 115 ms (1.6×) | 198 ms → 125 ms (1.6×) | 2.1 KB |
| lms-sm3-m32-h10-w8 | 242.3 s | 161 ms | 140 ms | 2.7 KB |

**XMSS and XMSS^MT (SP 800-208)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| xmss-sha2_10_192 | 152.6 s → 11.1 s (13.8×) | 328 ms → 21.3 ms (15.4×) | 78.3 ms → 5.56 ms (14.1×) | 3.1 KB |
| xmss-sha2_10_256 | 235.9 s → 13.0 s (18.1×) | 511 ms → 25.7 ms (19.9×) | 130 ms → 7.10 ms (18.2×) | 4.1 KB |
| xmss-shake256_10_256 | 293.5 s | 571 ms | 150 ms | 4.5 KB |
| xmssmt-sha2_20-4_192 | 19.3 s → 1406 ms (13.7×) | 592 ms → 33.0 ms (18.0×) | 336 ms → 24.0 ms (14.0×) | 3.4 KB |
| xmssmt-sha2_20-4_256 | 29.8 s → 1658 ms (18.0×) | 918 ms → 39.9 ms (23.0×) | 476 ms → 26.3 ms (18.1×) | 4.3 KB |
| xmssmt-shake256_20-4_192 | 55.9 s → 27.5 s (2.0×) | 1701 ms → 626 ms (2.7×) | 950 ms → 468 ms (2.0×) | 3.9 KB |
| xmssmt-shake256_20-4_256 | 74.4 s → 37.1 s (2.0×) | 2271 ms → 846 ms (2.7×) | 1185 ms → 591 ms (2.0×) | 4.8 KB |

### Chinese algorithms and SM3 instantiations

**Aigis-enc (PQMagic)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| aigis-enc-1-shake | 5.99 ms → 4.32 ms (1.4×) | 10.3 ms → 7.26 ms (1.4×) | 10.2 ms → 7.54 ms (1.4×) | 11.0 KB |
| aigis-enc-1-sm3 | 6.49 ms → 4.81 ms (1.3×) | 10.5 ms → 7.66 ms (1.4×) | 10.9 ms → 8.21 ms (1.3×) | 11.7 KB |
| aigis-enc-2-shake | 8.97 ms → 6.52 ms (1.4×) | 12.6 ms → 9.10 ms (1.4×) | 12.5 ms → 9.46 ms (1.3×) | 14.7 KB |
| aigis-enc-2-sm3 | 9.79 ms → 7.38 ms (1.3×) | 12.8 ms → 9.65 ms (1.3×) | 13.3 ms → 10.4 ms (1.3×) | 15.3 KB |
| aigis-enc-3-shake | 9.19 ms → 6.68 ms (1.4×) | 12.8 ms → 9.30 ms (1.4×) | 12.6 ms → 9.59 ms (1.3×) | 14.8 KB |
| aigis-enc-3-sm3 | 10.1 ms → 7.59 ms (1.3×) | 13.1 ms → 9.86 ms (1.3×) | 13.6 ms → 10.6 ms (1.3×) | 15.5 KB |
| aigis-enc-4-shake | 16.3 ms → 11.6 ms (1.4×) | 21.9 ms → 15.5 ms (1.4×) | 21.5 ms → 15.9 ms (1.4×) | 20.8 KB |
| aigis-enc-4-sm3 | 18.1 ms → 13.4 ms (1.4×) | 23.1 ms → 17.0 ms (1.4×) | 23.6 ms → 17.8 ms (1.3×) | 21.5 KB |

**Aigis-sig (PQMagic)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| aigis-sig-1-shake | 17.4 ms → 12.1 ms (1.4×) | 83.0 ms → 69.4 ms (1.2×) | 17.3 ms → 12.6 ms (1.4×) | 64.2 KB |
| aigis-sig-1-sm3 | 23.7 ms → 16.3 ms (1.5×) | 66.2 ms → 51.4 ms (1.3×) | 23.9 ms → 17.0 ms (1.4×) | 64.4 KB |
| aigis-sig-2-shake | 26.9 ms → 18.5 ms (1.5×) | 35.7 ms → 27.6 ms (1.3×) | 26.5 ms → 18.9 ms (1.4×) | 85.4 KB |
| aigis-sig-2-sm3 | 37.3 ms → 25.4 ms (1.5×) | 66.8 ms → 51.0 ms (1.3×) | 37.2 ms → 26.1 ms (1.4×) | 85.6 KB |
| aigis-sig-3-shake | 38.2 ms → 26.2 ms (1.5×) | 70.9 ms → 55.7 ms (1.3×) | 37.6 ms → 26.5 ms (1.4×) | 108.4 KB |
| aigis-sig-3-sm3 | 53.7 ms → 36.4 ms (1.5×) | 70.4 ms → 51.6 ms (1.4×) | 53.9 ms → 37.4 ms (1.4×) | 108.8 KB |

**Aigis-sig, low-stack build**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| aigis-sig-1-shake | 17.5 ms → 12.1 ms (1.4×) | 82.0 ms → 126 ms (0.7×) | 17.4 ms → 12.7 ms (1.4×) | 55.1 KB |
| aigis-sig-1-sm3 | 23.7 ms → 16.3 ms (1.5×) | 65.9 ms → 83.4 ms (0.8×) | 23.9 ms → 17.0 ms (1.4×) | 55.5 KB |
| aigis-sig-2-shake | 27.0 ms → 18.6 ms (1.4×) | 35.7 ms → 27.6 ms (1.3×) | 26.7 ms → 19.1 ms (1.4×) | 69.3 KB |
| aigis-sig-2-sm3 | 37.3 ms → 25.4 ms (1.5×) | 68.9 ms | 37.2 ms → 26.1 ms (1.4×) | 69.6 KB |
| aigis-sig-3-shake | 38.5 ms → 26.4 ms (1.5×) | 70.7 ms → 91.6 ms (0.8×) | 37.9 ms → 26.8 ms (1.4×) | 83.3 KB |
| aigis-sig-3-sm3 | 53.7 ms → 36.4 ms (1.5×) | 70.3 ms → 51.5 ms (1.4×) | 53.9 ms → 37.3 ms (1.4×) | 83.9 KB |

**ML-KEM with SM3 (PQMagic)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| mlkem512-shake-pqmagic | 5.45 ms → 3.95 ms (1.4×) | 5.91 ms → 4.46 ms (1.3×) | 6.81 ms → 5.37 ms (1.3×) | 10.0 KB |
| mlkem512-sm3-pqmagic | 5.69 ms → 4.13 ms (1.4×) | 6.33 ms → 4.75 ms (1.3×) | 7.21 ms → 5.63 ms (1.3×) | 10.7 KB |
| mlkem768-shake-pqmagic | 8.94 ms → 6.54 ms (1.4×) | 9.79 ms → 7.34 ms (1.3×) | 11.0 ms → 8.52 ms (1.3×) | 14.8 KB |
| mlkem768-sm3-pqmagic | 10.3 ms → 7.42 ms (1.4×) | 11.2 ms → 8.26 ms (1.4×) | 12.3 ms → 9.39 ms (1.3×) | 15.9 KB |
| mlkem1024-shake-pqmagic | 14.1 ms → 10.3 ms (1.4×) | 15.1 ms → 11.2 ms (1.3×) | 16.6 ms → 12.7 ms (1.3×) | 20.8 KB |
| mlkem1024-sm3-pqmagic | 16.8 ms → 12.0 ms (1.4×) | 17.9 ms → 13.0 ms (1.4×) | 19.4 ms → 14.5 ms (1.3×) | 22.3 KB |

**ML-DSA with SM3 (PQMagic)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| mldsa44-shake-pqmagic | 20.2 ms → 14.4 ms (1.4×) | 30.7 ms → 24.5 ms (1.2×) | 21.3 ms → 15.8 ms (1.3×) | 52.1 KB |
| mldsa44-sm3-pqmagic | 30.3 ms → 21.3 ms (1.4×) | 86.1 ms → 68.9 ms (1.2×) | 27.7 ms → 20.1 ms (1.4×) | 52.9 KB |
| mldsa65-shake-pqmagic | 35.4 ms → 24.9 ms (1.4×) | 97.3 ms → 81.4 ms (1.2×) | 35.7 ms → 26.0 ms (1.4×) | 79.1 KB |
| mldsa65-sm3-pqmagic | 53.0 ms → 36.9 ms (1.4×) | 88.1 ms → 68.7 ms (1.3×) | 48.6 ms → 34.6 ms (1.4×) | 79.9 KB |
| mldsa87-shake-pqmagic | 59.8 ms | 78.0 ms | 61.0 ms | 121.2 KB |
| mldsa87-sm3-pqmagic | 89.3 ms → 61.8 ms (1.4×) | 109 ms → 81.1 ms (1.3×) | 85.0 ms → 59.9 ms (1.4×) | 122.2 KB |

**SLH-DSA with SM3 (PQMagic)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| slhdsa-sm3-128f-pqmagic | 199 ms → 141 ms (1.4×) | 4742 ms → 3348 ms (1.4×) | 293 ms → 206 ms (1.4×) | 3.3 KB |
| slhdsa-sm3-128f-robust-pqmagic | 382 ms → 268 ms (1.4×) | 8951 ms → 6279 ms (1.4×) | 569 ms → 399 ms (1.4×) | 4.2 KB |
| slhdsa-sm3-128s-pqmagic | 12.7 s → 9000 ms (1.4×) | 99.1 s → 69.9 s (1.4×) | 106 ms → 74.4 ms (1.4×) | 3.7 KB |
| slhdsa-sm3-128s-robust-pqmagic | 24.4 s → 17.1 s (1.4×) | 185.8 s → 130.3 s (1.4×) | 209 ms → 147 ms (1.4×) | 4.4 KB |

**SPHINCS-alpha (PQMagic)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| sphincs-a-sha2-128f | 173 ms → 63.7 ms (2.7×) | 4138 ms → 1336 ms (3.1×) | 222 ms → 78.8 ms (2.8×) | 2.9 KB |
| sphincs-a-sha2-128s | 8088 ms → 2606 ms (3.1×) | 83.8 s → 26.6 s (3.1×) | 304 ms → 107 ms (2.8×) | 105.6 KB |
| sphincs-a-shake-128f | 643 ms → 416 ms (1.5×) | 16.2 s → 10.4 s (1.6×) | 832 ms → 537 ms (1.6×) | 3.5 KB |
| sphincs-a-shake-128s | 31.9 s → 20.4 s (1.6×) | 332.2 s → 212.5 s (1.6×) | 1157 ms → 746 ms (1.6×) | 106.0 KB |
| sphincs-a-sm3-128f | 193 ms → 140 ms (1.4×) | 4891 ms → 3448 ms (1.4×) | 259 ms → 186 ms (1.4×) | 3.4 KB |
| sphincs-a-sm3-128s | 9092 ms → 6436 ms (1.4×) | 98.1 s → 69.1 s (1.4×) | 348 ms → 250 ms (1.4×) | 106.0 KB |

**GmSSL: Kyber and SM3-based hash signatures**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| gmssl-hss-sm3-h5-h5 | 17.8 s | 136 ms | 285 ms | 12.0 KB |
| gmssl-kyber768 | 23.7 ms → 9.70 ms (2.4×) | 25.7 ms → 11.5 ms (2.2×) | 29.3 ms → 15.1 ms (1.9×) | 15.5 KB |
| gmssl-lms-sm3-h5 | 8852 ms | 137 ms | 146 ms | 3.3 KB |
| gmssl-sphincs-sm3-128s | 16.0 s |  |  | 0.3 KB |
| gmssl-xmssmt-sm3-20_4 | 24.9 s | 89.4 ms | 388 ms | 28.5 KB |

**LAC**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| lac-128 | 8.78 ms → 4.40 ms (2.0×) | 13.7 ms → 7.64 ms (1.8×) | 17.0 ms → 10.9 ms (1.6×) | 13.5 KB |
| lac-192 | 12.9 ms → 8.39 ms (1.5×) | 19.8 ms → 13.8 ms (1.4×) | 25.5 ms → 19.5 ms (1.3×) | 26.3 KB |
| lac-256 | 18.5 ms → 12.4 ms (1.5×) | 31.4 ms → 23.1 ms (1.4×) | 43.1 ms → 34.7 ms (1.2×) | 26.5 KB |
| lac-light | 4.91 ms → 2.25 ms (2.2×) | 6.97 ms → 3.48 ms (2.0×) | 8.12 ms → 4.64 ms (1.8×) | 13.2 KB |

### NGCC round-1 candidates with open implementations

**Aigis-Enc+ (PQMagic)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| aigis-enc-plus-512 | 4.68 ms → 3.45 ms (1.4×) | 6.45 ms → 4.94 ms (1.3×) | 7.72 ms → 6.11 ms (1.3×) | 12.7 KB |
| aigis-enc-plus-1024 | 8.87 ms → 6.70 ms (1.3×) | 13.3 ms → 10.4 ms (1.3×) | 16.2 ms → 13.2 ms (1.2×) | 26.5 KB |
| aigis-enc-plus-2048 | 21.3 ms → 15.7 ms (1.4×) | 30.6 ms → 23.8 ms (1.3×) | 38.7 ms → 31.1 ms (1.2×) | 56.6 KB |

**Aigis-Sig+ (PQMagic)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| aigis-sig-plus-1 | 11.3 ms → 8.25 ms (1.4×) | 46.8 ms → 38.9 ms (1.2×) | 12.4 ms → 9.63 ms (1.3×) | 75.5 KB |

**NEV (PQMagic)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| nev-512-769 | 2.88 ms → 2.43 ms (1.2×) | 2.68 ms → 2.18 ms (1.2×) | 3.01 ms → 2.73 ms (1.1×) | 6.7 KB |
| nev-512-769-c | 2.87 ms → 2.43 ms (1.2×) | 2.41 ms → 2.02 ms (1.2×) | 3.04 ms → 2.87 ms (1.1×) | 6.5 KB |
| nev-512-769-c-sm3 | 2.63 ms → 2.33 ms (1.1×) | 2.14 ms → 1.89 ms (1.1×) | 2.88 ms | 6.8 KB |
| nev-512-769-sm3 | 2.62 ms → 2.32 ms (1.1×) | 2.30 ms → 1.99 ms (1.2×) | 2.85 ms → 2.67 ms (1.1×) | 7.0 KB |
| nev-512-1409 | 3.99 ms → 3.32 ms (1.2×) | 3.23 ms → 2.56 ms (1.3×) | 3.45 ms → 3.06 ms (1.1×) | 8.0 KB |
| nev-512-1409-sm3 | 3.69 ms → 3.22 ms (1.1×) | 2.91 ms → 2.44 ms (1.2×) | 3.46 ms → 3.13 ms (1.1×) | 8.4 KB |
| nev-512-3329 | 4.56 ms → 3.56 ms (1.3×) | 4.18 ms → 3.18 ms (1.3×) | 4.23 ms → 3.50 ms (1.2×) | 8.6 KB |
| nev-512-3329-sm3 | 4.79 ms → 3.88 ms (1.2×) | 4.41 ms → 3.50 ms (1.3×) | 4.70 ms → 3.96 ms (1.2×) | 9.6 KB |
| nev-1024-769 | 6.66 ms → 5.77 ms (1.2×) | 5.57 ms → 4.57 ms (1.2×) | 6.31 ms → 5.87 ms (1.1×) | 15.6 KB |
| nev-1024-769-c | 6.74 ms → 5.84 ms (1.2×) | 5.26 ms → 4.42 ms (1.2×) | 6.37 ms | 14.9 KB |
| nev-1024-769-c-sm3 | 6.00 ms → 5.46 ms (1.1×) | 4.83 ms → 4.25 ms (1.1×) | 6.62 ms | 15.3 KB |
| nev-1024-769-sm3 | 6.00 ms → 5.46 ms (1.1×) | 5.07 ms → 4.39 ms (1.2×) | 6.57 ms → 6.15 ms (1.1×) | 16.2 KB |
| nev-1024-1409 | 8.67 ms → 7.61 ms (1.1×) | 5.82 ms → 4.76 ms (1.2×) | 6.75 ms → 6.25 ms (1.1×) | 19.6 KB |
| nev-1024-1409-sm3 | 8.14 ms → 7.42 ms (1.1×) | 5.62 ms → 4.81 ms (1.2×) | 7.18 ms → 6.66 ms (1.1×) | 20.0 KB |
| nev-1024-3329 | 8.40 ms → 6.79 ms (1.2×) | 7.01 ms → 5.39 ms (1.3×) | 7.19 ms → 6.24 ms (1.2×) | 15.9 KB |
| nev-1024-3329-sm3 | 7.94 ms → 6.77 ms (1.2×) | 6.89 ms → 5.62 ms (1.2×) | 7.90 ms → 6.97 ms (1.1×) | 17.1 KB |
| nev-2048-769 | 20.8 ms → 17.6 ms (1.2×) | 14.1 ms → 11.2 ms (1.3×) | 14.6 ms → 13.6 ms (1.1×) | 38.1 KB |
| nev-2048-769-c | 20.8 ms → 17.6 ms (1.2×) | 13.2 ms → 10.7 ms (1.2×) | 14.4 ms | 38.0 KB |
| nev-2048-769-c-sm3 | 20.4 ms → 17.9 ms (1.1×) | 12.5 ms → 10.7 ms (1.2×) | 15.1 ms | 43.1 KB |
| nev-2048-769-sm3 | 20.3 ms → 17.9 ms (1.1×) | 13.5 ms → 11.4 ms (1.2×) | 15.6 ms → 14.6 ms (1.1×) | 43.1 KB |
| nev-2048-1409 | 24.4 ms → 21.4 ms (1.1×) | 16.2 ms → 12.7 ms (1.3×) | 16.9 ms → 15.6 ms (1.1×) | 46.3 KB |
| nev-2048-1409-sm3 | 23.2 ms → 21.2 ms (1.1×) | 15.8 ms → 13.1 ms (1.2×) | 18.3 ms → 16.8 ms (1.1×) | 51.8 KB |
| nev-2048-3329 | 23.6 ms → 19.1 ms (1.2×) | 18.0 ms → 13.4 ms (1.3×) | 16.7 ms → 14.6 ms (1.1×) | 39.8 KB |
| nev-2048-3329-sm3 | 23.4 ms → 19.8 ms (1.2×) | 18.1 ms → 14.4 ms (1.3×) | 18.8 ms → 16.4 ms (1.1×) | 46.2 KB |

**NEV-AKE (PQMagic)**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| nev-ake-512-769 | 2.26 ms → 2.04 ms (1.1×) | 4.34 ms → 3.84 ms (1.1×) | 12.7 ms → 9.94 ms (1.3×) | 6.8 KB |
| nev-ake-512-769-c | 2.23 ms → 2.01 ms (1.1×) | 4.04 ms → 3.65 ms (1.1×) | 11.7 ms → 9.42 ms (1.2×) | 6.2 KB |
| nev-ake-512-1409 | 3.19 ms → 2.80 ms (1.1×) | 5.65 ms → 4.87 ms (1.2×) | 14.3 ms → 11.1 ms (1.3×) | 8.1 KB |
| nev-ake-1024-769 | 5.21 ms → 4.87 ms (1.1×) | 9.30 ms → 8.52 ms (1.1×) | 27.4 ms → 21.4 ms (1.3×) | 15.8 KB |
| nev-ake-1024-769-c | 5.20 ms → 4.87 ms (1.1×) | 8.97 ms → 8.36 ms (1.1×) | 26.2 ms → 21.0 ms (1.2×) | 15.0 KB |
| nev-ake-1024-1409 | 7.15 ms → 6.65 ms (1.1×) | 11.5 ms → 10.5 ms (1.1×) | 29.7 ms → 23.1 ms (1.3×) | 19.4 KB |
| nev-ake-2048-769 | 15.7 ms → 14.4 ms (1.1×) | 24.6 ms → 22.5 ms (1.1×) | 78.0 ms → 57.8 ms (1.3×) | 38.1 KB |
| nev-ake-2048-769-c | 15.7 ms → 14.4 ms (1.1×) | 23.8 ms → 22.1 ms (1.1×) | 73.5 ms → 55.7 ms (1.3×) | 38.3 KB |
| nev-ake-2048-1409 | 17.9 ms | 29.0 ms → 27.0 ms (1.1×) | 87.4 ms → 64.5 ms (1.4×) | 46.4 KB |

**SCloud+**

| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Peak stack + heap |
|---|---:|---:|---:|---:|
| scloudplus-128-aes | 1380 ms → 170 ms (8.1×) | 1372 ms → 148 ms (9.3×) | 1364 ms → 145 ms (9.4×) | 32.2 KB |
| scloudplus-128-shake | 712 ms → 438 ms (1.6×) | 704 ms → 418 ms (1.7×) | 697 ms → 415 ms (1.7×) | 32.5 KB |
| scloudplus-128-sm3 | 900 ms → 563 ms (1.6×) | 889 ms → 540 ms (1.6×) | 886 ms → 539 ms (1.6×) | 52.5 KB |
| scloudplus-192-aes | 2752 ms → 406 ms (6.8×) | 2735 ms → 351 ms (7.8×) | 2730 ms → 355 ms (7.7×) | 59.7 KB |
| scloudplus-192-shake | 1506 ms → 922 ms (1.6×) | 1529 ms → 870 ms (1.8×) | 1524 ms → 873 ms (1.7×) | 60.0 KB |
| scloudplus-192-sm3 | 1977 ms → 1146 ms (1.7×) | 2007 ms → 1084 ms (1.9×) | 2009 ms → 1091 ms (1.8×) | 95.9 KB |
| scloudplus-256-aes | 5518 ms → 838 ms (6.6×) | 5571 ms → 712 ms (7.8×) | 5560 ms → 714 ms (7.8×) | 83.3 KB |
| scloudplus-256-shake | 3189 ms → 1786 ms (1.8×) | 3356 ms → 1689 ms (2.0×) | 3344 ms → 1690 ms (2.0×) | 83.6 KB |

