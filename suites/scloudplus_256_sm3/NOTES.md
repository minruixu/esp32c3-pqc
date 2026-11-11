# SCloud+-256 with SM3

Same sources and code as `suites/scloudplus` (see its `NOTES.md`); this image
holds only the SM3 family of level 256. It exists because of memory: together
with the SHAKE and AES families the image left too little heap, the
submission's SM3 helper failed to allocate its buffers, and it then returned
wrong hashes without an error (keys and ciphertexts differed from the host).
The optimized Keccak and SHA-256 backends are left out of this image
(`prims_off.txt`); the SM3 family does not use them.
