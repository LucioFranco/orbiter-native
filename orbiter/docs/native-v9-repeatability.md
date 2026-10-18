# Native 60-second repeatability diagnosis

The no-driver serialized-state failure is real. The original two runs use
identical scenario, configuration, module and asset bytes across 1,161 checked
files. The input runner and external module match their verified archives, and
Core, LEM and Saturn5 hashes match the recorded build. The texture aliases
resolve to the same external cache; this headless investigation does not rehash
that entire cache. Fresh runs of the untouched input runner still differ at
step 3601. No acceptance tolerance or production correction is introduced.

The localized mechanism is a wall-time seed changing the rigid-body accumulator
flush phase. The relevant expressions are present in the locked upstream Git
blobs as well as the native tree:

* NASSP `src_sys/missiontimer.cpp:47` calls `srand(time(NULL))` in every
  mission-timer constructor, reseeding the process C RNG.
* Orbiter `Src/Orbiter/Body.cpp:39` initializes `updcount` with `irand(100)`;
  `Src/Orbiter/Vecmat.cpp:9` obtains that value from `rand()`.
* `Src/Orbiter/Rigidbody.cpp:265` flushes accumulated position and velocity
  increments when the post-incremented counter equals 1000. Random initial
  counters deliberately spread these updates. A different flush phase changes
  the floating-point grouping of later increments.

Columbia is constructed before NASSP's timer reseeding affects Eagle's body
construction. In two seed-preserving traced runs, Columbia starts at counter
41 and flushes at step 960 in both. Eagle starts at 61 versus 76 and flushes at
940 versus 925. Their complete saves differ at 3601. Six timer seed requests
are recorded per launch; requested and applied seeds remain identical in these
observational runs.

The external observer reads only `Body::updcount` through a C++ member pointer.
It logs initial/reset/final counters, calls no physical-vector getter, writes no
simulation field and saves only at the final boundary. Its ordinary 559-step
save exactly matches the archived untouched runner. With a fixed diagnostic
seed, its 3601-step save exactly matches two independent untouched-runner saves,
SHA-256 `fc0f60de9f3a27fcc2d3f684a69b8170ac97120a9376604b1d8c6a6ecf3bd74a`.
This checks observation neutrality at both bounds without intermediate save
callbacks.

An external dyld interposer also permits explicit seed counterfactuals. It
changes only the argument to existing `srand` calls in those controls; it does
not change the original scenario, core or NASSP binaries. Seed 1 gives Eagle
counter 83/first flush 918, and seed 2 gives 66/first flush 935. Columbia's
counter and first flush remain 41/960. Independent final-only runs bracket the
first observed saved difference near that flush:

| Final step | Complete scenario bytes, seed 1 versus seed 2 |
| --- | --- |
| 900 | Equal |
| 918 | Equal |
| 919 | Equal |
| 920 | Differ: Eagle `LASTGLOBALVEL` and `RR_RATE` only |
| 3601 | Differ; raw residuals retained |

At step 920, the last velocity component is `13433.040103971025` versus
`13433.040103971027` m/s, and RR_RATE is `102.456304153137` versus
`102.456304153136`. These are observations, not acceptance bounds. Both vessels'
complete saved AGC/DSKY blocks, rounded trajectory tokens and MJD agree at all
five sampled boundaries. Every earlier step was not individually saved, so
920 is the first differing observed final boundary in this flush neighborhood.
The original failed runs did not record initial counters/seeds; their exact
historic phases cannot be recovered.

Fixed-seed repeats establish a bounded causal control for shared RNG/flush
scheduling. They do not establish that every later decimal comes from the
flush alone: Orbiter has other `rand()` consumers. They also do not exclude
uninitialized-state problems in other scenarios. End-only saves exclude an
intermediate-save effect in these runs; equal fixed-seed results across wall
seconds argue against variable callback timesteps as this failure's cause.

Both seed and flush expressions are inherited upstream, so this phase isolates
no native-only defect warranting a runtime patch. Windows long-run RNG/runtime
behavior has not been measured here. Native startup also lacks Windows
WinMain's `srand(12345)`; adding it would not prevent later timer reseeding and
would change existing initial phases, so it is not applied to make this gate
pass. A production RNG policy or rephasing change requires a separate reviewed
compatibility decision. No equations, intended assignment timing or mission
state change, and the strict original failure remains open.

External sources live in `upstream/repeatability-diagnostic`; no production
CMake target or acceptance-test registration uses them. Their removal path is
to delete that directory and the external diagnostic build/evidence after this
investigation, while retaining the raw ledger. Build commands and binary/source
hashes are preserved outside the production tree. The first RTLD_NEXT-based
interposer attempt crashed with SIGSEGV; its failed source, binary and execution
record are preserved separately. Successful controls use a direct replacee
call inside the interposer image.

Exact execution records, complete diffs and artifact hashes are in
[validation-v9-repeatability-macos-2026-10-12.json](../upstream/validation-v9-repeatability-macos-2026-10-12.json).
Seventeen diagnostic runs complete normally; the one failed tooling attempt is
reported separately. No production rebuild or full CTest rerun occurs in this
phase. Historical strict AGC resume remains red and its redesign stays paused.
Physical devices, complete flight, graphical-native parity and manual landing
remain unverified. No GPU/VM launch, new Windows capture, push or hosted CI occurs.
