# pqmagic_nev_ake: NEV-AKE (NGCC round-1 candidate, key exchange)

Authenticated key exchange built on NEV. Reference implementation as shipped
with PQMagic (`third_party/pqmagic/kex/nev-ake/std`, Apache-2.0), all nine
parameter sets, hashing with SHA-3/SHAKE as PQMagic's library build does.

A key exchange does not fit the KEM runner, so `suite.c` has its own: two
parties generate long-term keys (`keygen`, `keygen-peer`), the initiator
sends message 1 (`init`), the responder answers with message 2 and derives
its key (`response`), the initiator derives its key (`finish`). The two
keys must be equal; with a modified message 2 they must differ (`tamper`).

Checks: the above, equal fingerprints on device and host (90), identical
outputs with the implementation's own Keccak on the host.
