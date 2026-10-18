# Bounded Windows comparison: classified differences

This follows the immutable `938b10af6252f58d6575eedf1b4ab86477ea9ce7`
[comparison](native-v9-matched-parity.md). That reference, its raw saves and
verified backups remain intact. Source pins are unchanged. Only the DSE loader
below changes NASSP code; no equations, mission inputs or propagation tolerances
change. Severity describes the impact on this bounded demo, not a flight rating.

| Priority | Difference and user impact | Cause, expected stock Windows behavior, minimal action |
| --- | --- | --- |
| P1, unresolved/shared | Fresh resume loses AGC scheduling and pending CDU FIFO state; a checkpoint cannot claim faithful guidance continuation. | `src_sys/apolloguidance.cpp:533` skips `CYCLECOUNTER` upstream as well. Stock Windows' 25 s save reloads from 2,122,068 to 853 after its 10 ms initialization step; native reproduces that. Preserve the strict failing test. Broad snapshot redesign stays paused. |
| P2, fixed here | A short legacy CSM recorder record loads arbitrary latched K2/K6 relay states, potentially changing later PCM playback. | `src_csm/csm_telecom.cpp:5690`, identical to upstream at the reference, scans 11 values into an uninitialized nine-int array. The old record only provides five tokens and its third decimal stops the integer scan. Initialize the missing relays to the constructor's false defaults. Native then matches the stock Windows startup/coast recorder exactly. Remove the three-line hunk from `07-compiler-portability.patch` after upstream adopts it and retains the regression. |
| P2, comparison limitation | Rendered and headless saves differ in 35 Columbia meters and Eagle's helium display; saved needles cannot establish equal cockpit readings. | The unchanged `src_sys/toggleswitch.cpp:3929` updates/filters display values when `GetDisplayValue()` is called. 33 of 35 differing Columbia meters and Eagle's helium display remain exactly at their original tokens headless. The two other CSM meters are also polled by unchanged `src_csm/satsystems.cpp:3411` for caution/warning. Windows additionally requests display values while rendering. Match rendering/callback cadence before judging these fields; do not move the original filters or alter systems equations for a headless comparison. |
| P3, intentional precision behavior | Initial mission clocks differ by 13.763 ms; absolute mission timestamps differ although the bounded coast duration agrees. | `02-mission-integration.patch` reads `MISSNTIME` directly as double in `src_csm/saturn.cpp:2446` and `src_lm/LEM.cpp:1866`; stock Windows reads through float. The direct parse prevents quantization on every reload and satisfies existing exact zero-step clock checks. Retain it with explicit documentation; an upstream precision fix plus the same reload checks is its removal path. |
| P3, quantified/unresolved | IMU fractional PIPA remainders differ up to 1.70e-10 pulse after the coast. No differing AGC integer memory/channel records were observed. | `src_sys/imu.cpp:367` and its pulse/remainder equations at 508–518 are byte-identical after CRLF normalization. Different full-precision inertial observations can explain tiny remainders; the precise cross-platform arithmetic cause is not isolated. Record the differences without a new acceptance tolerance or bitwise assertion waiver. |
| P3, intentional extra records | Native saves include `MASSCG_STATE`, `THRUSTCOMMAND` and `NATIVE_LANDING_AUDIO`. | Existing `08-state-persistence.patch` retains cached mass/CoG and DPS command before the first step; `02`/`04` retain the landing audio scheduler. Stock Windows lacks these keys. Keep them in raw diffs; remove only after equivalent upstream state/audio support and existing persistence/audio gates pass. |
| P3, existing saved-state correction | Eagle tank/thermal/battery values differ after startup. | The [early-step comparison](native-v9-motor-load-comparison.md) proves stock v9 rejects both saved `MOTORRUNNING 1` flags. Native's existing exact-token loader restores them and consumes their first-frame power. Reproducing stock's ignored flags makes all 144 selected Eagle records match exactly through step 301. Retain the native correction and original mission; upstream the loader hunk before removing it from `08-state-persistence.patch`. No equations change. |
| Unclassified residual | A few Columbia chemical decimals and inertial `LASTGLOBALVEL`/`REMAININGDELTAVEL` still differ. | These are retained in complete raw diffs. They remain unresolved; no new tolerance or waiver is justified. |

Paths in the table are relative to
`Addons/NASSP/Orbitersdk/samples/ProjectApollo`. The recorder defect is shared
undefined behavior exposed by native stack contents, rather than a change
introduced by the native patch stack. Windows' observed zeros are consistent
with constructor defaults; they are not a reliable promise of how an unmodified
loader will behave on another compiler or process run.

## Clock reproducer and compatibility decision

The regression calls the actual C `sscanf` conversions on the untouched scenario
tokens. Columbia's `368744.392486901314` becomes `368744.40625` through `%f`,
an offset of `0.013763098686467856` s. Eagle's `368744.423737` becomes
`368744.4375`, an offset of `0.013763000024482608` s. The observed stock Windows
baseline/coast clocks follow those rounded values; native follows the doubles.
The offset is already present before integration, independently of gravity or
guidance equations. `SIMULATEDTIME` still uses upstream's float parser on both.

The direct-double loader predates this upgrade. Available history does not
contain its author's rationale. Its current, demonstrable purpose is preserving
zero-step mission-clock precision, explicitly required by
`Tests/NativePDIProtocol.py:606` and `Tests/NativePDIPilot.py:193`.
Changing it back to float would trade away that existing requirement merely to
copy the stock quantization. The new regression verifies both untouched initial
clock values and a fresh zero-step reload of the newly saved coast. This is a
documented precision correction, not a claim of complete Windows parity.

## Recorder regression and instrument evidence

The original `DATARECORDER` payload is
`15.000000 15.000000 0.000000 5 368744.392487`. Current v9 expects two doubles and
nine ints. `%d` accepts `0` from `0.000000`, then the next scan fails at `.`, so
only three assignments occur and eight integers were previously unset.
The focused regression first reproduced the wrong loaded relays and wrong
K2/K6 at baseline/coast. With zero initialization it verifies:

* Legacy load: `15.000000 15.000000 0 0 0 0 0 0 0 0 0`.
* Stock startup: `14.500000 15.241667 0 0 1 0 0 0 1 0 0`.
* Stock coast: `3.750000 35.829167 0 0 1 0 0 0 1 0 0`.
* Fresh reload retains the complete modern record, including asserted K3/K7.

The tape speed/position, switch/relay equations and save format are unchanged.
The original mission file is staged byte-for-byte. The new fixture contains
selected stock Windows records with hashes of the complete raw inputs and its
unmodified master configuration; only fixed-step policy is appended at runtime.

The machine-readable report enumerates every differing Columbia meter and the
LM helium display, with original/Windows/native values and whether native held
the original. It also records function hashes: `MeterSwitch::GetDisplayValue`,
`Saturn::GetDisplayedAtmosStatus` and `IMU::Timestep` remain identical to pinned
upstream. The two changing CSM meters are `SuitComprDeltaPMeter` and
`EcsRadTempPrimOutletMeter`, matching the caution/warning poll sites.
Matching graphical callbacks remains untested in this phase; no GPU or VM was
started.

## IMU magnitude and remaining validation

The largest coast differences are 1.70e-10 pulse in Eagle RPZ and 6.30e-11 pulse
in Columbia RPZ. With upstream's `pipaRate` of 0.01 m/s per LM pulse and
0.0585 m/s per CSM pulse (`imu.cpp:121–123`), their equivalent residual velocity
differences are approximately 1.70e-12 and 3.69e-12 m/s respectively. These are
dimensional conversions of observations, not permitted flight errors or future
acceptance bounds. Equal integer AGC records during five seconds do not rule out
later divergence near a pulse boundary.

Run the focused check through CTest:

```sh
ctest --test-dir build/native-v9 -R '^native_scenario_compatibility$' --output-on-failure
```

The test uses the real host and NASSP loaders for 301 neutral steps, then a fresh
zero-step reload. It retains raw saves, logs, traces and `result.json` in a fresh
subdirectory of `build/native-v9/scenario-compatibility-evidence` each time.
Remove `Tests/NativeScenarioCompatibility.py`, its fixture directory and the
registration in `cmake/PortableChecks.cmake` only with equivalent upstream
coverage. The engine inventory records these as regression/build paths.

Exact new test outcomes and local evidence hashes are in
[validation-v9-scenario-compatibility-macos-2026-10-11.json](../upstream/validation-v9-scenario-compatibility-macos-2026-10-11.json).
The earlier 99-check ledger remains a historical result. This phase adds one
registered check and runs only relevant headless gates; it does not claim a new
complete graphical suite.

The subsequent [bounded thermal diagnosis](native-v9-thermal-diagnosis.md)
reproduces all 36 meter records exactly by polling the original display methods
once per completed step. The observed battery and thermal state remains
identical with and without these polls. It also excludes mission-clock rounding
within the observed scope and specifies the two short stock Windows saves still
needed to locate the first energy difference. No thermal equation patch or new
acceptance tolerance follows from that diagnosis.

The fresh [Windows early-step comparison](native-v9-motor-load-comparison.md)
subsequently resolves the Eagle energy cause. The two requested captures are
complete; no further Windows measurement is needed for that bounded diagnosis.

The [native repeatability diagnosis](native-v9-repeatability.md) subsequently
verifies identical inputs for the 60-second no-driver failure and localizes a
wall-time RNG/accumulator-flush mechanism inherited from both upstream sources.
Fixed-seed controls repeat exactly; the untouched runner still fails strict
repeatability. Adjacent final-only boundaries match at steps 918/919 and first
differ at 920 in Eagle inertial velocity and radar rate. No production RNG policy,
assignment timing, tolerance or AGC persistence change follows from the diagnosis.
