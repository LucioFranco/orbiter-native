#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "agc_engine.c"

/* Build manually with clang -std=c11 -O1 -Wl,-dead_strip and the yaAGC
 * include directory. This is a private-helper investigation, not a full
 * AGC/PCM snapshot regression or a production serializer.
 *
 * Diagnostic only: include the exact engine so private FIFO state is visible.
 * No production source, scheduling rule or equations are changed. */
static void clear_fifo(void) {
  memset(CduFifos, 0, sizeof(CduFifos));
  CduChecker = 0;
}
static void trace(agc_t *state, int16_t out[1500][3]) {
  for (int tick = 0; tick < 1500; ++tick) {
    ++state->CycleCounter;
    ServiceCduFifo(state);
    for (int axis = 0; axis < 3; ++axis)
      out[tick][axis] = state->Erasable[0][FIRST_CDU + axis];
  }
}
int main(void) {
  static agc_t a, b, checkpoint;
  static int16_t expected[1500][3], restored[1500][3], counter_only[1500][3];
  CduFifo_t saved[NUM_CDU_FIFOS];
  int checker;
  clear_fifo();
  a.CycleCounter = 1000;
  b.CycleCounter = 100000;
  PushCduFifo(&a, FIRST_CDU, 1);
  assert(ServiceCduFifo(&b) == 1);
  assert(a.Erasable[0][FIRST_CDU] == 0);
  assert(b.Erasable[0][FIRST_CDU] == 1);
  puts("shared_fifo: pulse enqueued by AGC A consumed by AGC B (confirmed)");

  memset(&a, 0, sizeof(a));
  clear_fifo();
  a.CycleCounter = 1000;
  /* Ordered sign and speed changes on all axes. */
  for (int axis = 0; axis < 3; ++axis) {
    for (int n = 0; n < 3; ++n) PushCduFifo(&a, FIRST_CDU + axis, 1);
    for (int n = 0; n < 2; ++n) PushCduFifo(&a, FIRST_CDU + axis, 023);
    for (int n = 0; n < 4; ++n) PushCduFifo(&a, FIRST_CDU + axis, 021);
    for (int n = 0; n < 1; ++n) PushCduFifo(&a, FIRST_CDU + axis, 3);
  }
  /* Nonzero ring/checker/interval phase before the checkpoint. */
  for (int n = 0; n < 661; ++n) {
    ++a.CycleCounter;
    ServiceCduFifo(&a);
  }
  assert(CduChecker != 0 && CduFifos[0].Ptr != 0);
  checkpoint = a;
  memcpy(saved, CduFifos, sizeof(saved));
  checker = CduChecker;
  trace(&a, expected);

  b = checkpoint;
  memcpy(CduFifos, saved, sizeof(saved));
  CduChecker = checker;
  trace(&b, restored);
  assert(memcmp(expected, restored, sizeof(expected)) == 0);
  puts("complete_private_fifo_snapshot: exact 1500-cycle trace matches (confirmed)");

  b = checkpoint;
  clear_fifo();
  trace(&b, counter_only);
  assert(memcmp(expected, counter_only, sizeof(expected)) != 0);
  puts("counter_only_snapshot: 1500-cycle trace differs (confirmed)");

  memset(&a, 0, sizeof(a));
  clear_fifo();
  for (int n = 0; n < 128; ++n) PushCduFifo(&a, FIRST_CDU, n % 2 ? 023 : 021);
  PushCduFifo(&a, FIRST_CDU, 021);
  assert(CduFifos[0].Size == 128); /* Existing overflow behavior is retained. */
  for (int n = 0; n < 1400; ++n) {
    ++a.CycleCounter;
    ServiceCduFifo(&a);
  }
  for (int n = 0; n < 100; ++n) PushCduFifo(&a, FIRST_CDU, n % 2 ? 023 : 021);
  assert(CduFifos[0].Ptr > 0);
  assert(CduFifos[0].Ptr + CduFifos[0].Size > 128);
  checkpoint = a;
  memcpy(saved, CduFifos, sizeof(saved));
  checker = CduChecker;
  trace(&a, expected);
  b = checkpoint;
  memcpy(CduFifos, saved, sizeof(saved));
  CduChecker = checker;
  trace(&b, restored);
  assert(memcmp(expected, restored, sizeof(expected)) == 0);
  puts("wrapped_full_fifo_snapshot: exact 1500-cycle trace matches (confirmed)");
  return 0;
}
