# PQRV

- Source: https://github.com/Ji-Peng/PQRV, branch `ches2025`, commit
  `87440598d6cbd83e3c0471298ebde533c8ab885b` (2025-02-06).
- Paper: Zhang, Yan, Huang, Koç, "Optimized Software Implementation of
  Keccak, Kyber, and Dilithium on RV{32,64}IM{B}{V}", IACR TCHES 2025(1).
- License: MIT (see `LICENSE`; it also lists the public-domain and Apache-2.0
  code PQRV builds on).
- Copied here, unchanged: the two RV32IM Keccak-f[1600] routines in `sha3/`.

Use in this project: `suites/keccak` times PQRV's Keccak-f[1600] on the
ESP32-C3 next to this project's own permutation. The PQRV routines use the
`gp` and `tp` registers as scratch space, which is not safe under FreeRTOS
with interrupts enabled, so they are used for comparison only (with
interrupts masked). PQRV's NTT code for Kyber and Dilithium was studied for
its layer-merging layout but is not included: its modular multiplications
rely on `mulh`, which takes five cycles on the ESP32-C3.
