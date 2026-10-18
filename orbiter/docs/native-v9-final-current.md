# Current native v9 full regression and remaining QA

The bounded live retry is now complete: manual START/release, separation and
initial climb are verified. This file retains the earlier full-suite/handoff
record; current instructions and limits are in the
[operator guide](native-v9-operator-demo.md) and [acceptance matrix](native-v9-acceptance.md).

One unfiltered serial CTest run against clean source
`0bd60e8105910096ece760cd614517bb5d0dbd5c` completed with **103 passed,
1 failed, 0 skipped, 104 total** in 1191.33 seconds. CTest returns 8.
All 93 inventoried executable/module/library hashes are unchanged across the
run. Every registered test has a result; no test is disabled or marked as an
expected failure. Source is clean before and after the run, before this report
is added. The source tree and its assertions are not modified for testing.

The sole failure is `native_pdi_pilot`: saved AGC cycle counter 124205511,
reload 0, with the strict faithful-resume/CDU FIFO assertion still enforced.
No new regression appears. The separate original 60-second exact-repeatability
failure remains open and is outside this registered suite. No AGC snapshot
redesign, failure waiver or tolerance change occurs.

The complete run includes startup pause, manual step alignment, sunlight
occlusion, original insertion/liftoff/descent, controller/input/save and GPU
checks. Automated `native_pdi_window` passes in 469.48 seconds. These automated
windows do not establish human input, physical controller calibration or full
mission acceptance. Raw JUnit, log, full per-test output and 295 fresh runtime
artifacts are retained at
`<NATIVE_EVIDENCE_ROOT>/final-current-0bd60e8-20261012`.
The native GPU is released at 2026-10-12T05:50:31.333141+00:00.

The existing isolated paused QA runtime is refreshed for the tested renderer.
Only its WgpuClient hash changes to
`c240f6f6eca21bb028dd12193aa656fa395e4b1be3cefdf923cffaeba83ee0e4`.
The app bytes, signature, bundle identity and unique registration remain intact;
both original scenarios/configs/inputs are unchanged. Both guarded dry runs
pass, and cleanup still refuses an unowned PID. No interactive QA is launched
by the implementation worker, and no new app copy or registration is made.
The launchers retain PID/start-time/bundle/executable ownership guards,
1,800-second wall budget, one retained CUA binding, and no reacquisition after
exit or binding loss. They initialize one 1/60-second step, then remain paused.

PDI is the next bounded case, using:

```sh
python3 <NATIVE_EVIDENCE_ROOT>/paused-app-qa-20261012/launch_paused.py pdi --launch
```

Only the coordinator/Astra launches it. Verify PAUSED Eagle and frozen clock;
inspect commander, DSKY, FDAI, X-pointers, F2 instruments and the exterior.
Close overlays before flight input. If reliable key down/up is available,
resume briefly, tap/release L for 0.2–0.3 seconds, pause and inspect the response,
then close normally within the 10-second resumed simulation cap. Release does
not imply zero angular rate. Do not enter PRO/V57 or a landing checklist.
Avoid the previously timed-out F4 focus cycle during initial inspection.
Retain real images/logs/named save and verify process exit before another case.

For liftoff, use the same launcher with `liftoff` only after PDI closes. From
commander, Ctrl+Left selects the left breaker/EDS view. Unmodified wheel down
widens vertical FOV to its 90-degree limit; Ctrl+wheel does nothing. Resizing
improves pixel size but does not expand the vertical viewing angle. At maximum
FOV, the stock HE PRESS/ASC click centre is at 99.90% of viewport height, so
FOV/window changes alone do not provide a reliable unclipped target.

Existing normal right-button drag can look down while paused, if the operator
or tool supports it. The previous CUA interface did not expose button choice.
The existing NASSP plain Down-arrow free-camera control is an alternative:
resume, hold Down for about 0.25–0.5 simulated seconds at 1x, release, then F5
pause. It lowers the eye by 0.05–0.1 m, moving the predicted centre to about
94.70–89.51% of viewport height at 90-degree FOV. This source-based geometry
proposal is not a live visibility result. It works only while running, and
positioning time counts against the 30-second cap. Confirm actual control and
label visibility before clicking; do not invent an unsupported drag API or use
hidden clicks/state edits.

Once visible, operate MASTER ARM, HE PRESS/ASC and ENG ARM ASC with clear
readbacks. Only then resume, press/release ] for about 0.5 seconds, inspect a
short separation/climb and pause/close within the cap. Key release does not
mean shutting down a latched running engine. Keep START blocked if positioning
or the three readbacks remain uncertain. No source feature or mission-state
bypass is added to make this check possible.

Physical controllers, the F4 tool timeout, terrain/material parity, complete
flight/manual landing and strict resume/repeatability remain unresolved.
No push, hosted CI, guest work or publication occurs in this phase.

[Machine-readable full result and QA handoff](../upstream/validation-v9-final-current-macos-2026-10-12.json)
