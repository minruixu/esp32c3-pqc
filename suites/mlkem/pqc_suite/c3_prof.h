/* A sampling profiler for the device: the FreeRTOS tick interrupt (100 Hz)
 * records where the interrupted task was executing. Built only with
 * -D PQC_PROFILE=1; gen/prof_symbols.py turns the report into a table of
 * functions. This is how the hot spots listed in NOTES.md were found. */
#ifndef PQC_C3_PROF_H
#define PQC_C3_PROF_H

#if defined(PQC_PROFILE) && !defined(PQC_HOST)
void c3_prof_start(void);
void c3_prof_stop(void);
/* print the `top` most frequent 64-byte code buckets as INFO lines */
void c3_prof_report(const char *label, int top);
#else
#define c3_prof_start() ((void)0)
#define c3_prof_stop() ((void)0)
#define c3_prof_report(label, top) ((void)0)
#endif

#endif
