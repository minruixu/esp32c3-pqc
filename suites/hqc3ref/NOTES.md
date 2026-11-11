# HQC-3 reference code on a static stack

This suite has no sources of its own. `pqc_suite/suite.cmake` sets
`HQC_REF3_ONLY` and includes `suites/hqc/pqc_suite/suite.cmake`; everything
about HQC is documented in `suites/hqc/NOTES.md`.

Why it exists: decapsulation with the reference code of HQC-3 needs 132 KB
of stack. The harness takes task stacks from the heap, whose largest block
on the ESP32-C3 is below 128 KB. This image contains only the reference
code of HQC-3 and runs it on a 140 KB static array
(`suites/hqc/pqc_suite/hqc_bigstack.c`), which prints the same result lines
as the harness. `tools/pqc run hqc3ref --check` works as for any suite.
