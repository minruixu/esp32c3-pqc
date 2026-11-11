# NGCC round-1 public-key candidates on the ESP32-C3: status

One row per candidate, for its parameter set with the lowest claimed classical security level
(usually 128 bits; another set is shown when only that one ran). The reference implementations
are built unmodified from the official submission packages (see [inventory.md](inventory.md)) and
driven through the ICCS uniform interface by `suites/ngcc`. No candidate was optimized individually.

What the columns mean:

- **Outcome**: *works on device* = keygen, encapsulation/signing, decapsulation/verification and the
  tamper test pass on the ESP32-C3 from RAM. *host only* = the same test passes on the build machine but
  not on the chip (reason given). *does not build* / *does not work* = the generic port failed already on
  the build machine (reason given).
- **KAT**: the run seeded like test vector 0 of the ICCS KAT program reproduces the first vector of the
  package's own KAT file (*match*), does not (*differs*), or the package has no usable file.
- **= host**: output fingerprints of the device run that equal those of the host run of the same code.
- Times are averages on the ESP32-C3 at 160 MHz (3 runs, or 1 run for slow operations), with the
  harness random generator; stack and heap are the peaks of one operation; image is the RAM image size.
- **opt**: most candidates hash with the SM3-based helper functions that ICCS supplies. For those, a
  second run sends the SM3 compression function to this project's optimized backend
  (`components/prims`); the column is the speed-up over the unmodified package (geometric mean of the
  three operations). The outputs of both runs are identical.

Summary: 36 works on device, 22 host only, 16 does not build, 7 not attempted here, 3 does not work.

## Key encapsulation

| Candidate | Family | Set (level) | Outcome | KAT | = host | keygen | encaps | decaps | opt | Stack KB | Heap KB | Image KB | Note |
|---|---|---|---|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| Aigis-Enc+ | Lattice (Module-LWE) | | not attempted here | | | | | | | | | | ported from PQMagic by another suite of this project |
| Amoeba | Lattice-based (Ring-LWE) | Amoeba-576 (128) | works on device | match | 16/16 | 13 ms | 18 ms | 21 ms | 1.37× | 18.1 | 1.2 | 151.1 |  |
| BAG-Loong | Code-based (rank metric) | BAG-Loong-128 (128) | host only | differs | 0/16 |  |  |  |  |  |  | 145.8 | operations return an error on the chip (decaps rc=-6, encaps rc=-6, kat rc=-6, kat-roundtrip, keygen rc=-6) |
| BAG-Piglet | Code-based (rank metric) | BAG-Piglet-128 (128) | does not build |  |  |  |  |  |  |  |  |  | 1 source files use x86 inline assembly; kem_BAG-Piglet/Implementations/Reference_Implementation/bag_piglet128/src/common/gabidulin.c:237:5: error: ... |
| BIKE-MLThre | Code-based (QC-MDPC) | BIKE_MLThre-128 (128) | does not build |  |  |  |  |  |  |  |  |  | undefined symbols: randombytes_init_with_seed_len |
| BRA | Code-based (rank metric) | BRA-128 (128) | does not build |  |  |  |  |  |  |  |  |  | 15 source files need x86 intrinsics; undefined symbols: kem_dec, kem_enc, kem_get_ct_len_bytes, kem_get_pk_len_bytes, kem_get_sk_len_bytes, kem_... |
| BRQC | Code-based (rank metric) | BRQC-128 (128) | does not build |  |  |  |  |  |  |  |  |  | 15 source files need x86 intrinsics; undefined symbols: kem_dec, kem_enc, kem_get_ct_len_bytes, kem_get_pk_len_bytes, kem_get_sk_len_bytes, kem_... |
| BW-KEM | Lattice-based KEM | BW_KEM_C128 (128) | works on device | match | 16/16 | 8.45 ms | 10 ms | 12 ms | 1.29× | 10.5 | 0.8 | 131.4 |  |
| C-Multi-UR-AG | Code-based (rank metric) | Level-128 (128) | does not build |  |  |  |  |  |  |  |  |  | 15 source files need x86 intrinsics; undefined symbols: kem_dec, kem_enc, kem_get_ct_len_bytes, kem_get_pk_len_bytes, kem_get_sk_len_bytes, kem_... |
| CheetahKEM | Lattice (Ring/Module-LWE) | Cheetah128 (128) | works on device | match | 16/16 | 9.74 ms | 14 ms | 17 ms | 1.25× | 27.8 | 2.0 | 137.1 |  |
| COMPASS-KEM | Lattice-based | COMPASS-KEM-128 (128) | works on device | match | 16/16 | 52 ms | 54 ms | 55 ms | 1.55× | 16.7 | 8.5 | 133.4 |  |
| CTL Algorithm | Lattice-based | CTL-512 (512) | does not build |  |  |  |  |  |  |  |  |  | 2 source files do not compile on the host (This module must not be compiled separately.); undefined symbols: solve_ntru_3329_2048_clean |
| DKEM (Ding Key Encapsulation) | Lattice (Module-LWE) | DKEM-128 (128) | works on device | match | 16/16 | 6.25 ms | 7.72 ms | 9.15 ms | 1.30× | 10.1 | 0.2 | 130.8 |  |
| DTRU | Lattice-based (NTRU) | DTRU-648 (128) | works on device | match | 16/16 | 5.70 ms | 5.45 ms | 10 ms | 1.07× | 19.8 | 1.6 | 137.6 |  |
| FLIT | Lattice (NTRU-like KEM) | FLIT128 (128) | works on device | match | 16/16 | 4.00 ms | 4.95 ms | 6.61 ms | 1.15× | 7.9 | 0.3 | 138.4 |  |
| HARE | Code-based (quasi-cyclic) | HARE-2 (128) | works on device | match | 16/16 | 386 ms | 838 ms | 1.2 s | 1.00× | 81.2 | 9.5 | 145.2 |  |
| Hybrid Equivalent Punctured and Quasi-Cyclic | Code-based (quasi-cyclic) | ref (?) | does not build |  |  |  |  |  |  |  |  |  | 3 source files need x86 intrinsics; 2 source files include auxfunc.h, which is not in the reference tree; undefined symbols: iccs_get_random_number, iccs_init_random_number, kem_dec, kem_enc, kem_get_ct_len_bytes,... |
| LoongKEM | Lattice (LWE) | Loong128 (128) | host only | no output | 0/0 |  |  |  |  |  |  | 131.1 | no output for 100 s on the chip: hangs or is far too slow |
| Lore | Lattice (module-LWR) | Lore-SHAKE/Lore-L1:Lore-128 (128) | works on device | match | 8/8 | 13 ms | 26 ms | 32 ms |  | 17.8 | 0.1 | 131.5 |  |
| MAMBA-Frost | Lattice-based (learning with quantization) | MAMBA-Frost-128 (128) | host only | match |  |  |  |  |  |  |  | 133.4 | no output for 150 s on the chip: hangs or is far too slow |
| MAMBA-Viper | Lattice-based (Module-LWQ) KEM | MAMBA-Viper-128 (128) | works on device | match | 16/16 | 7.99 ms | 13 ms | 16 ms | 1.19× | 15.6 | 1.6 | 134.3 |  |
| Mithril | Lattice (radical-ring LWR) | Mithril-128 (128) | works on device | match | 16/16 | 13 ms | 15 ms | 16 ms | 1.14× | 11.7 | 1.0 | 131.6 |  |
| Mito | Code-based | Mito1 (128) | works on device | match | 16/16 | 807 ms | 1.3 s | 1.9 s | 1.00× | 78.7 | 12.0 | 156.7 |  |
| MORNING-Scabbard | Lattice (Module-LWR) | Scabbard-128 (128) | works on device | match | 16/16 | 28 ms | 30 ms | 30 ms | 1.28× | 18.9 | 0.8 | 129.5 |  |
| NEV | Lattice-based (NTRU/RLWE hybrid) | | not attempted here | | | | | | | | | | ported from PQMagic by another suite of this project |
| NSS-HQC | Code-based (HQC-type quasi-cyclic codes) | NSS-HQC-128 (128) | host only | differs | 0/16 |  |  |  |  |  |  | 131.1 | operations return an error on the chip (decaps rc=-1, encaps rc=-5, kat rc=-5, kat-roundtrip, keygen rc=-5) |
| NTRE Key Encapsulation Mechanism | Lattice-based (NTRU) KEM | NTRE-2593-648 (128) | works on device | match | 16/16 | 3.14 ms | 3.05 ms | 3.21 ms | 1.22× | 9.2 | 0.5 | 131.4 |  |
| OAEP-NTRU | Lattice (NTRU) | OAEP-NTRU-648 (128) | works on device | match | 16/16 | 2.94 ms | 3.56 ms | 3.71 ms | 1.20× | 9.6 | 0.5 | 133.6 |  |
| Polar-KEM | Lattice (polar-code-defined) | PolarKEM-128 (128) | works on device | match | 16/16 | 5.15 ms | 18 ms | 36 ms | 1.57× | 10.6 | 4.3 | 131.2 |  |
| PolarLAC | Lattice-based (module-LWE with polar-code decoding) | x86/POLARLAC-128 (128) | works on device | match | 16/16 | 5.51 ms | 7.51 ms | 9.56 ms | 1.29× | 13.4 | 0.7 | 144.3 |  |
| QIMEN-PIKE | Isogeny-based | | not attempted here | | | | | | | | | | ported from PQMagic by another suite of this project |
| Quasi-Cyclic Twisted McEliece Key Encapsulation Mechanism | Code-based (quasi-cyclic twisted McEliece) | QCTM128 (128) | does not build |  |  |  |  |  |  |  |  |  | 1 source files include drng.h, which is not in the reference tree; undefined symbols: iccs_get_random_number, iccs_init_random_number, kem_dec, kem_enc, kem_get_ct_len_bytes,... |
| QUBE | Code-based | QUBE-128 (128) | works on device | match | 16/16 | 27 ms | 27 ms | 48 ms | 1.20× | 29.8 | 8.3 | 139.3 |  |
| Rudraksh2 | Lattice-based KEM | Rudraksh2-128-I (128) | works on device | match | 16/16 | 104 ms | 105 ms | 106 ms | 1.52× | 19.7 | 1.0 | 129.6 |  |
| Scloud+ | Lattice-based (unstructured LWE with Barnes–Wall decoding) | | not attempted here | | | | | | | | | | ported from scloudplus repository by another suite of this project |
| TRIKE | Code-based | TRIKE-2 (128) | host only | match | 16/16 | 6.4 s | 569 ms |  | 1.01× |  |  | 138.1 | runs on the chip, but decapsulation returns a different shared secret than encapsulation |
| TriQ-KEM | Code-based | TriQ-KEM-128 (128) | works on device | match | 16/16 | 243 ms | 473 ms | 712 ms | 1.01× | 60.0 | 3.1 | 140.4 |  |
| UVW Key Encapsulation Mechanism | Code-based | UVW-KEM-128 (128) | host only | match |  |  |  |  |  |  |  |  | image does not fit in RAM (dram0_0_seg over by 11 KB) |
| Weaver | Lattice (Module-LWR) | Weaver-640 (128) | works on device | match | 16/16 | 18 ms | 20 ms | 22 ms | 1.40× | 21.0 | 0.9 | 136.9 |  |
| YuanYang.KEM | Lattice (NTRU) | YuanYang.KEM-512 (128) | works on device | no KAT file in the package | 16/16 | 18 ms | 30 ms | 40 ms | 1.04× | 22.6 | 2.5 | 147.0 |  |
| ZEN | Lattice (NTRU) | ZEN_128 (128) | host only | match |  |  |  |  |  |  |  |  | needs code that uses 128-bit integers (__int128), which the 32-bit RISC-V compiler does not have (poly.c) |

## Digital signatures

| Candidate | Family | Set (level) | Outcome | KAT | = host | keygen | sign | verify | opt | Stack KB | Heap KB | Image KB | Note |
|---|---|---|---|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| Aigis-Sig+ | Lattice (Module-LWE/SIS, Fiat-Shamir) | | not attempted here | | | | | | | | | | ported from PQMagic by another suite of this project |
| BIT: Bimodal Triangular distribution based lattice signatures | Lattice-based | BiT-128 (128) | works on device | match | 12/12 | 14 ms | 227 ms | 15 ms | 1.34× | 40.4 | 0.7 | 141.0 |  |
| CEDRUS+C | Hash-based (stateless) | cedrus+c-160f (160) | works on device | match | 12/12 | 472 ms | 16.9 s | 506 ms | 1.63× | 5.0 | 0.3 | 131.1 |  |
| CEDRUSɑ | Hash-based | CEDRUSα-160f (160) | host only | match |  |  |  |  |  |  |  |  | image does not fit in RAM (dram0_0_seg over by 38 KB) |
| Chinith | Symmetric-key (VOLE-in-the-head) signature | SM4th-d3-128f-lo (128) | host only | match |  |  |  |  |  |  |  |  | sig_Chinith/Implementations/Reference_Implementation/sm4th_d3_128f_loose/sm4_witness.c:271:12: error: 'w_out' undeclared (first use in this function) |
| COMPASS-SIG | Lattice-based | COMPASS-SIG-128 (128) | host only | no output | 0/0 |  |  |  |  |  |  | 134.5 | does not fit in RAM: stack overflow or memory fault with every stack/heap split that was tried |
| CS | Lattice (Module-LWE, Fiat-Shamir) | CS-128 (128) | does not build |  |  |  |  |  |  |  |  |  | 1 source files do not compile on the host (static declaration of 'MultiplyNTT' follows non-static declaration); undefined symbols: AddPoly, AddVector, AddorSubPoly, ExchangeModulus, HalfCenterModVector, INTT, LazyReduct... |
| DARTS | Lattice-based | DARTS-128 (128) | works on device | match | 12/12 | 21 ms | 1.4 s | 14 ms | 1.30× | 92.2 | 9.1 | 157.5 |  |
| DOVE | Multivariate (Double Oil-and-Vinegar) | DOVE_classic_128 (128) | host only | no output | 0/0 |  |  |  |  |  |  | 134.0 | keys or signatures are too large to allocate on the chip |
| Facto-DSA | Multivariate (cubic factorisation) | Facto-DSA-128 (128) | host only | differs | 9/12 | 1.4 s | 644 ms | 51 ms | 1.09× |  |  | 140.5 | runs on the chip, but its outputs differ from the host run (kat0-pk, kat0-sk, kat0-sig) |
| FlexTree | Hash-based (stateless) | Flextree-160f (160) | host only | match |  |  |  |  |  |  |  |  | needs code that uses 128-bit integers (__int128), which the 32-bit RISC-V compiler does not have (pors_fp.c) |
| Galas Signature Scheme | Symmetric (MPC/VOLE-in-the-Head) | Galas-160F (128) | host only | differs |  |  |  |  |  |  |  |  | image does not fit in RAM (dram0_0_seg over by 355 KB) |
| GreatWall Signature Algorithm | Symmetric-key (VOLE-in-the-head) signature | GreatWall-128 (128) | host only | no output | 0/0 |  |  |  |  |  |  | 199.8 | does not fit in RAM: stack overflow or memory fault with every stack/heap split that was tried |
| Lynxer | VOLE-in-the-head signature | Lynxer-160f (160) | host only | match |  |  |  |  |  |  |  |  | image does not fit in RAM (dram0_0_seg over by 239 KB) |
| MORNING-ATLAS | Lattice (Module-LWR, Fiat-Shamir) | ATLAS-128 (128) | does not work | differs |  |  |  |  |  |  |  |  | failed: lwrdsa128/opt/verify, lwrdsa128/ref/verify |
| Octarine | Lattice-based signature | Octarine-128 (128) | works on device | match | 12/12 | 29 ms | 51 ms | 34 ms | 1.23× | 71.3 | 1.5 | 145.7 |  |
| OPS Digital Signature Algorithm | Lattice-based signature | OPSsig-128 (128) | works on device | match | 12/12 | 78 ms | 118 ms | 74 ms | 1.49× | 96.5 | 5.3 | 140.7 |  |
| Origami | Multivariate (layered MQ) | Origami-128 (128) | works on device | match | 12/12 | 90 ms | 1.3 s | 1.2 s | 1.35× | 12.5 | 15.0 | 139.2 |  |
| Phoenix | Hash-based signature | Phoenix-128f (128) | works on device | differs | 6/6 | 1.1 s | 20.4 s | 563 ms |  | 6.5 | 0.1 | 135.6 |  |
| Qing Luan | Code-based (restricted syndrome decoding, MPC-in-the-head) | QingLuan-128 (128) | works on device | match | 12/12 | 9.40 ms | 636 ms | 271 ms | 1.30× | 3.2 | 120.1 | 131.9 |  |
| ReSolveD-ɑ | Code-based (regular syndrome decoding, VOLE-in-the-head) | | not attempted here | | | | | | | | | | ported from PQMagic by another suite of this project |
| Rhyme | Lattice-based | Rhyme-128 (128) | host only | match |  |  |  |  |  |  |  |  | needs 128-bit integers (__int128), which the 32-bit RISC-V compiler does not have |
| Shuttle | Lattice (Module-LWE/Module-SIS) | SHUTTLE-128 (128) | host only | match |  |  |  |  |  |  |  |  | needs 128-bit integers (__int128), which the 32-bit RISC-V compiler does not have |
| Sigurd | Code-based (regular syndrome decoding) | Sigurd-128 (128) | host only | match |  |  |  |  |  |  |  |  | image does not fit in RAM (dram0_0_seg over by 545 KB) |
| SQIsign2D-push1/2 | Isogeny-based signature | Level-1 (128) | does not build |  |  |  |  |  |  |  |  |  | 252 source files include gmp.h, which is not in the reference tree; 4 source files do not compile on the host (variable has incomplete type 'struct timespec'); undefined symbols: sig_get_pk_len_bytes, sig_get_sk_len_bytes, sig_get |
| SQIsign2D2 | Isogeny | Level1-eff (128) | does not build |  |  |  |  |  |  |  |  |  | 30 source files need a Windows build of GMP; undefined symbols: sig_get_pk_len_bytes, sig_get_sk_len_bytes, sig_get_sn_len_bytes, sig_keygen, sig_sign, ... |
| SQIsignTriangle | Isogeny/quaternion signature | SQIsignTriangle_lvl1 (?) | does not build |  |  |  |  |  |  |  |  |  | 58 source files do not compile on the host ("Radix must be 32bit or 64 bit"); 51 source files include gmp.h, which is not in the reference tree; undefined symbols: crypto_sign, crypto_sign_keypair, sqisign_open |
| SYDO | Code-based signature (syndrome decoding, MPC-in-the-head) | sydo_160f (160) | host only | no output | 0/0 |  |  |  |  |  |  | 169.8 | no output for 150 s on the chip: hangs or is far too slow |
| Tins | Multivariate (MPC-in-the-head) | Tins-128 (128) | does not work | differs |  |  |  |  |  |  |  |  | failed: tins128/opt/kat, tins128/opt/kat-roundtrip, tins128/opt/verify, tins128/ref/kat, tins128/ref/kat-ro... |
| TRINE | Trilinear-form group-action signature | Balanced-TRINE-128 (128) | host only | differs | 8/12 | 311 ms |  |  | 1.33× |  |  | 141.3 | operations return an error on the chip (kat rc=-5, kat-roundtrip, sign rc=-5, verify rc=-6) |
| TSUOV | Multivariate | reference_implementation/tsuov_128 (128) | works on device | match | 12/12 | 427 ms | 746 ms | 655 ms | 1.21× | 62.1 | 2.7 | 139.4 |  |
| UVW signature | Code-based (Wave-type, syndrome decoding over F3) | UVW128 (128) | does not build |  |  |  |  |  |  |  |  |  | 5 source files use x86 inline assembly; undefined symbols: dp_alloc, dp_free, dp_random_non_zero, dp_right_product_matrix, dp_shuffle, gauss_jordan... |
| VDOO: Vinegar-Diagonal-Oil-Oil | Multivariate (UOV family) | vdoo_128 (128) | does not build |  |  |  |  |  |  |  |  |  | 3 source files do not compile on the host (conflicting types for 'ngcc_rb_ull'); undefined symbols: crypto_sign, crypto_sign_keypair, crypto_sign_open |
| YuanYang.DSA | Lattice-based (NTRU hash-and-sign) | YuanYang.DSA-512 (128) | does not build |  |  |  |  |  |  |  |  |  | 1 source files need x86 intrinsics; undefined symbols: bigint_add, bigint_copy, bigint_div_pow2, bigint_fast_normalize, bigint_fast_normalize_s... |

## Key exchange

| Candidate | Family | Set (level) | Outcome | KAT | = host | init_a | init_b | full | opt | Stack KB | Heap KB | Image KB | Note |
|---|---|---|---|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| ADKEX (Authenticated Ding Key Exchange) | Lattice (unilaterally authenticated KEM-based key exchange) | ADKEX-128 (128) | works on device | match | 20/20 | 1 µs | 6.26 ms | 51 ms | 1.21× | 10.2 | 0.2 | 132.5 |  |
| AFS-KEX | Lattice (Module-LWE AKE) | AFS_KEX_C128 (128) | works on device | match | 20/20 | 17 ms | 17 ms | 88 ms | 1.30× | 19.6 | 0.8 | 133.9 |  |
| CreTAKE | Lattice (composite AKE: KEM + signature) | CreTAKE128/CreTAKE-K2K-PLAC128:K2K-PolarLAC-128 (128) | host only | match |  |  |  |  |  |  |  | 157.4 | no output for 150 s on the chip: hangs or is far too slow |
| DKEX (Ding Key Exchange) | Lattice (mutually authenticated key exchange) | DKEX-128 (128) | works on device | differs | 20/20 | 22 ms | 21 ms | 271 ms | 1.01× | 52.7 | 0.1 | 149.1 |  |
| Loom | Hybrid lattice KEM/signature AKE | Loom-128 (128) | does not work | differs |  |  |  |  |  |  |  |  | 1 source files include config.h, which is not in the reference tree; failed: loomkex-128/opt/full, loomkex-128/opt/kat, loomkex-128/opt/kat-roundtrip, loomkex-128/ref/full, loo... |
| MAMBA-NIKE | Lattice-based NIKE | MAMBA-NIKE-128 (128) | works on device | match | 20/20 | 189 ms | 189 ms | 950 ms | 1.00× | 54.7 | 0.1 | 143.0 |  |
| NEV-AKE | Lattice (NTRU/Ring-LWE AKE) | | not attempted here | | | | | | | | | | ported from PQMagic by another suite of this project |
| NIIKE | Isogeny-based non-interactive key exchange | ngccapi (?) | does not build |  |  |  |  |  |  |  |  |  | 32 source files do not compile on the host ("Radix must be 32bit or 64 bit"); undefined symbols: kex_derive_ss_a, kex_derive_ss_b, kex_generate_pass1_msg_a, kex_generate_pass2_msg_b, ke... |
| TriQ-KEX | Code-based authenticated key exchange | TriQ-KEX-128 (128) | works on device | match | 20/20 | 233 ms | 234 ms | 3.8 s | 1.01× | 59.2 | 57.0 | 143.5 |  |

## Further parameter sets that ran on the device

Operations are keygen / encaps or sign / decaps or verify (init_a / init_b / full exchange for key exchange).

| Candidate | Set (level) | KAT | Op 1 | Op 2 | Op 3 | Stack KB | Heap KB | Image KB |
|---|---|---|---:|---:|---:|---:|---:|---:|
| Amoeba | Amoeba-864 (192) | match | 17 ms | 26 ms | 30 ms | 26.0 | 1.7 | 151.2 |
| Amoeba | Amoeba-1152 (256) | match | 21 ms | 30 ms | 35 ms | 33.2 | 2.2 | 151.4 |
| Amoeba | Amoeba-1728 (384) | match | 31 ms | 45 ms | 53 ms | 48.4 | 3.4 | 151.5 |
| BW-KEM | BW_KEM_C256 (256) | match | 20 ms | 22 ms | 26 ms | 21.4 | 1.6 | 140.8 |
| BW-KEM | BW_KEM_C512 (512) | match | 68 ms | 72 ms | 84 ms | 41.5 | 3.1 | 141.6 |
| CheetahKEM | Cheetah256 (256) | match | 26 ms | 32 ms | 37 ms | 53.7 | 7.8 | 137.5 |
| COMPASS-KEM | COMPASS-KEM-256 (256) | match | 203 ms | 205 ms | 208 ms | 26.4 | 8.5 | 133.6 |
| COMPASS-KEM | COMPASS-KEM-384 (384) | match | 124 ms | 129 ms | 134 ms | 32.8 | 8.5 | 135.0 |
| COMPASS-KEM | COMPASS-KEM-512 (512) | match | 216 ms | 222 ms | 229 ms | 43.6 | 8.5 | 135.1 |
| DKEM (Ding Key Encapsulation) | DKEM-256 (256) | match | 19 ms | 21 ms | 23 ms | 20.9 | 0.2 | 131.5 |
| DKEM (Ding Key Encapsulation) | DKEM-512 (512) | match | 67 ms | 72 ms | 77 ms | 40.4 | 0.3 | 135.5 |
| DTRU | DTRU-Light (128) | match | 2.42 ms | 2.95 ms | 6.17 ms | 7.5 | 1.2 | 134.3 |
| DTRU | DTRU-768 (192) | match | 4.75 ms | 6.72 ms | 12 ms | 23.3 | 2.1 | 137.1 |
| DTRU | DTRU-1024 (256) | match | 6.71 ms | 8.30 ms | 15 ms | 30.2 | 2.7 | 138.5 |
| DTRU | DTRU-Prime (256) | match | 3.1 s | 14 ms | 29 ms | 42.5 | 2.8 | 143.5 |
| DTRU | DTRU-1536 (384) | match | 8.90 ms | 13 ms | 24 ms | 44.1 | 4.1 | 137.9 |
| DTRU | DTRU-2048 (512) | match | 18 ms | 19 ms | 34 ms | 58.0 | 5.4 | 145.6 |
| FLIT | FLIT256 (256) | match | 12 ms | 10 ms | 14 ms | 15.0 | 0.7 | 138.7 |
| FLIT | FLIT512 (512) | match | 40 ms | 32 ms | 43 ms | 28.7 | 6.2 | 137.4 |
| Lore | Lore-SM3/Lore-L1:Lore-128 (128) | match | 11 ms | 18 ms | 24 ms | 17.8 | 2.4 | 133.0 |
| MAMBA-Viper | MAMBA-Viper-192 (192) | match | 18 ms | 24 ms | 28 ms | 22.1 | 2.3 | 135.1 |
| MAMBA-Viper | MAMBA-Viper-256 (256) | match | 30 ms | 38 ms | 44 ms | 30.4 | 3.1 | 134.8 |
| Mithril | Mithril-256 (256) | match | 34 ms | 40 ms | 46 ms | 19.3 | 1.8 | 132.0 |
| Mithril | Mithril-512 (512) | match | 108 ms | 130 ms | 153 ms | 35.1 | 3.7 | 131.9 |
| MORNING-Scabbard | Scabbard-256 (256) | match | 68 ms | 73 ms | 76 ms | 37.5 | 1.7 | 129.6 |
| MORNING-Scabbard | Scabbard-512 (512) | match | 199 ms | 219 ms | 229 ms | 63.5 | 3.2 | 129.9 |
| NTRE Key Encapsulation Mechanism | NTRE-2917-1296 (256) | match | 5.93 ms | 5.93 ms | 6.33 ms | 18.3 | 0.8 | 131.9 |
| NTRE Key Encapsulation Mechanism | NTRE-3457-2304 (512) | match | 11 ms | 13 ms | 14 ms | 32.3 | 1.3 | 132.9 |
| OAEP-NTRU | OAEP-NTRU-1296 (256) | match | 8.97 ms | 12 ms | 10 ms | 18.3 | 5.0 | 136.7 |
| OAEP-NTRU | OAEP-NTRU-2592 (512) | match | 21 ms | 33 ms | 29 ms | 35.3 | 9.8 | 138.4 |
| Polar-KEM | PolarKEM-256 (192) | match | 10 ms | 23 ms | 47 ms | 15.1 | 4.3 | 131.6 |
| Polar-KEM | PolarKEM-512 (256) | match | 21 ms | 45 ms | 92 ms | 24.3 | 4.3 | 132.0 |
| PolarLAC | x86/POLARLAC-256 (256) | match | 10 ms | 14 ms | 19 ms | 24.5 | 1.4 | 150.3 |
| PolarLAC | PolarLAC-512* (512) | match | 32 ms | 44 ms | 55 ms | 48.6 | 2.7 | 161.1 |
| PolarLAC | PolarLAC-512* (512) | match | 41 ms | 57 ms | 73 ms | 54.1 | 3.1 | 166.8 |
| PolarLAC | x86/POLARLAC-Light (?) | match | 5.06 ms | 6.95 ms | 8.95 ms | 12.8 | 0.7 | 144.5 |
| QUBE | QUBE-192 (192) | match | 66 ms | 64 ms | 118 ms | 55.9 | 17.1 | 143.0 |
| Rudraksh2 | Rudraksh2-256-I (256) | match | 207 ms | 208 ms | 211 ms | 37.9 | 1.8 | 130.6 |
| Weaver | Weaver-1024 (256) | match | 27 ms | 31 ms | 33 ms | 26.7 | 2.7 | 140.8 |
| Weaver | Weaver-2048 (512) | match | 88 ms | 101 ms | 107 ms | 44.2 | 5.9 | 149.6 |
| YuanYang.KEM | YuanYang.KEM-1024 (256) | no KAT file in the package | 42 ms | 29 ms | 49 ms | 39.4 | 4.6 | 151.5 |
| ADKEX (Authenticated Ding Key Exchange) | ADKEX-512 (512) | match | 1 µs | 67 ms | 469 ms | 40.7 | 13.1 | 137.2 |
| AFS-KEX | AFS_KEX_C256 (256) | match | 41 ms | 41 ms | 202 ms | 40.8 | 1.6 | 143.5 |
| MAMBA-NIKE | MAMBA-NIKE-192 (192) | match | 182 ms | 182 ms | 919 ms | 54.7 | 0.1 | 143.0 |
| MAMBA-NIKE | MAMBA-NIKE-256 (256) | match | 190 ms | 190 ms | 954 ms | 54.7 | 0.1 | 143.0 |
