# Bounded Mac PDI protocol: first N68 checkpoint

This separate observational probe starts from the supplied Apollo 11 PDI T-15m
scenario and stops at the first N68/V57 decision, worksheet row 179. It sends no
V57, chooses no operational dH acceptance envelope, and attempts no landing.
**It is not full checklist fidelity.** The existing 2000 ft diagnostic in
`NativeDescent.cpp` / `NativeOriginalDescent.py` remains unchanged.

The branch `macos-pdi-protocol` starts at `d6b1c7d`. Its additions are confined to
native test code, CMake test registration and this documentation. No NASSP
source, equations, ROM, workbook or scenario data is edited. Windows comparisons
remain deferred. Baseline C++ compilation and the three bounded CTest gates passed on 2026-10-07.
The synthetic contract checks supplement the actual runtime/reload evidence;
passing synthetic checks alone does not establish simulator behavior. Final-tree baseline/candidate validation is recorded in the follow-up evidence
report described below.

## Timing and evidence

Both pinned baseline and candidate original scenarios begin with LGC clock
368743.87 s and stored TIG 369645.02 s, a countdown of 901.15 s. The initial
scenario mission clock has a different offset; the verifier checks elapsed
mission time rather than equating the two absolute clocks.

The probe reads saved original LGC state without modifying it. Luminary099
TIME2/TIME1 are erasables 0024/0025, TIG is 3441/3442, and TTOGO/TFI is
3453/3454 (all octal). Each signed 15-bit one's-complement pair is decoded as
`(signed(high) * 16384 + signed(low)) / 100` seconds. CLOKJOB stores
TIME2 minus TIG in TFI, so TFI is negative before ignition. The independent log
verifier checks the decoded recorded words and N62 countdown agreement.
The LGC ignition routine updates TIG from TIME2. The original descent-command
override can start DPS thrust before that LGC event; both the measured offset
from initial stored TIG and the current LGC countdown are retained. TFI stops
being updated when the original CLOKTASK is terminated at LGC ignition. The simulator's first
zero-to-positive original DPS thrust observation is recorded separately, every
1/60 s simulation step. All TIG-plus events use that measured DPS timestamp;
a PRO key press is never treated as ignition.

Address and timing references are the original
[Luminary099 erasable assignments](https://github.com/virtualagc/virtualagc/blob/master/Luminary099/ERASABLE_ASSIGNMENTS.agc),
[master ignition routine](https://github.com/virtualagc/virtualagc/blob/master/Luminary099/BURN%2C_BABY%2C_BURN_--_MASTER_IGNITION_ROUTINE.agc),
[noun tables](https://github.com/virtualagc/virtualagc/blob/master/Luminary099/PINBALL_NOUN_TABLES.agc),
and [R60/R62 maneuver routine](https://github.com/virtualagc/virtualagc/blob/master/Luminary099/R60%2CR62.agc).

`PDIProtocol.jsonl` records every original callback input down/up, simulated and
mission timestamps, switch requested state/readback, raw DSKY registers,
LGC clock/TIG/TFI, guidance channels 011/012/031/032/033, original landing-radar
readbacks/flags/beam geometry, body angular velocity, and active thruster levels/directions. One-second samples supplement
these events. Key holds and gaps advance the same simulation clock. PRO is
held for two seconds; other keys for 0.2 s. The input stream is independently
checked for order, holds and any extra DSKY commands, including V57.

## Scoped checklist actions

The original scenario selects `Apollo 11 LM Checklists.xls`, PDI worksheet.
Baseline workbook SHA256 is
`63582cd97fd8fd4efacce963644286c61927b2e555446a284473d9d825b0aaf6`;
candidate is
`51056ee8013ab11f58bbc3dfa72a1768a42f8982ae21173602581a6931b30b4c`.
Preflight requires byte-identical supplied scenario, mission config, Luminary099
ROM and selected workbook. The worksheet remains the source of the scoped
control actions; its row 179 says dH within limits without a numeric envelope.

| Worksheet rows | Probe behavior |
| --- | --- |
| 50–53 | PGNS AUTO, original PRO, observe the automatic maneuver return. An explicit ten-second initial settling interval is logged rather than claimed as exact PDI-minus-15 timing. |
| 86, 88–89 | At stored TIG minus five minutes, close original LR breaker and confirm ORDEAL/FDAI settings. |
| 92–94 | At minus four minutes, original trim PRO, wait for F50 N18 return, then ENTER and observe V06 N62/TFI. |
| 97–134 | Confirm the supplied readiness configuration without repairing mismatches; set Rate Scale and AGS AUTO through original callbacks. |
| 139 | At minus three minutes, enter V77. |
| 143, 146, 149–151 | At minus 35 seconds arm DESC; at minus 7.5 seconds record thrusters; at minus five seconds wait for V99 N62 and original PRO. Observe V06 N63 (row 150) before override ON (row 151). Require separately measured DPS ignition and original engine-on/override readbacks. |
| 154 | Observe throttle at actual ignition plus 26 seconds; require throttle-up by plus 30 seconds. Do not equate normalized SDK level with the checklist's engineering percent. |
| 163–164 | At actual ignition plus 2:30, set ALT/ALT RATE and RR SLEW through original callbacks. |
| 55, 169 | Default run logs deferred pilot fidelity. Optional `--checklist-yaw` records original NUMPAD3 input and gyro rotation for the explicit 10° / 174° right-yaw targets. It does not certify a settled attitude. |
| 177–179 | At plus five minutes, enter V16 N68, observe for at least five seconds, save and stop before the V57 decision. |

Prior power setup, visual instrument checks, AGS updates, communications, full
pilot maneuver fidelity, landing-site decisions and graphical hit testing are not
validated. The default run defers the manual yaws. The optional input experiment
exercises their original keyboard/ACA/RCS path while leaving settled attitude
validation unresolved. Range/velocity validity and N68
R3 are observations, not approval to incorporate radar data. Invalid radar or a
sentinel R3 can still be saved at an observational decision checkpoint, and the
report labels measurement availability separately.

The [Apollo 11 mission report](https://apollojournals.org/alsj/a11/a11MIssionReport_1971015566.pdf#page=46)
and [flight transcript](https://apollojournals.org/alsj/a11/a11.landing.html)
describe historical radar disagreement/incorporation, but do not certify a
numerical acceptance envelope for this run. The worksheet's later H=2000 ft
DEDA ENTER step (row 207) is outside this checkpoint and is not a radar
acceptance threshold.

## Validation commands after resource release

First run the light contract suite, which uses synthetic log records and rejects
PRO-anchored ignition, incorrect holds, early arming, wrong readbacks, hidden
commands, missing observation delay and corrupt timestamps/TFI:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 Tests/NativePDIProtocolContract.py
```

Configure a fresh native build for this checkout using the established pinned
Mac toolchain and data profile in `macos-demo-validation.md` / the clean demo
instructions. Do not reuse another checkout's CMake cache. After the coordinator
releases CPU resources, build the original native modules and
`native_pdi_protocol_probe`, then run only the new bounded fixture/gate:

```sh
cmake --build build/native-pdi-protocol --parallel 2
ctest --test-dir build/native-pdi-protocol --output-on-failure \
  -R '^(pdi_protocol_contract|nassp_stage_pdi_protocol|native_pdi_protocol)$'
```

The fixture stages a separate `runtime-pdi-protocol` from the selected original
data. The probe has a 1500 s simulated duration bound and the Python runner a
360 s wall timeout. It writes `NativePDIProtocolCheckpoint.scn` on success;
on a protocol failure it writes the last observation and a blocked checkpoint
where possible. The runner verifies all five original vessel classes and uses a
fresh process with zero steps to verify the saved N68 state and original LGC
memory/channels (except the existing non-restored CYCLECOUNTER). No fresh
process continues the trajectory beyond the decision checkpoint.

Retain `mission-source.json`, `PDIProtocol.jsonl`, `PDIProtocol.log`,
`PDIProtocolReload.log`, both checkpoint/reloaded scenarios and
`pdi-protocol-verification.json`. The report includes input/binary/log hashes,
measured ignition and first-N68 timing, radar measurement availability and
explicit scope limits. Run the bounded test against baseline first; use a
separate candidate build/runtime for the candidate profile after baseline is
understood. No GPU is needed for this headless probe, but simulator/build work
still requires the parent's resource release while packaging validation runs.

At this initial checkpoint the next work was source-grounded pilot maneuver
and radar acquisition, then resolution of the operational dH criterion. The
original pilot-input extension and published source-chart gate are recorded
in the 2026-10-09 section below.

## Initial `4650f20` baseline evidence, 2026-10-07

`upstream/validation-pdi-protocol-macos-2026-10-07.json` records the tested source,
input and binary hashes. The fresh build used two compile jobs; the three new
CTest gates passed in 54.84 s, including 21 synthetic contract checks and the
actual bounded run/fresh zero-step reload. Original diagnostic files remain
byte-identical to `d6b1c7d`.

Complete N62 appeared at 698.7 simulated seconds: R2 `-03 23`, decoded LGC TFI
-202.96 s and LGC countdown 202.45 s. First positive DPS thrust appeared at
898.616667 s, after the original override input, 2.533333 s before initial stored
TIG. First N68 was requested 300.083333 s after measured DPS ignition; the probe
stopped at 1206.5 s with P63 / V16 N68. All five original vessel classes and LGC
memory/channels survived a fresh zero-step reload.

At the checkpoint, landing-radar range/velocity validity was false and R3 was
`+99999`. **No usable dH measurement was obtained.** V57 was not sent, no
operational envelope was selected, and no landing was attempted. This checkpoint
therefore establishes the bounded Mac control/timing/log/save/reload path while
leaving radar acquisition and omitted pilot inputs unresolved.

Evidence is archived at
`<DEPENDENCY_CACHE>/demo-validation/pdi-protocol-baseline-20261007`.
Earlier attempts are preserved separately under the build's
`pdi-protocol-evidence`: they exposed transient blank DSKY registers, an
unsupported ten-second N62 wait, and an unsupported requirement that original
DPS override thrust agree with LGC TIG within two seconds. The final probe records
blank registers atomically, waits for complete N62 before the unchanged V77
window, and retains the measured ignition difference explicitly.

## Follow-up: original radar state and bounded yaw-input experiment

The follow-up restores a previously omitted observation in the original PDI
worksheet: row 150's V06 N63 precedes row 151's override ON. In the corrected
baseline run, natural LGC-controlled DPS thrust starts at 901.183333 s instead
of the earlier 898.616667 s override start. All later timing still uses measured
DPS ignition. The verifier requires the recorded N63/override order and original
engine-on or override state rather than imposing nominal ignition equality.

The probe now reproduces only the read-only beam transform from
`Addons/NASSP/Orbitersdk/samples/ProjectApollo/src_lm/lm_lr.cpp` and logs its
beam/nadir cosine, predicted slant range, antenna discretes, data-good discretes
and original operate/test switch. No model parameter or cutoff is changed:

- `lemsystems.cpp` wires LR power to `PGNS_LDG_RDR_CB`; `lm_lr.cpp` clears both
  position discretes when unpowered. A live position-1 discrete after stepping
  with the breaker closed supports power availability at that observation.
- Position 1 is 24°, position 2 is 0°. AUTO retains the source's normal LGC
  command behavior. The first-N68 runs retain position 1 and no position-2 command.
- The model computes `(ground_altitude_m - 3) / cosine / 0.3048` in **feet**;
  radar velocity components are feet/second. A negative range is replaced by the
  **1,000,000 ft sentinel**. Range/velocity good require the existing source's
  `10 < slant_range_ft < 50000` condition. This is a model lock condition,
  **not an operational dH envelope**.
- The original `RadarTestSwitch` stays in operate (1). No synthetic radar test
  data, early HOVER selection or direct attitude/thruster/AGC writes are used.
- N68 R3 remains the original LGC's feet display; `+99999` is the initialized
  unavailable disagreement. The report distinguishes it from a usable measurement.

The corrected no-yaw baseline reaches first N68 with beam/nadir cosine about
-0.466 and the negative-beam range sentinel. This explains the recorded invalid
flags without diagnosing a power, unit or readback defect.

For the bounded input experiment, `LEM.cpp::clbkConsumeDirectKey` maps normal
NUMPAD3 to right-yaw ACA deflection; `lemsystems.cpp` feeds that to original
`CDR_ACA`, LGC/SCS and RCS. The existing gyro/ATCA mapping in `lmscs.cpp` identifies
Orbiter body Y as LM yaw, and right-yaw response is measured along -Y. The test
holds the original key until the integral of that body rate reaches the explicit
worksheet angle, releases it, verifies neutral keyboard/ACA state and continues
to first N68. It records all original inputs, gyro rates and simulation clocks.

Baseline input evidence: 10.105955° during the first key hold, 174.019757° during
the second. The latter 0.2-second release readback still has roughly 19°/s yaw rate; neutral ACA does
not imply a settled 174° pose. At first N68 the beam points toward the Moon
(cosine about +0.541), but source range is about 59,289 ft, beyond the unchanged
50,000 ft lock condition. Neither range nor velocity flags become valid during
this run; R3 remains `+99999`.

**This is an input-path/gyro experiment, not full checklist or settled-pilot
fidelity.** Compensating/braking attitude commands, further antenna choices or
landing/site decisions are not invented to force radar validity. Source-derived
pilot maneuver fidelity and subsequent acquisition remain the practical blocker
before any operational V57 decision. A Windows comparison has not been used.

To reproduce the optional run, stage another original runtime using
`tools/stage_nassp_mission.py` with the same explicit data/profile/textures and
`--output build/native-pdi-protocol/runtime-pdi-yaw`. Then pass that runtime and
the unchanged supplied PDI scenario to `Tests/NativePDIProtocol.py`, adding
`--checklist-yaw`. Use a separate candidate build/runtime with its pinned
adapted source and original candidate data. The original three CTest gates use
the default observational scope; the optional yaw experiment is separately
verified and archived. Both stop before V57 and fresh-reload with zero steps.

The final-tree suite and per-profile default/yaw evidence are recorded in
`upstream/validation-pdi-protocol-followup-macos-2026-10-07.json`. The earlier
`validation-pdi-protocol-macos-2026-10-07.json` remains the historical `4650f20`
checkpoint, not the final corrected harness result.

The candidate yaw-input run records 10.105955° / 174.224639° during the key holds,
with 18.69°/s after the second release. Its original first-N68 range is
about 60,416 ft, also outside the unchanged lock condition.

Final validation on 2026-10-07: baseline **91/91** in 502.79 s;
candidate **91/91** in 510.97 s, each serial. The final
25-check contract suite and both separate yaw-input runs/fresh zero-step reloads
passed. Both profiles retain invalid radar data and unavailable N68 dH at this
checkpoint. Raw evidence is archived at
`<DEPENDENCY_CACHE>/demo-validation/pdi-protocol-followup-20261007`.

## Instrumentation follow-up: release, rate settling and attitude overshoot

The earlier 91/91 baseline/candidate suites are historical evidence for
`fbe1c7f`; they are not rerun or claimed for this instrumentation change. The
current final tree passes all 34 synthetic contract checks (the original 25 plus
nine settling checks), both matched three-gate bounded CTest groups, and the two
separate instrumented yaw runs/fresh zero-step reloads. CI remains on hold.

The probe preserves the original input logic and adds no waits. All 52
input-event fields/timestamps are exactly equal to each profile's archived
`fbe1c7f` yaw run. It records one additional observation per existing 1/60 s step
for eight seconds after each release: body-to-global attitude matrices, original
ACA proportional output/out-of-detent, LGC output channels 05/06, original PGNS
jet requests/driver/final requests and RCS levels. Read-only test observers access
the existing protected state through valid member pointers; no derived-object
casts or NASSP accessors/source edits are introduced.

The verifier independently maps channels 05/06 to PGNS jet requests and checks
all 480 samples per release. It distinguishes the **0.2 s neutral-input readback**
from a diagnostic yaw-rate settling observation: absolute body-Y rate below
0.1 deg/s continuously for one second. This is a test observer criterion, not a
flight requirement, full three-axis attitude certificate or operational dH limit.
Original PGNS pitch/roll guidance continues during the observation.

| Target/profile | Key-held gyro integral | First low yaw rate after release | One-second dwell verified | Matrix yaw projection at +8 s | Full rotation error to fixed-start-frame target at +8 s |
| --- | --- | --- | --- | --- | --- |
| 10°, both | 10.106° | 2.567 s | 3.567 s | 22.399° | 12.400° |
| 174°, baseline | 174.020° | 3.783 s | 4.783 s | 209.315° | 35.361° |
| 174°, candidate | 174.225° | 3.767 s | 4.767 s | 209.636° | 35.673° |

The unwrapped yaw projection is measured from the recorded attitude matrices in
the frame at input start. Original right yaw is a negative body-Y rate and a
positive matrix angle in Orbiter's left-handed frame. The initial postprocessing
sign assumption was corrected against those recordings; the final independent
verifier was reapplied to the unchanged simulator JSONL, and the initial analysis
reports remain archived with explicit names. The full rotation error compares
against an ideal pure right-yaw rotation in the fixed start frame; it does not
remove subsequent PGNS pitch/roll guidance or establish a flight attitude target.

**Automatic braking works.** In the 174° runs, original LGC braking requests
appear at release +0.083 s / +0.117 s; nonzero RCS levels follow at +0.100 s /
+0.133 s. The approximately 19 deg/s values were sampled at +0.2 s, while braking
had only just begun. There are no stuck key, ACA or channel-031 breakout bits.
The eight-second post-release gyro integrals add 35.301° / 35.417°; the 10° run
adds 12.293°. At the first verified one-second yaw-rate dwell, matrix yaw is
209.542° / 209.866° for the 174° cases. Thus the current stop rule validates a
key-held angular integral and neutral release while overshooting the requested
settled orientation. Extra pilot counter-thrust is not required to demonstrate
braking and has not been added.

The recorded beam/nadir cosine moves from +0.757 at the second release to
+0.475 / +0.472 at the verified dwell, then +0.481 / +0.478 at +8 s. First N68 is
still reached at 1209.050 s / 1209.017 s with cosine +0.541 / +0.539 and original
range about 59,289 / 60,416 ft. Range/velocity flags remain false and N68 R3 is
`+99999`. These are unchanged-model observations, not calibrated flight parity.
The existing 50,000 ft lock cutoff remains separate from any operational dH
criterion. This correlation supports an input stop-rule/pointing explanation;
it does not prove that a corrected yaw alone will acquire radar.

The next minimal harness correction is to validate the terminal matrix attitude
and account for the original controller's measured automatic-braking distance
when choosing release timing. Use the original ACA path and avoid fixed
counter-thrust, pose writes or an invented operational dH envelope. The intended
pilot attitude frame/target and terminal tolerance still need to be made explicit
before claiming checklist fidelity. No control correction is included in this
instrumentation checkpoint, no V57 is sent, and no Windows comparison is needed
to explain the early-rate sample.

Reproduce with the same pinned toolchains and original data, building only
`native_pdi_protocol_probe`. Stage a separate `runtime-pdi-settling` and run the
existing Python runner with `--checklist-yaw`; use a separate candidate build and
runtime. Run the 34-check contract suite and the same three bounded CTest gates.
Final evidence and hashes are recorded in
`upstream/validation-pdi-yaw-settling-macos-2026-10-07.json` and archived at
`<DEPENDENCY_CACHE>/demo-validation/pdi-yaw-settling-20261007`.

## Separate relative-yaw target diagnostic (2026-10-07)

The selected original Apollo 11 LM workbook distinguishes the relative maneuvers
at rows 55 (`Yaw 10 deg right for SBD`) and 169 (`Yaw Right 174 deg`) from the
explicit FDAI RPY maneuver at rows 32 and 52–53. Both yaw rows have position -1
and automatic 0. The original `checklistControllerHelpers.cpp` treats position -1
as a manual/no-readback item; it contains no hidden angle acquisition routine.
The original `LEM.cpp` NUMPAD3 path feeds the ACA, and `lmscs.cpp` maps yaw rate
to Orbiter body Y, the thrust axis. This supports a relative body-axis maneuver,
not an absolute FDAI reading of 10 or 174 degrees. The [Apollo 11 Mission
Report](https://apollojournals.org/alsj/a11/a11MIssionReport_1971015566.pdf), printed
page 108, independently describes the powered-descent yaw-around as a manual
maneuver using the hand controller's proportional rate output.

The test defines its terminal yaw component explicitly: form the relative
rotation `Rstartᵀ Rcurrent`, convert it to unit quaternion `[w,x,y,z]`, and unwrap
`2 atan2(y,w)` about input-start body Y. The C++ driver uses the equivalent matrix
projection; the independent Python verifier reconstructs the quaternion. Ongoing
PGNS pitch/roll appears as swing and is recorded through the full quaternion,
body-to-global matrix and full rotation error. A pure fixed-start-frame yaw pose
is **not** certified: its observed error at the second acquisition is 3.42°
(baseline) / 3.25° (candidate), largely outside this yaw component. The original
checklist does not supply a quaternion convention or a numeric terminal tolerance;
these are explicit diagnostic definitions, not operational flight specifications.

Use the mutually exclusive `--yaw-target` option for this separate driver. The
original `--checklist-yaw` held-gyro-integral experiment and the default no-yaw
protocol remain available. The driver releases original NUMPAD3 early, lets the
original DAP brake, and uses bounded right-only trim pulses when short of the yaw
target. Its predictor uses a fixed conservative 4°/s² estimate and 0.25 s lag
allowance; those are test-driver values, never changes to NASSP physics. It sends
no left yaw, counter-thrust, direct RCS commands, pose/rate writes or LGC memory
writes. The predictor was fixed before the candidate run and was not tuned to
radar validity.

The independent verifier requires target error ≤1°, measured yaw rate ≤0.1°/s,
and neutral original ACA continuously for one second, then verifies the same
conditions throughout eight more seconds. These are diagnostic observer criteria.
Acquisition begins inside a tighter 0.75° driver margin. The first baseline
attempt used the outer 1° margin, stopped at 9.034°, and failed the dwell at
8.999934° after low-rate drift. That failed trace and blocked save are preserved;
the corrected driver added a trim pulse, while the verifier's 1° criterion stayed
unchanged. All successful simulation JSONL files remain immutable. A final
analysis pass added pulse-hold and continuous neutral-window checks; both the
initial runner reports and final verifier hashes are archived.

| Observation | Baseline | Candidate |
| --- | ---: | ---: |
| 10° diagnostic acquisition | 9.425319° | 9.425319° |
| 10° yaw after eight seconds | 9.125857° | 9.125857° |
| 174° diagnostic acquisition | 173.561761° | 173.285416° |
| 174° yaw after eight seconds | 173.744131° | 173.549584° |
| Original right-yaw pulses, first / second maneuver | 2 / 11 | 2 / 10 |
| First observed original range + velocity data good, elapsed s | 1120.916667 | 1124.083333 |
| First N68 elapsed s | 1209.033333 | 1209.033333 |
| First N68 original radar range, ft | 37263.365167 | 37784.640150 |
| First N68 beam/nadir cosine | 0.861741 | 0.862034 |
| First N68 dH, ft | -1832 | -1171 |

Both original radar validity flags and LGC discretes are true at first N68.
Previously, the exact-input harness settled near 22.4° / 209.5–209.9° and reached
N68 with invalid radar and +99999 dH. The new yaw-input policy is sufficient to change the acquisition outcome with
original models and data. That policy combines earlier release, bounded right-yaw
trim pulses and added neutral dwells; this comparison does not isolate release
time as a single causal factor. The 48 non-yaw input events preserve their
semantics, while their timestamps differ: final ENTER by +0.167 s, ignition PRO
by +0.067 s, and N68 inputs by approximately ±0.017 s, depending on profile. The
unchanged exact-input regression demonstrates reproducibility of the prior
policy; it is not a single-factor causal control. It does not certify historical flight parity,
full checklist fidelity, an operational dH envelope, or readiness to send V57.

At `bd010cd`, the focused validation comprised 42 synthetic contract checks (all
previous 34 retained, eight independent quaternion/dwell/input regressions added),
both target runs and fresh zero-step reloads, both preserved exact-input runs and
reloads, and the three default protocol CTest gates per profile. The exact-input
run's 52 input events and times match `9b6d484` in each profile. All 2288 event
matrices, body rates, original controller/jet values, DSKY displays and radar
validity flags also match exactly. Small altitude differences remain below
0.00018 m; therefore whole JSON records are not claimed byte-identical.
The full 91/91 suites at `fbe1c7f` remain historical at this checkpoint; no broad
suite or CI run is attributed to `bd010cd`. The final validation after the scope
guard correction is recorded separately below.

Reproduce by staging a fresh isolated runtime with the existing
`tools/stage_nassp_mission.py` command, then run the existing Python protocol
runner with `--yaw-target`. Use separate outputs for `--checklist-yaw` and the
default CTest fixture, to retain the independent evidence. No CMake/CI test count
or original 2000 ft diagnostic was changed.

Machine-readable results:
`upstream/validation-pdi-yaw-target-macos-2026-10-07.json`.
Raw evidence archive:
`<DEPENDENCY_CACHE>/demo-validation/pdi-yaw-target-20261007`.
A verified local bundle and evidence mirror are under
`<LOCAL_PATH>`.

The bounded relative-yaw/radar checkpoint is complete. The next useful action is
to review this causal result before choosing another validation scope. Full
visual/checklist fidelity and the operational V57 decision remain outside this
diagnostic. Windows comparisons, CI work and pushes remain deferred.


## Final validation after requested-variant guard correction

The runner now independently checks that the logged target-driver flag equals
its requested `--yaw-target` option, in addition to checking overall yaw scope.
An exact-input log cannot satisfy a target-driver request just because both
variants exercise manual yaw. The failure is retained in the verification report.
Two new contract checks reject this mismatch in both directions; all previous 42
checks remain, for **44 synthetic checks**. Both actual preserved exact-input
logs were also revalidated and rejected when supplied to the target-request scope
check. No simulator or C++ maneuver driver was changed by this correction.

Final full suites use the source at `bd010cd` plus that Python scope guard and
its tests. All graphics tests run serially, one profile after the other:

| Profile | Full CTest | Failures / skips | Wall seconds | Separate target variant |
| --- | ---: | ---: | ---: | --- |
| Baseline | 91/91 | 0 / 0 | 490.26 | Passed, fresh reload verified |
| Candidate | 91/91 | 0 / 0 | 491.30 | Passed, fresh reload verified |

The 91-test CTest suite contains the **default no-yaw protocol**. The target
variant is explicitly run and retained separately in `runtime-pdi-target-final`
for each profile; it is not included in the 91-test count. Both final target runs
reproduce the earlier target-policy attitude/rate/controller records exactly and
again obtain original radar data-good and dH -1832 / -1171 ft at first N68.
The target protocol sends no V57 and chooses no operational dH envelope.

The full suite also retains the pre-existing, separate original descent
regression with its documented 2000-ft diagnostic bound and V57/approach actions.
That regression is unchanged; it is not the target protocol or an operational
flight approval. Original insertion, liftoff, descent, MCC display restoration
and visual checks pass in both profiles.

The conclusion remains at the yaw-input-policy level: earlier release, bounded
right-yaw trim pulses and additional neutral dwell together change the observed
outcome. Non-yaw input semantics are preserved, with the timestamp differences
recorded above and in the final report. The old exact-input regression establishes
reproducibility, not single-factor release-time causality. The controller uses
attitude/rate readbacks and original keys, independently of radar validity or dH.
The target frame and tolerance remain diagnostic, with no full pilot/checklist
or historical flight-parity certificate.

Final machine-readable report:
`upstream/validation-pdi-yaw-final-macos-2026-10-07.json`.
Raw full-suite logs/JUnit, separately retained target traces/saves/reloads,
negative scope checks and source snapshots:
`<DEPENDENCY_CACHE>/demo-validation/pdi-yaw-final-20261007`.
Verified evidence mirror and incremental local bundle:
`<LOCAL_PATH>`.

This automated Mac validation phase is complete. Normal user window-close
interaction and physical-controller/manual-flight QA remain unverified because
this executor has no CUA/controller tools. CI, Windows comparisons and pushes
remain deferred; no NASSP source, physics, ROM or supplied scenario changed.

## Original pilot-input procedure and source-chart radar gate, 2026-10-09

This opt-in procedure extends the first-N68 checkpoint from `90199e2` through
original radar incorporation and guided descent to a manual handoff. It uses
the original Apollo 11 PDI T-15m scenario, Luminary099 ROM and LM workbook.
NASSP source and simulation equations are unchanged in this phase. The earlier
default no-yaw, exact-keyboard and quaternion-target variants retain their
separate scopes. The new pilot variant is registered as `native_pdi_pilot` with
its own staging fixture, runtime and verifier, alongside 16 new synthetic
radar-envelope and complete-input contract checks. The prior 44 checks remain.

The final flown timeline's printed page 9, July 1 revision G, shows a **174°
right yaw**, verified directly in the scan. Its OCR says 176° incorrectly.
The crew G&N dictionary, PGNS-44, calls for windows up, V57, then V16N68 to
observe convergence. The dictionary and original native ACA/RGA path identify
yaw about the LM thrust/body-X axis; this corresponds to right rotation around
negative Orbiter body Y. The 1969 technical debrief describes a 5°/s crew
command during the yaw-around. The driver uses an approximately 5.16°/s
quantized command through SDL virtual-stick input, native VESIM, the original
ACA, original LGC counter and original RCS controller.
[Final flown timeline](https://www.ibiblio.org/apollo/Documents/Apollo11-LM-TimelineBook-excerpts.pdf),
[crew dictionary](https://www.ibiblio.org/apollo/Documents/ApolloXI_FlightCrewG%26NDictionary_5-29-1969.pdf),
[landing transcript and crew debrief](https://apollojournals.org/alsj/a11/a11.landing.html).

The observer integrates the measured original body-yaw rate **including neutral
braking**. Bounded right-input pulses and neutral dwells acquire 10° and 174°
within explicit 1°/0.1°/s observation tolerances, followed by eight seconds of
neutral observation. Windows-up geometry is independently checked from the
original rotation matrix and forward-window +Z direction; original powered
GASTA/FDAI angles are also retained. These tolerances and the stopping predictor
are test policies, not published Apollo flight requirements. The yaw controller
does not use radar validity or ΔH, and it writes no pose, angular rate, thruster
level or LGC memory. The full historical pilot trajectory is not certified.

Original NASSP writes the signed C RHC counter directly. Original AGC
`SignExtend` masks it as a one's-complement word, so raw −20 decodes to −19.
This driver therefore uses axis 14823, which produces raw −20 and the decoded
−19 command under the original normal-rate law. This is input quantization,
not a new simulator fix or a demonstrated Mac-specific defect. The test log
records both representations. Signed counter serialization initially exposed
two logger parsing errors; those failed traces are preserved separately.

Apollo 11 mission rule **5-89**, printed 5-13, ties radar acceptance to the
altitude-dependent premission limit carried in the crew chart. LM crew chart 9
(June 27 final data, printed 50) and the final flown timeline give a 17,500-ft
plateau covering the 40,700-ft through 27,500-ft tabulated rows. The driver
accepts only complete, nonsentinel N68 ΔH with absolute value ≤17,500 ft while
original LGC HCALC lies in **27,500..40,700 ft**, original radar/LGC range and
velocity data-good agree, and incorporation is still inhibited. It does not
extrapolate the chart beyond that interval. Mission techniques memo 28D item 2
records that MIT had not independently verified this published envelope.
[Mission rules](https://www.nasa.gov/wp-content/uploads/static/history/alsj/a11/a11missionrules.pdf),
[crew chart](https://www.ibiblio.org/apollo/Documents/Apollo%2011%20Crew%20Charts%20And%20Console%20Charts.pdf),
[MTM 28D](https://www.ibiblio.org/apollo/Documents/mission_techniques_memo_28d.pdf).

The V57 key sequence changes original Luminary FLAGWORD11 bit 8 (LRINH) from
inhibited to permitted. V57 returns the display to N63, so the next original
input is the dictionary's **V16N68**. The first extended run omitted that
reselection and stopped on the wrong display; the preserved trace proves a
driver omission, not failed radar convergence. The corrected run observes
N68 for 30 seconds and checks net reduction in absolute ΔH. This diagnostic
observation does not certify every mission go/no-go condition or radar
adequacy for the entire descent. Read-only HCALC is decoded at octal 3534/3535,
with original double-precision scale 2^24 m. The downloaded Luminary099 source
blobs for the rate law, erasables, V57 flag and update path are hash-verified
and retained with the evidence; the supplied binary is never reassembled.

After the convergence observation the driver returns to N63, records throttle
at ignition +6m40s, exercises original PGNS ATT HOLD/AUTO switches, and explicitly
defers the manual-control evaluation. At +8m it monitors N68; it observes the
original P64 transition before selecting landing-radar HOVER. KEY REL restores
the original N64 approach display, followed by the +9m PRO. It stops at the
first observed original HCALC ≤2,000 ft and saves `NativePDIPilotHandoff.scn`.
HCALC and DSKY refresh asynchronously, so the reported handoff altitude can be
below 2,000 ft and N64 may still show the previous value. No ground-contact,
P66/manual landing, AGS DEDA or full-checklist result is asserted. Fresh zero-step
reload verifies all five original vessels and original LGC memory/channels and
mission clock. Source assets, ROM, mission config and workbook remain byte-identical.

Final complete suites use the source at `90199e2` plus the pilot test additions.
Graphics and simulations run serially, one pinned profile after the other.

| Profile | Full CTest | Wall seconds | Yaw after neutral observation, 10° / 174° | ΔH before / 30 s after V57, ft | Handoff LGC altitude, ft |
| --- | --- | ---: | ---: | --- | --- |
| Baseline | 94/94, 0 failures/skips | 571.53 | 9.178758 / 173.870740 | -1832 → +83 | 1918.061, P64 |
| Candidate | 94/94, 0 failures/skips | 573.93 | 9.178758 / 173.818977 | -1171 → +170 | 1978.757, P64 |

The 94-test suites include both the default no-yaw checkpoint and the new
pilot procedure; the earlier explicit quaternion/exact-keyboard diagnostics
are not added to that count. Machine-readable report:
`upstream/validation-pdi-pilot-macos-2026-10-09.json`. Raw logs/JUnit, primary scans and source blobs,
failed driver diagnostics and source snapshots:
`<DEPENDENCY_CACHE>/demo-validation/pdi-pilot-20261009`. Verified local bundle and evidence mirror:
`<LOCAL_PATH>`.


To reproduce a profile's complete registered pilot procedure:

```sh
ctest --test-dir build/native-pdi-protocol -R 'pdi_pilot' --output-on-failure -j1
```

The CTest fixture stages a fresh original scenario under `runtime-pdi-pilot`.
`Tests/NativePDIPilot.py` audits the first-N68 prefix independently, then the
complete key/control stream, source gate, original flag change, N68 convergence,
P64/HOVER ordering and handoff. Its `--verify-existing` option checks a completed
log and fresh reload without repeating the simulation. Neither the prior
default verifier nor its requested-variant guard accepts a pilot log as a
default, exact-keyboard or quaternion-target result.

The remaining pilot phase requires landing-site/LPD judgment, manual-control
evaluation, AGS DEDA and physical-controller/window interaction QA. This executor
has no CUA or physical-controller tools, and no independently supported scripted
manual-landing policy has been established. The saved handoff and original
workbook are the concrete starting point for that work. Windows comparisons,
hosted CI activation, visibility changes and pushes remain deferred.

## Handoff persistence and processor-resume blocker, 2026-10-10

The `13866c0` reload check preserved vessel classes, LGC memory/channels and
mission clock, but its helper explicitly excluded `CYCLECOUNTER`. It did not
check radar range, both pointer readings, gimbal motor flags or a continued
flight. Its earlier green result therefore does not establish faithful resume.
The white X-pointer correction remains committed at `e9cc62d`.

Archived baseline and candidate handoffs reproduce four changed fields on a
zero-step reload: LR range, the right pointer pair and both gimbal motor flags.
Both pristine pinned sources contain the same three parsing defects. RANGE
matched the RANGEGOOD prefix and reused a failed scalar parse; the right pointer
used an offset inside its key; MOTORRUNNING compared the terminating null with
the space before its value. Exact-token/complete-value loading fixes these
without changing their instrument equations. A standalone original-class
regression also checks malformed/prefix tokens and both pointer sides.

A new 31-sample, 0.5-second neutral comparison exposed further missing state.
The unsaved private DPS throttle command caused a zero-thrust first step before
DECA recalculated it. New checkpoints preserve `THRUSTCOMMAND`. LM saves move
the SDK origin temporarily to the mesh center; cold reload also lost the cached
CoG and the fuel weight at the last 100 kg update. An initial recalculation
experiment still differed from the candidate's cached CoG by 13.3 mm. New
`MASSCG_STATE` records preserve CoG, PMI and that last-update fuel weight as one
complete record. Restoration reuses the existing physical-setup body before
the first systems frame. The original calculations and 100 kg threshold remain
unchanged. Older saves lack these fields and cannot establish lossless resume.

A native SDK origin shift also left cached altitude/horizon readings stale while
paused. The geometric origin-shift regression fails before the correction and
passes after `ShiftCentreOfMass` refreshes its surface parameters. ROMs, supplied
mission/scenario/workbook bytes and the equations remain unchanged. No landing
policy or Windows comparison is introduced.

The stronger gate checks exact serialized radar/pointer/gimbal/DPS/mass-state
persistence, all original vessels, LGC state and clock, then compares fresh
neutral continuation with the uninterrupted run. It retains first-step thrust,
range/data-good flags, both pointers, gimbal commands/motors, body rates and
rotation matrices. SDK decimal output precision motivates the numerical observer
bounds in `Tests/NativePDIPersistence.py`; they are not Apollo flight tolerances.
Raw RHC -20 is now required alongside its original decoded -19 value.

**Faithful processor resume remains blocked.** Pinned
`ApolloGuidance::LoadState` deliberately skips the saved AGC cycle counter until
CDU FIFOs are saved/loaded. `yaAGC/agc_engine.c` holds three FIFO queues and their
checker in module-static state, with an explicit note that they need to
move into `agc_t`. Fresh reload resets the cycle counter to zero. The gate now
records and rejects that loss even when a short instrument trace happens to
match. The candidate also shows a two-count forward-CDU difference late in the
neutral trace; its causal link to the incomplete processor snapshot has not
been isolated. At the `e01b5b2` milestone the tolerance was retained and the
ACA pulse was deferred.

**Historical plan, superseded for uninterrupted ACA by `c5d7b7b`:** the plan
below records the then-current dependency on saved-state continuation. The
snapshot gate remains a prerequisite for a **resumed** ACA test. The separate
[uninterrupted P64 workflow](#uninterrupted-p64-aca-demo-2026-10-10) has since
validated pulse/release in the same live simulation without loading a save.

> The next step is per-AGC FIFO/timing snapshot and restore, tested with the
> original multi-vessel scenario, followed by this continuation gate. A bounded
> original-ACA pulse/release test follows only after that gate passes. Manual
> landing, physical controller and graphical hit-testing QA remain unvalidated.
> CI activation, Windows comparisons and pushes remain deferred.

Final results and evidence hashes are recorded in
`upstream/validation-pdi-persistence-macos-2026-10-10.json`. Logs, scenarios,
failed intermediate traces, final JUnit and source snapshots are archived under
`<DEPENDENCY_CACHE>/demo-validation/pdi-persistence-20261010`,
with a verified mirror and local bundle under
`<LOCAL_PATH>`.

The five NASSP files form a removable `08-state-persistence.patch` in both
pinned inventories. The seven earlier patch files remain byte-identical. Both
stacks reconstruct every inventoried blob (102 baseline, 94 candidate), and
applying the persistence overlay to pristine upstream before the seven native
adapters produces the same result. The native SDK cache fix lives in Orbiter.
See [the overlay workflow](upstream-native-adoption.md#persistence-overlay-2026-10-10)
for regeneration and removal paths.

Final complete suites ran with unchanged source/overlay hashes, one profile at a
time. Both include 68 synthetic contract checks and the original-class loader
regression. The sole failing test is `native_pdi_pilot`; it remains enabled and
fails on the incomplete processor snapshot.

| Profile | Full CTest | Wall seconds | Saved / reloaded AGC cycle counter | Maximum neutral range difference, ft |
| --- | --- | ---: | --- | ---: |
| Baseline | 94/95, 1 failure, 0 skips | 592.90 | 124205511 → 0 | 0.030255 |
| Candidate | 94/95, 1 failure, 0 skips | 596.37 | 124034844 → 0 | 0.045979 |


## Pending-state scope investigation, 2026-10-10

Starting at clean `e01b5b2`, the bounded investigation found that FIFO-only
serialization cannot establish faithful processor resume. No AGC or PCM source
was changed. The strict regression stays enabled; the latest complete suites
remain 94/95 in both profiles, with `native_pdi_pilot` the sole failure.

`agc_engine.c` is compiled separately into `LEM.dylib` and `Saturn5.dylib`.
Both binaries contain their own non-external FIFO/checker symbols. The normal
LM/CSM pair therefore uses separate module-local queues, while multiple AGCs
of one vessel module share them. This corrects the earlier process-global
description. The same problem applies to gyro/coarse-alignment pulse statics.

The diagnostic in `tools/nassp_agc_pending_state_probe.c` includes the exact
engine source and calls only its FIFO helpers. In both profiles it confirms
that AGC B can consume a pulse queued for A. A complete private FIFO/phase copy
matches every sample of two 1,500-cycle traces, including ordered slow/fast
positive/negative counts and a wrapped, previously full ring. Restoring only
the CPU counter produces a different trace. This is an ownership/state-contract
probe; it is not a production serializer or a complete AGC/PCM resume test.

The minimal faithful-continuation contract must cover these coupled fields:

| State | Current persistence gap |
| --- | --- |
| Per-AGC FIFO and scheduling | Three 128-word queues, ring head/size, interval phase, uint64 next-update deadlines and checker are module-static; cycle counter is saved but deliberately not loaded. |
| Gyro and coarse-alignment output | GyroCount/OldChannel14/GyroTimer; three signed residual CDU commands; ImuCduCount/ImuChannel14 are also shared statics. |
| DSKY state | DskyTimer, DskyFlash and RestartLight are already per-AGC but omitted by the scenario serializer. |
| CPU wrapper phase | Both LM and CSM lose LastCycled and its fractional remainder. A new snapshot must save its age and validity and rebase to the new simulation epoch. |
| External interrupt phase | LM/CSM PCM lose last-update age and word/frame counters. Native DOWNRUPT is generated by PCM, while the engine's DownruptTime creation/service is disabled for NASSP. |

Existing erasable memory, channels, pending instruction/delay flags, scaler,
interrupt requests and warning/trap state must remain intact. ROM-derived
parity and client/thread pointers are not checkpoint payload. Pending telemetry
buffers must also be retained if a snapshot is taken away from an idle byte
boundary. Snapshotting must capture the CPU, wrapper and PCM at one completed
frame boundary after any accelerated worker has finished; the guidance save
method itself currently has no cycle-mutex guard.

A versioned extension should stage and validate every required record before
restoring its clock and queues together. Direct ring storage needs three exact
records, 128 packed 32-bit words each, bounded head/size/checker/interval fields
and explicit uint64 deadlines. Preserve sign/rate bits, overdue pending work,
the original 13/13/14 and 213/213/214 intervals and existing queue overflow
behavior. Missing extensions must keep older scenarios loadable with legacy
startup, while recording that faithful continuation is unavailable. Partial,
unknown or malformed extensions must never enable a counter-only restoration.
Init, power-loss and same-class multi-vessel ownership need dedicated regression
coverage; this investigation does not introduce a new hardware-reset policy.

The candidate's two-count difference is now localized to the forward CDU:
ticks 28 through 30 show 334 uninterrupted and 336 resumed, with matching
lateral counts. The original pointer filter converts this into the observed
0.013928, 0.026114 and 0.036777 display differences. Tick 7 instead has equal
raw counts and a 1e-6 display-text difference. No gate tolerance was relaxed.
The cause of the two-count change remains unisolated: the current IMU path
writes read counts directly into erasable memory, and no live FIFO occupancy
was captured at the original handoff. Wrapper fractional-cycle timing and PCM
interrupt phase are candidates, not proven causes.

A FIFO-only relocation/serializer is a small removable change; the complete
contract crosses engine ownership, pulse scheduling, wrapper epoch rebasing and
PCM interrupt generation. That wider implementation is paused at this boundary.
No equation fork is indicated, but a coherent upstream persistence change and
multi-vessel regressions are required before claiming faithful resume.

The bounded demo alternative is the original uninterrupted flight through
source-chart radar incorporation, original P64 and the existing below-2,000 ft
handoff. Both profiles already have that trace and its 31 neutral samples.
Saved checkpoints remain useful archival evidence but carry the resume limit.
Stop at that handoff; resumed ACA, manual landing and hardware/hit-testing QA
remain pending. A future snapshot gate should cover several DOWNRUPT frames,
coarse bursts and at least two 1.28-second DSKY periods, in addition to the
current exact zero-step and bounded instrument comparisons.

The contract, raw source hashes, module symbols and existing trace references
are in `upstream/validation-agc-pending-state-macos-2026-10-10.json`. Diagnostic
source, binaries and logs are archived under
`<DEPENDENCY_CACHE>/demo-validation/agc-pending-20261010`.
No Windows comparison, CI activation, push or ACA pulse was performed.


## Uninterrupted P64 ACA demo, 2026-10-10

The pending-state scope boundary remains unchanged: no broad AGC/FIFO ownership
or snapshot implementation is introduced. A separate opt-in `--pilot-aca`
procedure starts the original PDI T-15m scenario and continues in that process
from original pilot yaws, source-chart radar incorporation and P64 through a
bounded ACA pulse/release. `native_pdi_uninterrupted_aca` has its own staging
fixture and verifier; `native_pdi_pilot` retains its strict saved-state check.
The shared probe's default and prior pilot/resume options retain their scopes.

The crew G&N dictionary PGNS-45 permits manual PGNS ATT HOLD during P64.
Workbook readiness requires all three ATTITUDE CONTROL switches at MODE CONT,
PGNS selection, normal rate scaling and minimum deadband. Original SCCA sets
the ATT HOLD discrete, the ACA supplies out-of-detent/proportional input and
Luminary's P-axis DAP selects manual rate command, then RATEDAMP/attitude hold
when the stick returns to detent. The flown timeline places its later manual
control near TIG +10 minutes and roughly 500 ft. This diagnostic deliberately
tests earlier at the existing below-2,000 ft P64 handoff; it does not reproduce
that landing decision or redesignation policy.
[Crew dictionary](https://www.ibiblio.org/apollo/Documents/ApolloXI_FlightCrewG%26NDictionary_5-29-1969.pdf),
[flown timeline](https://www.ibiblio.org/apollo/Documents/Apollo11-LM-TimelineBook-excerpts.pdf).
The corresponding original sources are `lm_channels.h:200`,
`lmscs.cpp:2075`, `lm_aca.cpp:48`, `LEMcomputer.cpp:238` and
`P-AXIS_RCS_AUTOPILOT.agc`, original pages 1430–1433. No ROM is reassembled.

After selecting ATT HOLD with the original virtual-cockpit callback, the probe
checks the supplied remaining readiness state. It observes one second at
neutral, commands virtual-stick axis 14823 for exactly 18 frames at 1/60 s,
then centers it for 240 frames. It records 321 phase samples including their
boundaries. The original ACA fraction is about -0.488071; channel-13 reads
produce raw RHC -20, decoded original one's-complement -19. These counter reads
are asynchronous, not an immediate input-frame echo. Observer guards require
read/release within 0.2 s, measurable right yaw above the neutral baseline,
original PGNS requests/driver/jet response, opposing braking torque and a final
one-second yaw-rate dwell within 0.15 deg/s. The altitude guard is 1,200 ft with
no ground contact; these are test bounds, not Apollo flight requirements.

The logger's torque observer follows original `Vessel.cpp:4035` force × arm
convention. The first audit caught an incorrect arm × force sign and a verifier
that treated proportional ACA fractions as counter counts and assumed instant
counter reads. Those initial traces/rejections are preserved. Only diagnostic
units/conventions were corrected; the final registered tests rerun both
uninterrupted flights using the corrected logger. There is no change to NASSP
equations, actual input scaling, pulse duration, physics or pending-state logic.

All measured attitudes, body rates, GASTA/FDAI values, RCS levels, channel
readbacks, cycle counts, both X-pointers, radar and gimbals are retained. CPU
cycle history and phase clocks must continue monotonically in the live process.
Thirteen focused mutation contracts reject omitted frames, a restarted clock,
wrong mode, retained stick/counter input, missing jet/braking response,
unsettled release, P66 and invalid attitude matrices. The earlier 68 contracts
remain. Synthetic contracts test the verifier; the real original-vessel traces
provide the command/motion evidence.

Use [the demo workflow](original-mission-demo.md#original-pdi-through-p64-uninterrupted-workflow)
to stage and launch. Saved handoffs remain archival only for this result; no
save is reloaded before the pulse. Neither a green uninterrupted gate nor prior
memory/channel comparisons establish faithful processor resume. Physical
controller calibration, graphical control hit testing, landing-site/LPD
judgment and manual landing remain unvalidated. No Windows comparison, hosted
CI, repository visibility change or push is part of this phase.


Final-tree verification: both profiles pass the two registered stage/ACA tests
and seven applicable checks (default stage/PDI, native world, original-class LM
persistence and the three contract modules). The contracts comprise 81 cases.
The strict resume verifier was rerun with each final probe against the prior
handoff, using fresh zero-step reload and neutral continuation; both exit 1 on
the missing cycle/FIFO snapshot. Its source bytes remain unchanged. The registry
now contains 98 tests; the previous full-suite 94/95 result is historical,
not a new 98-test full-suite result. Unrelated full graphics/suites were not
repeated for this diagnostic-only change. Core executable hashes match e01b5b2.

| Profile | Neutral baseline max yaw rate, deg/s | Peak right yaw, deg/s | Final yaw rate, deg/s | Pulse plus release body yaw, deg | Lowest LGC altitude, ft |
| --- | ---: | ---: | ---: | ---: | ---: |
| Baseline | 0.026881 | 1.629219 | 0.043318 | 0.511281 | 1611.713 |
| Candidate | 0.063007 | 1.663703 | 0.009015 | 0.412498 | 1660.925 |

Both show the first right-yaw RCS torque at pulse +0.216667 s, then opposing
braking torque after release. The original program remains P64 throughout.
Machine-readable measurements, source/input hashes and exact coverage are in
`upstream/validation-pdi-uninterrupted-aca-macos-2026-10-10.json`. Final and
initial failed-observer evidence are archived under
`<DEPENDENCY_CACHE>/demo-validation/pdi-uninterrupted-aca-20261010`,
with a verified local mirror/bundle under
`<LOCAL_PATH>`.


## Same-host window handoff, 2026-10-10

The diagnostic probe now has opt-in `--pilot-aca-window` and bounded
`--pilot-aca-window-test` modes. [The launch guide](original-mission-demo.md#original-pdi-through-p64-uninterrupted-workflow)
provides one staging/launch command. The earlier `4c31c55` launch-gap review is
superseded by this removable transport; the historical saved-state limitations
and strict resume failure remain unchanged.

The original T-15m scenario is launched once. WgpuClient/ProjectApolloMFD are
initialized before that launch, and its existing virtual cockpit/SDL window
render while the same diagnostic performs the same 1/60 s protocol. The
accelerated prefix renders every five simulation seconds; ACA observations
render at every fixed step. No NASSP/core/AGC source, equations, ROM, mission
configuration, scenario bytes or pose are changed for this transport.

An SDL filter isolates scripted input until handoff. An owned focus-gained
event prevents the normal background-window gate from disarming the scripted
stick; OS focus and flight-input events are filtered during automation. Manual
focus/capture behavior is restored after the virtual stick is detached. The
first graphical attempt correctly failed the original pilot prefix because
normal background gating disarmed that stick. This was fixed in the diagnostic
adapter, without weakening the pilot/ACA verifiers.

At the original neutral-release endpoint, an existing pause request is applied
on a paused tick. The diagnostic checks exact equality of simulation/mission
time, LGC cycles, attitude, rates, position and velocity across that boundary.
It removes only its owned virtual stick/map, republishes device loss, releases
native keyboard/mouse and joystick bindings, and requires neutral ACA with no
active yaw binding. The same host remains alive for the existing cockpit input
loop. F5 resumes/pauses; laptop L/J provides yaw; window close or manual Esc
exits. No second Launch or reload bridges the handoff.

Manual continuation pauses at 12 simulated seconds, calculated guidance
altitude at/below 1,200 ft or contact, and cannot be resumed past that guard.
No landing command or policy is injected. The bounded SDL lifecycle test checks
keyboard-to-original-ACA ownership and release, pause/unpause/focus behavior,
guard enforcement, native close/Esc paths and fresh repeated launch. It retains
the complete unchanged uninterrupted ACA verifier after each graphical run.

Real renderer readbacks are saved at original PDI and the paused P64 handoff.
They establish cockpit rendering and status text, not physical desktop
visibility or human control. CUA is unavailable here. Human mouse hit testing,
one short mapped input/release in the retained live state, physical-controller
calibration and landing remain unvalidated. Headless gates remain enabled;
faithful saved-state resume remains blocked. Windows, push and CI are deferred.


Both pinned profiles passed `native_pdi_window`, each with two full fresh
launches: baseline 463.99 s and candidate 464.56 s. Each also passed the nine
applicable headless/contract checks; the candidate headless run preceded only
the final synthetic Esc assertion correction. The four previous verifiers and
both core executables are unchanged. No full 99-test suite was rerun.

Baseline handoff retained cycle 124,734,577 at 1,611.71 ft HCALC; candidate
retained cycle 124,563,911 at 1,660.93 ft. Keyboard input after virtual detach
reached the original ACA and returned to neutral. Repeated input/state sequence,
clocks, CPU cycles, body rotation/rates, jets and ACA matched exactly. Whole logs
are not byte-identical: relative geometry varies by at most 0.276 mm, FDAI by
1.76e-10 degrees, and candidate cross-pointer serialization by 1e-6 in four
records. Locked-radar range varies by less than 0.001 ft; near-horizontal beams
with range-good false amplify the geometric variation. Its cause has not been
isolated; no verifier threshold was changed.

The guard uses original HCALC readbacks. Its first threshold-crossing pause was
at 1,186.43 ft baseline and 1,141.12 ft candidate; F5 could not bypass it. The
12-second and contact branches were not separately exercised. The captured
commander forward window shows starfield at handoff; flight-view correctness
was not assessed. The diagnostic changes no camera or vessel pose.

`upstream/validation-pdi-window-macos-2026-10-10.json` records the commands,
per-run checks, hashes, renderer evidence and limits. Evidence is archived in
`<DEPENDENCY_CACHE>/demo-validation/pdi-window-20261010` and
mirrored under `<LOCAL_PATH>`.
Automatic approval review rejected a redundant candidate headless repeat,
citing the already-green checks and retained job limit; that repeat was not
retried. Subsequent new checks ran one job at a time.
