# Bounded native / stock Windows comparison

This experiment starts at native commit
`a12ae78f7300f7c3db951cf481cabac9cf814f9f`, preserving that commit's 99-check
coverage and its strict AGC resume failure. It changes no NASSP source, mission
bytes, equations, engine API, or previously validated runtime. No native GPU or
VM operation is required. Windows reference binaries remain official unmodified
OpenOrbiter/NASSP v9 binaries under x64 emulation in Windows ARM.

## Current reproduction after StartPaused adoption

The results below are historical for their recorded commits and configuration.
After `4aae8cd`, the native host honors StartPaused and performs the stock initial
step. A manual-step driver must use **StartPaused FALSE** before Launch, so it
owns step1 and every later step. Resuming a TRUE-configured host would add an
unwanted initialization step. The probe rejects TRUE before loading the world
and verifies that every counted step actually advances simulation.

From the engine directory, prepare a fresh manually stepped runtime explicitly:

```sh
python3 tools/stage_nassp_manual.py build/native-v9 \
  --addon /absolute/path/to/locked-nassp-v9-data --profile v9 \
  --scenario 'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m.scn' \
  --config Tests/Fixtures/NativeScenarioCompatibility/Orbiter.cfg \
  --output /absolute/fresh/runtime
```

Add the established `--textures /absolute/orbiter-2024/Textures` path when needed.
The tool preserves the original scenario and reference config bytes; the fresh
runtime selects FALSE and a full-precision1/60 fixed-step policy. Other copied
configuration fields remain intact. Then use the301/559 probe commands below.
The new regression checks actual MJD/AGC boundaries at1/301/319/559, including
neutral/pulse response and refusal of stale TRUE-configured runtimes. Primary
interactive GUI launches continue to honor TRUE; this policy applies to drivers
which manually own every step.

## Historical matched inputs and timing

Both use the locked sources documented in [native-v9-upgrade.md](native-v9-upgrade.md)
and the original Apollo 11 PDI T-15m scenario, raw SHA256
`cef66d6824503ccc6c36a6cad46fcc967d33c53323565a3857dd6e7d238e7203`.
Focus remains Eagle, P63/V50/N18; no PRO/checklist or panel-state changes are sent.
The Windows bundle's CRC and five raw scenario hashes were independently checked.
All 21 selected runtime assets have matching content: seven match raw bytes;
fourteen text inputs match Windows SHA256 after comparison-only CRLF encoding.
No asset or coefficient bytes were changed for the experiment.

The isolated native runtime copies the verified Windows master configuration,
then appends `FixedStep = 0.016666666666666666`. Effective readbacks are:

| Setting | Value |
| --- | --- |
| NonsphericalGravitySources / DistributedVesselMass / RadiationPressure | TRUE |
| StabiliseOrbits | TRUE |
| AtmosphericWind / RefuelOnPad / UnlimitedFuel | FALSE |
| ElevationMode | 1 |
| StartPaused in configuration | TRUE |
| Explicit native step / Windows fixed step | 1/60 s |
| Time acceleration | 1 |

At the recorded pre-fix commit, native `Launch` did not perform the Windows `StartPaused` initialization step.
The driver explicitly executes one initial step, saves `ParityBaseline`, then
executes 300 neutral steps and saves `ParityCoast`. Total simulation time is
`5.016666666666654` seconds; serialized MJD is `40422.8318202371`.
All 301 running steps reported `OrbitStabilised() == false` and
`NonsphericalGravityEnabled() == true`; force/state vectors were finite.
Measured successive simulation-time differences range from
`0.016666666666666607` to `0.01666666666666705`; requested and engine dt are
exactly the same double representation of 1/60. Serialized MJD has lower precision
than the per-step trace and cannot recover exact frame counts alone.

Run the separate observer against a fresh staged runtime:

```sh
cmake --build build/native-v9 --target native_parity_probe --parallel 2
build/native-v9/native_parity_probe /absolute/fresh/runtime \
  'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m' \
  301 --pause-after
```

`--pause-after` applies a pause with a zero-advance observation. A fresh-process
reload uses the saved scenario name with 0 or 1 steps; `--dt 0.01` reproduces
stock Windows' adaptive 10 ms paused-start initialization for its older 25-second
save. The raw loaded state is saved before any step, independently of the pause.
The original CLI also completed 301 headless steps in an independent runtime.
Its entire serialized vessel blocks differ slightly from the observer's blocks;
raw diffs are retained rather than claiming byte identity.

## Observed comparisons

At startup baseline and after the five-second coast, both platforms have identical
serialized MJD, Columbia/Eagle AGC and DSKY records, and RPOS/RVEL/AROT/VROT tokens.
This establishes bounded agreement at the scenario's serialization precision;
Windows does not yet provide a corresponding full-precision per-frame trace.
It does not establish full mission or complete systems-state parity.

Differences retained without an invented tolerance include:

* IMU remainder differences up to approximately `1.7e-10` after the coast.
* Mission time differs by approximately 13.763 ms from the original scenario.
  Existing `02-mission-integration.patch` reads `MISSNTIME` directly as double in
  native Saturn/LEM; upstream reads it through a float. Float32 rounding of the
  supplied tokens predicts the observed offset. The independent `SIMULATEDTIME`
  token for Columbia agrees. This established native behavior was not changed.
* Recorder state, instrument/meter readings, and small systems values differ.
  The reference uses a rendered Windows cockpit; native runs headless. Their
  causes require further inspection, and they are not waived as rounding.
* Native adds its established `MASSCG_STATE` and `NATIVE_LANDING_AUDIO` records.

The Windows 25-second save contains LGC cycle 2,122,068. Native fresh zero-step
reload resets it to 0; one 10 ms step gives 853, matching the Windows paused
re-save. Both AGC and DSKY record sets then agree; serialized orbit/attitude tokens
and Eagle IMU records agree. This characterizes a shared loader limitation;
it does not restore AGC scheduling or pending CDU FIFO state and does not resolve
the strict resume failure. Broad snapshot redesign remains paused.

## Small keyboard ACA experiment

```sh
build/native-v9/native_parity_probe /absolute/another/fresh/runtime \
  'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m' \
  559 --aca --pause-after
```

The first 301 steps match the coast. Steps 302–319 hold unmodified NUMPAD3 for
18 steps (0.3 s); steps 320–559 release it for 240 steps (4 s). No modifiers,
joystick binding, direct thruster setting, or panel/guidance changes are used.
`NativeKeyEvent` reaches the original LEM keyboard ACA path. Nominal 11.5-degree
right yaw becomes -11.500366 degrees after integer conversion. This differs from
the existing P64 virtual-axis test's 14,823 signed input and is a separate test.

Native completed all 559 steps using regular propagation. Original RCS thrusters
responded; peak right-yaw rate was `0.9012357630512802` deg/s, final body-Y rate
was `0.007986107082812735` deg/s, and all final thruster levels were zero.
These are observations, not flight tolerances. A matched Windows pulse,
physical controller, and manual landing remain unverified.

## Minimal Windows observer/input interface

Use a separate optional plugin built against the exact OpenOrbiter public SDK;
leave Orbiter and NASSP DLLs unchanged. Required tools are an x64 C++ compiler
with MSVC-compatible ABI, linker, Windows SDK/runtime libraries, and matching
`ORBITER_LIB` and `ORBITER_SDK_LIB` from the release's `Orbitersdk/lib` directory.
MSVC or clang-cl with those existing libraries suffices; no DirectX SDK or NASSP
headers are needed. Compiler availability must be checked before considering an
installation. The plugin has not been built or run by this native task.

Register `oapi::Module` using `oapiRegisterModule`. The minimal callbacks are
`clbkSimulationStart`, `clbkPostStep`, and `clbkProcessKeyboardImmediate`.
Count actual fixed simulation steps in `clbkPostStep`, excluding the initial
baseline step. Use callback time/MJD arguments there: global time API values are
still at the previous step until `EndTimeStep`.

Apply NUMPAD3 in `clbkProcessKeyboardImmediate`, after Orbiter clears its physical
key buffer and before the focus vessel consumes it. Clear modifiers and other
keys, set `kstate[OAPI_KEY_NUMPAD3] = 0x80` for the 18 pulse steps, then return
false so the original LEM consumes it. Keep the window active and dialogs closed;
record missed input callbacks as a failed protocol, not as completed input.
Stock Windows' selected VESIM/DirectInput RHC path must permit the keyboard
fallback; record its configuration and active-device state.

Queue checkpoint saves during `clbkPostStep` and perform them in the immediate-key
callback after `EndTimeStep`, so saved dates describe the completed state.
Request pause after the final checkpoint. Record public `VESSEL` position,
velocity, rotation, angular velocity, force/weight/torque, thruster levels,
`OrbitStabilised`, and `NonsphericalGravityEnabled`, plus raw scenario blocks.
The documented Lua `simulateimmediatekey` called from a post-step coroutine is
not a verified replacement: this pinned engine clears that buffer in `UserInput`
before vessel consumption. No core fix was made for this inherited ordering.

## Evidence and removal

Canonical report: [validation-v9-parity-macos-2026-10-11.json](../upstream/validation-v9-parity-macos-2026-10-11.json).
Raw local evidence is under the task's `evidence/matched-parity`, including the
independent CLI run, first observer iteration, final probe traces, all raw saves,
copied Windows reference inputs, complete diffs, run arguments and SHA256 ledger.
Do not overwrite earlier v9 evidence or its verified backups.

Remove `Tests/NativeParity.cpp` and its three-line `native_parity_probe` registration
in `cmake/NativeWorld.cmake` when equivalent upstream comparison coverage exists.
The test belongs to the engine inventory's `regression-tools` group; its build
registration remains in `build`. Neither changes the eight NASSP patch groups.
Delete isolated runtimes only after retaining their raw evidence and hashes.
