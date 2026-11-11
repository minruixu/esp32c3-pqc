# mldsa-native

- Source: https://github.com/pq-code-package/mldsa-native, directory `mldsa/`,
  commit `dd79120` (2026-11-05).
- License: Apache-2.0 OR ISC OR MIT (see `LICENSE`).
- Local changes: none. The fast Keccak is added from the outside, through
  mldsa-native's native-backend hooks; the RV32IM arithmetic backend used by
  the optimized build is mldsa-native's own (see `suites/mldsa`).
