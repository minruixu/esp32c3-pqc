# Post-quantum cryptography on an ESP32-C3

A small ESP-IDF test firmware that runs the two NIST post-quantum standards on an
ESP32-C3, checks them against known-answer test vectors, and measures time and
stack use for every operation:

- **ML-KEM-768** (FIPS 203, key encapsulation, formerly Kyber)
- **ML-DSA-65** (FIPS 204, digital signatures, formerly Dilithium)

The algorithms come from the portable C code of
[mlkem-native](https://github.com/pq-code-package/mlkem-native) and
[mldsa-native](https://github.com/pq-code-package/mldsa-native).

## Results

Measured on an ESP32-C3 (QFN32, revision v1.1) at 160 MHz, ESP-IDF v5.5.3,
compiled with `-O2`. All known-answer tests pass, and a corrupted ciphertext or
signature is rejected.

| Operation | Average time | Peak stack | With `PQC_REDUCE_RAM` |
|---|---|---|---|
| ML-KEM-768 key generation | 8.3 ms | 14 KB | n/a |
| ML-KEM-768 encapsulation | 9.0 ms | 17 KB | n/a |
| ML-KEM-768 decapsulation | 11.1 ms | 18 KB | n/a |
| ML-DSA-65 key generation | 33 ms | 44 KB | 35 ms, 11 KB |
| ML-DSA-65 signing | 112 ms (46 to 312) | 74 KB | 194 ms (51 to 613), 14 KB |
| ML-DSA-65 verification | 33 ms | 46 KB | 35 ms, 13 KB |

- Both libraries together take about 37 KB of flash and no static RAM.
- Signing time varies from one signature to the next; this is a property of
  ML-DSA. Averages are over 30 signatures with fresh randomness, so the two
  signing columns are not the same signatures. On the one fixed test vector,
  the low-RAM build signs about 2.4 times slower.
- These numbers were taken with the program loaded into RAM (see below). The
  flash build compiles but has not been timed on a device; code running from
  flash may be somewhat slower.

## Build and run

Set up ESP-IDF first (`. $IDF_PATH/export.sh`). Replace `PORT` with your serial
port.

### Run from RAM, without writing flash

This loads the test into RAM and leaves the firmware in flash as it is. After a
reset the board boots its own firmware again.

```
idf.py -B build_ram -D SDKCONFIG=build_ram/sdkconfig -D SDKCONFIG_DEFAULTS=sdkconfig.ram build
esptool.py -p PORT --no-stub load_ram build_ram/pqc_c3.bin
python tools/capture.py PORT out.log 150
```

`tools/capture.py` reads the USB serial console without resetting the chip and
stops once the report has been printed. The report repeats every 5 seconds, so
any serial monitor that does not reset the board works too.

For the low-RAM ML-DSA build, add `-D PQC_REDUCE_RAM=1` and use another build
directory:

```
idf.py -B build_ram_lowmem -D SDKCONFIG=build_ram_lowmem/sdkconfig -D SDKCONFIG_DEFAULTS=sdkconfig.ram -D PQC_REDUCE_RAM=1 build
```

`tools/flash_md5.py PORT 0x800000` prints a checksum of the whole flash (8 MB
here), computed on the chip. Running it before and after is a way to confirm
that the flash did not change.

### Flash it

This overwrites the application in flash.

```
idf.py build
idf.py -p PORT flash monitor
```

## Layout

- `main/main.c` runs each operation in its own task and prints the report.
- `main/kem_test.c`, `main/dsa_test.c` hold the known-answer tests and the timed
  operations.
- `components/pqc/` holds the two libraries and `pqc_rng.c`, the random source.
- `sdkconfig.ram` is the configuration for the RAM-only build.

## Before using this for real keys

- `esp_fill_random()` is a true random source only while Wi-Fi or Bluetooth is
  running, or while `bootloader_random_enable()` is active. This test runs
  without either, so its keys are for measurement only.
- Without Secure Boot and flash encryption, a private key stored in flash can be
  read by anyone with access to the USB port.
- The Mbed TLS that ships with ESP-IDF v5.5.3 (3.6.5) has neither algorithm, so
  this code does not give you post-quantum TLS by itself.
- The ESP32-C3 has no SHA-3 hardware, so all hashing is done in software.

## Third-party code

`components/pqc/mlkem` is a copy of `mlkem/` from mlkem-native at commit
`a446d5a`, and `components/pqc/mldsa` is a copy of `mldsa/` from mldsa-native at
commit `dd79120`. `main/kem_vectors.h` and `main/dsa_vectors.h` are the test
vectors from the same two projects. Their licenses are in
`components/pqc/LICENSE.mlkem-native` and `components/pqc/LICENSE.mldsa-native`.
