# Native macOS v9: bounded operator demo

The 2026-10-12 milestone verifies real Apollo 11 cockpit inspection, bounded PDI
input delivery, and manual START/release, separation and initial lunar climb.
The retry closed normally after 10.8167 simulated seconds. This is an initial
liftoff demo, not a completed ascent, lunar orbit, rendezvous or landing.
See the [acceptance matrix](native-v9-acceptance.md) for the precise boundary.

## Verified setup

| Component | Locked version / location |
| --- | --- |
| OpenOrbiter | `orbitersim/orbiter@bd1d101f348da66e8c5b986633127c05e18d42c8`, 2026-10-09 development release |
| NASSP | `rcflyinghokie/NASSP@e731c28c0ebc1a4fe3b63f99e2006a1a8a2b42a9`, `NASSP-V9.0-Orbiter2024-434` |
| Checkout | `<POC_CHECKOUT>`, branch `macos-openorbiter-nassp-v9` |
| QA app | `local.orbiter.native-v9.qa-task2`, existing `evidence/app-launch-20261012/NASSP Native v9 QA.app` under the task root |
| Renderer | WgpuClient SHA256 `c240f6f6eca21bb028dd12193aa656fa395e4b1be3cefdf923cffaeba83ee0e4` |

Hashes select the versions; moving release tags do not. The clean original
`9e8d452` demo remains at `<PRESERVED_POC>`.
Build/data provenance and removal policy are in [native-v9-upgrade.md](native-v9-upgrade.md).

## Launch one original scenario

For a new machine or checkout, use the tracked stager and paused CLI recipe in
[native reproduction](../../docs/native-reproduction.md). The local app command
below records the original QA setup and depends on its retained host-specific
launcher and bundle; those are not shipped as a portable application.

Use the guarded launcher, with one simulator at a time:

```sh
python3 <NATIVE_EVIDENCE_ROOT>/paused-app-qa-20261012/launch_paused.py liftoff --launch
```

For PDI, replace `liftoff` with `pdi`. Omit `--launch` for a dry run.

| Case | Original scenario under `Scenarios/` | Original SHA256 | Running cap |
| --- | --- | --- | --- |
| Liftoff | `Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 11 - Lunar Liftoff T-1m.scn` | `16b02ad534ee76dd07d98eb1e1b6c3c949d8a0346a03a305dbb8c420edb20f5b` | 30 s |
| PDI | `Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m.scn` | `cef66d6824503ccc6c36a6cad46fcc967d33c53323565a3857dd6e7d238e7203` | 10 s |

Both use original input bytes, fixed 1/60 s, real-time, laptop controls,
WgpuClient, ProjectApolloMFD and 3D cockpit. StartPaused performs one original
initialization step, then freezes across inspection. The cap counts resumed
loop time. The 30-minute watchdog targets only the anchored PID, kernel start
token and app paths. Normal close writes `PausedQALiftoff` or `PausedQAPdi`.
These saves are observations, not faithful AGC restart points.

Confirm **PAUSED Eagle** and a frozen mission clock before operating controls.
For CUA, acquire the already-running app once and retain its binding. Never
reacquire after exit or binding loss: acquisition can otherwise reopen the app
with its default scenario. Coordinates depend on actual window/view framing;
identify labels rather than copying old screenshot coordinates.

## Liftoff actions verified in the bounded retry

1. From commander, Ctrl+Left selects the left breaker/EDS view. Unmodified wheel
   down widens the cockpit FOV, up to 90 degrees. Briefly resume with F5, request
   a 500 ms plain Down-arrow press/release, then F5 pause. This existing NASSP
   camera movement exposes HE PRESS/ASC; it consumes part of the running cap.
2. Click MASTER ARM to ON and verify its visible readback.
3. HE PRESS/ASC is **unguarded and spring-loaded to SAFE**. If checking this
   control, resume first and use a timed left press on its upper FIRE half;
   the retry requested 500 ms, then released and paused. SAFE afterward is
   expected. Transient FIRE/area 3818 delivery was not conclusively observed.
   The original scenario already has fired ascent pyros, open isolation valves,
   3020 psi helium and 184 psi ullage; do not edit state or require FIRE to latch.
4. Ctrl+Right twice reaches DSKY via commander. Set ENG ARM to ASC and verify
   the readback. F1 opens Flight controls: select Exterior view and dismiss F1.
   Wheel zoom works. Clear overlays before flight input.
5. Resume with F5; request a **500 ms `]` START press/release** in laptop mode.
   Observe the ascent stage separating, plume and increasing gap, then pause.
   The configured hold is not a measurement of exact simulated input duration.
   Engine thrust can remain on after START release; that is expected.
6. F2 opens Orbit/Surface instruments. The verified retry showed ALT-R 0.01 km
   and vertical speed +4.67 m/s. Close normally within the cap and verify exit.

The actual save has all ten original vessels plus `Eagle-DESCENTSTG`, Eagle
`CONFIGURATION 2`, free-flight `STATUS Orbiting Moon` and `THLEVEL 0:1.000000`.
Its position/velocity give radial speed 4.6661 m/s; the saved vessel origins are
10.1626 m apart.
The serialized word “Orbiting” means free-flight status here, not an achieved
lunar orbit. The [verified save record](../upstream/validation-v9-bounded-demo-macos-2026-10-12.json)
links the raw save, input timestamps and actual images.

## Short PDI inspection

Use a fresh original PDI launch. Inspect commander, DSKY/FDAI/X-pointers and F2
instruments while paused; Ctrl+Right/Left navigate seats. F1 switches cockpit
and exterior; wheel zoom exposes the lunar scene. The final live check requested
a 250 ms `L` press/release between F5 resume/pause, then closed at 0.65 simulated
seconds. It verifies input delivery and resumed observations, not isolated yaw
authority, neutral ACA telemetry or physical controller calibration. Avoid the
previously timed-out F4 focus cycle. No PRO/V57 or landing checklist is part of
this demo.

## Validation and retained evidence

The last full suite on source `0bd60e8` is **103 passed, 1 failed, 0 skipped,
104 total**, 1191.33 s, CTest exit 8. The strict `native_pdi_pilot` failure remains:
AGC counter 124205511 saves, but reloads as 0. All 93 binary hashes were unchanged.
Subsequent milestone work changes documentation only. See
[full results](native-v9-final-current.md) and [acceptance](native-v9-acceptance.md).

The paired fixed-camera native images and stock Windows reference verify
qualitative restoration of the lit pale LM face and gold foil with identical
Moon archives. Windows used software D3D9 under ARM64 Windows emulating x64 and
retained three experiment-module failures plus material/shutdown warnings.
This is not complete pixel, photometric, material or mission parity; see
[sunlight comparison](native-v9-sun-occlusion.md) and the recorded
[bounded state comparisons](native-v9-step-input-comparison.md).

Raw live reports remain under task-5 `evidence/final-manual-qa-20261012` and
`evidence/he-hold-retry-20261012`. The retry's 953 protected hashes match; no
simulator remains. Original image bytes are retained, including JPEG captures
with `.png` filenames; filename extension is not proof of encoding or dimensions.
Strict AGC resume, exact repeatability, terrain/material fidelity, physical
controllers and full missions/manual landing remain open. No push or hosted CI
is part of this milestone.
