# Stock Windows early steps: gimbal loader cause

The thermal and battery differences identified in the [bounded diagnosis](native-v9-thermal-diagnosis.md)
come from an existing native correction to a shared NASSP loader defect. No new
runtime change is required. The canonical native run restores the original
scenario's two running DPS gimbal motors. Stock NASSP v9 rejects both saved
flags, starts them during the first timestep and therefore omits their initial
power draw. The resulting startup energy difference propagates through the
original electrical and thermal equations.

The fresh stock Windows captures use the same original scenario, pinned
releases, physics configuration and full-precision 1/60 s fixed step. All three
closed normally. Their raw hashes and original index are retained. MJD tokens
and both AGC cycle counters match the native observations:

| Total step | CSM and LM AGC counters | Selected records differing in canonical native |
| --- | --- | --- |
| 2 | 2844 | 0 of 278 |
| 3 | 4266 | 1 Eagle radiator temperature |
| 11 | 15644 | 15 Eagle records; 1 Columbia chemical decimal |

At step 3, native's LM-ATCA-INV-Plate is `293.788674` K while stock saves
`293.788552` K. At step 11 the same +0.000122 K difference remains, and the
three battery energy offsets are already -0.3882, -0.1941 and -0.1849 J, close
to their 301-step values. Complete AGC and DSKY blocks match at steps 2, 3 and 11
for both vessels. These are bounded observations, not full mission acceptance.

Pinned `src_lm/lm_dps.cpp:926` checks
`strnicmp(line, "MOTORRUNNING", 13)`. The key has 12 characters; the thirteenth
comparison is the saved space against the literal's terminating NUL. The
condition fails, leaving the constructor's `motorRunning = false` unchanged.
The original scenario contains `MOTORRUNNING 1` for both pitch and roll. The
existing `08-state-persistence.patch:88` uses a 12-character comparison, checks
a whitespace boundary and requires one successful integer conversion. It
already exists in the protected `9e8d452` demo. Keep the fix: restoring saved
state is the correct native behavior, and the existing LM persistence gate
requires it.

The constructor, motor timestep, power predicate, system timestep and power draw
functions are identical to pinned upstream after newline normalization. Each
running actuator draws 17.5 W. The native observer confirms this sequence:

| Completed step | Canonical native versus stock-loader counterfactual |
| --- | --- |
| Load | Both restored running flags are 1 in canonical native, 0 in the diagnostic stock-loader case. No power has yet been consumed. |
| 1 | Canonical AC bus A has 35 W more pending load; the original 1.05 bus factor gives 36.75 W more inverter load. |
| 2 | The inverter heat-loss curve produces 5.5125 W more pending heat. Both motor cases now run and their subsequent AC loads agree. |
| 3 | The 750 J/K radiator receives 0.091875 J extra heat from the previous frame, initially adding 0.0001225 K. Later exchanger/radiation operations propagate that energy. |
| 7 onward | The original electrical distribution chain reaches the batteries. The initial energy offset persists; no sustained new motor-load difference exists. |

The diagnostic seed changes only those two motor flags to zero to reproduce
stock's failed load. It is explicitly labeled as modified mission state and is
never used as an acceptance fixture or substituted for the original mission.
Canonical and counterfactual runs use the same retained observer binary for
301 steps. In the counterfactual, all 144 selected Eagle tank chemical, battery
and radiator records match stock Windows **exactly** at steps 2, 3, 11 and 301.
This eliminates all 72 differing Eagle records from the earlier coast comparison.
Native's original-scenario control reproduces the earlier Eagle differences.
Additional diagnostic reads are verified against the earlier observer: every
observed canonical systems value agrees at all 302 points.

The conclusion is a corrected saved-state initialization, not a thermal or
battery integration defect. Do not remove the native loader fix, delay the
motors or alter equations to copy stock's bug. Its upstream removal path is
adopting the exact-token, complete-parse loader correction upstream, retaining
`Tests/NativeLMPersistence.cpp:37` coverage and then removing only the gimbal
loader hunk from `08-state-persistence.patch`. No new patch or test is added here.
The existing `native_lm_persistence` and `native_scenario_compatibility` checks
both pass in this phase.

A few final Columbia chemical decimal differences remain (13 in these fresh
native processes, versus three in the previous canonical run). They were not
explained or waived by this experiment. The IMU fractional remainder cause,
shared AGC scheduling/pending FIFO resume failure, physical controls and complete
graphical mission acceptance also remain outside this diagnosis. No new GPU,
VM, push, hosted CI or complete test-suite run occurred.

Raw Windows inputs, original/counterfactual saves, full-precision traces,
observer source/build commands, logs and the source audit are retained under
`evidence/early-step-comparison` in the task workspace. A debugger launch failed
in the sandbox; its authorized retry stalled and was terminated. The successful
replacement observer used untouched NASSP runtime libraries. Evidence hashes
and exact outcomes are in
[validation-v9-motor-load-macos-2026-10-11.json](../upstream/validation-v9-motor-load-macos-2026-10-11.json).
