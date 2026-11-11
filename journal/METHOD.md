# Method

Every optimization in this project went through the same loop.

1. **Baseline.** Port the upstream reference code unchanged and measure it on
   the ESP32-C3: time per operation, peak stack, peak heap, code size. This is
   the `ref` variant and it stays in the image.
2. **Find where the time goes.** Count primitive calls (hash blocks,
   permutations, multiplications), time the primitives alone, read the
   generated RV32 code of the hot loops.
3. **Idea.** State what should get faster and why, before writing code.
4. **Change.** Implement it as a second variant (`opt`) next to the reference,
   never in place of it.
5. **Check.** The changed code must pass, on the device:
   - the known-answer tests the upstream project ships;
   - a fingerprint comparison: every key, ciphertext, signature and shared
     secret of the `opt` variant must equal what an independent build of the
     reference code produces on the build machine (`tools/pqc check`);
   - for primitives, a comparison against the reference on random inputs.
6. **Measure** with the same seeds as the baseline, so both variants do the
   same work.
7. **Verdict.** Keep the change only if it is faster or smaller and passes
   the checks. Record the attempt in `experiments/<suite>.jsonl` either way;
   rejected ideas are as much part of the record as kept ones.
8. **Side effects.** A change that alters timing behaviour with respect to
   secret data is called out in the suite's `NOTES.md`, with what was measured
   about it.

Rules that hold across the project:

- The test board's flash is never written. Every image runs from RAM.
- The board is shared through a lock; one run at a time.
- Numbers quoted anywhere come from `results/*.json`, produced by a run of
  the committed code, not from memory or from intermediate notes.
