# Bounded live macOS QA

Only the QA worker launches these prepared runtimes, after the full suite has
finished and the coordinator has released the desktop/GPU. Use one window at a
time. Launch from an attached macOS desktop session; restricted GPU access can
produce `No wgpu adapter available` before the cockpit appears.

Lunar liftoff: `python3 <NATIVE_EVIDENCE_ROOT>/live-qa-20261012/launch.py liftoff`

Short PDI check: `python3 <NATIVE_EVIDENCE_ROOT>/live-qa-20261012/launch.py pdi`

Confirm Eagle in the window title before operating flight controls; after an
F4 focus check, return to Eagle before any input.

The launcher checks scenario, binaries, resources, configuration and absence of
native controller maps before launching the exact manifest command. It clears
external RNG/input diagnostic environment variables only in its own process.
These are new runtimes; no test runtime, Windows reference or archival P64 save
is used. Both original scenario files remain byte-identical. No Orbiter.cfg is
added: these use the already-tested original-demo physics defaults, including
nonspherical gravity disabled, rather than the separate Windows parity settings.

Press F5 promptly to pause. The title must say PAUSED; the mission-clock display
must stop. Use Fn with function keys if macOS reserves them. The CLI caps running
simulation time at 30 seconds for liftoff and 10 seconds for PDI. Pause time does
not consume that cap; close the native window normally within five wall minutes.
Closing or reaching the cap writes the distinct `LiveQALiftoff` / `LiveQAPdi`
save. These saves are observations; do not reload them as faithful AGC resume.

Liftoff procedure while paused: inspect commander cockpit, DSKY/FDAI/X-pointers
and lunar horizon. Use Ctrl+Left to reach the left EDS panel; set MASTER ARM on
and operate HE PRESS/ASC. Ctrl+Right returns to the commander panel; set ENG ARM
to ASC. If click state/readback is unclear, record a block; do not repair mission
state. F5 resumes. Hold `]` for about 0.5 seconds and release. This launch uses
laptop mode, which maps `]` to the LM's original engine START key; `[` is latched
STOP and a second press releases it. Observe at most 20 seconds of ascent, then
pause. Check stage separation and increasing altitude; full ascent/rendezvous is
outside this procedure.

While paused, F1 opens Flight controls. Click Exterior view, dismiss F1, inspect
Eagle and separated descent stage, right-drag the camera and scroll to zoom.
Reopen F1 and choose 3D cockpit. F2 opens instruments; F4 changes focus. Record
visibility, camera orientation, focus/vessel labels and any clipping or missing
textures. Close the UI panel before flight-key tests: it intentionally captures
keyboard/mouse input. Recenter by releasing all keys; focus loss also releases
held flight input.

For PDI, use a fresh launch of the original T-15m scenario and pause promptly.
Inspect the readable cockpit, instruments and Moon, and perform the same
cockpit/exterior/pause checks. Resume briefly, tap/release L for about 0.2–0.3
seconds, observe the yaw/jet response, then pause and exit within the ten-second
simulation cap. L/J map to yaw right/left in laptop mode. Release means no new
ACA command; it does not promise zero angular rate. Do not enter PRO/V57 or begin
a PDI checklist sequence in this short graphical check.

Minimal acceptance checklist (the QA worker records pass/fail/block, not inferred
from the automated suite):

1. Native window visibly opens with the requested original mission and Eagle
   cockpit; DSKY/FDAI/X-pointers and lunar/exterior textures are readable.
2. F5 pause/resume works; a paused mission clock stays fixed while inspecting.
3. Mouse clicks change the three requested liftoff controls with visible
   readbacks; START release works; Eagle separates and climbs in the short run.
4. F1 cockpit/exterior return, right-drag, zoom and F4 focus give coherent views
   and labels; no clipping/blank exterior prevents inspection.
5. Short PDI keyboard yaw produces a visible response and accepts release;
   opening/closing UI and losing focus leave no held input.
6. Normal window close exits cleanly and creates the case's named save; original
   scenario/config/module hashes remain unchanged. Capture cockpit and exterior
   screenshots plus the console output, Orbiter.log, launch record and new save.

Known limits: full legacy 2D panels and standalone RTCC MFD remain unfinished.
Human mouse/keyboard operation and exterior flight-view correctness await this
QA. Synthetic SDL and framebuffer tests are separate evidence. No physical
controller map/calibration is prepared. Strict AGC scheduling/CDU FIFO resume
and original 60-second exact repeatability remain unresolved. These short runs
do not establish historical flight, guided landing or complete mission parity.

## Exact prepared launch commands

Liftoff:

```sh
<POC_CHECKOUT>/orbiter/build/native-v9/orbiter --root <NATIVE_EVIDENCE_ROOT>/live-qa-20261012/runtime-liftoff --scenario 'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 11 - Lunar Liftoff T-1m' --graphics WgpuClient --module ProjectApolloMFD --window --vc --laptop-controls --realtime --steps 1800 --dt 0.016666666666666666 --profile --save LiveQALiftoff
```

Runtime: `<NATIVE_EVIDENCE_ROOT>/live-qa-20261012/runtime-liftoff`

Scenario: `Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 11 - Lunar Liftoff T-1m`

Original scenario SHA-256: `16b02ad534ee76dd07d98eb1e1b6c3c949d8a0346a03a305dbb8c420edb20f5b`

Pdi:

```sh
<POC_CHECKOUT>/orbiter/build/native-v9/orbiter --root <NATIVE_EVIDENCE_ROOT>/live-qa-20261012/runtime-pdi --scenario 'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m' --graphics WgpuClient --module ProjectApolloMFD --window --vc --laptop-controls --realtime --steps 600 --dt 0.016666666666666666 --profile --save LiveQAPdi
```

Runtime: `<NATIVE_EVIDENCE_ROOT>/live-qa-20261012/runtime-pdi`

Scenario: `Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m`

Original scenario SHA-256: `cef66d6824503ccc6c36a6cad46fcc967d33c53323565a3857dd6e7d238e7203`

