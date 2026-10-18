# Native StartPaused parity with the pinned Windows startup

The native host now honors the existing `StartPaused` configuration. The locked
Windows source at `bd1d101` intentionally initializes the world once after
simulation-start callbacks, then requests pause. Its first interval is 10 ms
unless a command-line/configured fixed step overrides it. Retained stock Windows
captures confirm one initial 1/60-second step, MJD `40422.8317623667` and both
Apollo 11 AGC counters `1422`.

The nine-line production change is confined to `NativeHost.cpp`. At the end of
Launch, when StartPaused is true, it uses the existing StepSimulation for one
initial step and the existing Pause request. The pause callback/effective state
is applied on the next normal caller tick before any further world update.
The initial step is observable; a paused start is not a zero-step load. Neither
TimeData equations nor the normal false/default running path changes. No NASSP
source, mission, ROM, control input or RNG policy changes.

New real-host/SDL regressions failed before for all three true modes (10 ms,
configured 1/60 s, command-fixed priority). False/default modes passed before.
After the fix, all five pass, with exact callback ordering, 600 system seconds
of idle frames freezing complete headless NASSP/AGC/trajectory state, resume,
input release and normal SDL close/save. Rendered true/false cases also pass
120 idle frames with frozen physics/AGC, then resume/input/close. The actual Main
loop test verifies that a paused window does not consume its one-running-step
cap. False/default complete saved scenario bytes match before/after exactly.

Affected headless invocation:9 pass/0 fail/0 skip. Graphical invocation:5 pass/
0 fail/0 skip, including original lunar liftoff; one shared staging fixture is
counted once. A subsequent Main-loop cap check passes. There are **13 distinct
current CTest checks passing** and two inventory coverage checks passing. The
registered suite now contains102 checks; no complete102-check run is claimed.
Historical100-check evidence remains99 pass/one strict AGC resume failure.

The existing zero-step compatibility test initially failed because its generated
fixture copied StartPaused TRUE while expecting a zero-step load. That failure
is retained. Its manual-step headless runtime now explicitly chooses FALSE;
all zero-step assertions and raw Windows reference configuration remain intact.
This driver continues to own every counted step, while startup pause is tested
independently. A fresh original PDI headless startup with the stock config and
1/60 fixed step matches the stock MJD/counter boundary, then freezes. This does
not establish full saved-state or mission parity.

The removable engine inventory records4002 unchanged files,319 differences and
8 upstream omissions. Remove the NativeHost initialization branch only when an
upstream native host implements equivalent StartPaused ordering. Retain or
upstream the two new regression sources and their NativeWgpu.cmake registrations
with equivalent callback, freeze, resume/input/close and false/default coverage.
The small CoreModuleFixture exports only observe test callbacks.

The automated GPU/test window is clear. Paused Astra QA has **not** been staged
or launched at this checkpoint. Existing QA launch manifests must be refreshed
for this build before a new paused runtime is handed off. The wrong-scenario
Columbia acquisition capture remains separate historical evidence. Shared AGC
resume, exact60-second repeatability, human input and full flight remain open.

Exact results, source references and raw artifact hashes:
[validation-v9-startpaused-macos-2026-10-12.json](../upstream/validation-v9-startpaused-macos-2026-10-12.json).
