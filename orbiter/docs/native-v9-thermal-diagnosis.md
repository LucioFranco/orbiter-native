# Bounded v9 thermal and battery diagnosis

This records the earlier diagnostic phase. The subsequent
[stock Windows early-step comparison](native-v9-motor-load-comparison.md)
locates the cause: stock ignores the original scenario's saved gimbal motor
flags, while the existing native loader correctly restores them. All selected
Eagle thermal and battery differences are reproduced by that startup effect.

The original 301-step [stock Windows comparison](native-v9-matched-parity.md)
contains small remaining thermal and battery differences. This phase isolates
initial values, serialization, mission-clock precision and display cadence. It
adds no NASSP or Orbiter runtime change and introduces no acceptance tolerance.
The source pins, original mission file and existing failing AGC resume evidence
remain intact.

All 278 selected tank chemical, battery and radiator records match Windows
exactly after the initial 1/60 s step. A zero-step native load changes only the
text of three original water-tank records. Every original/saved number in those
three records represents the identical binary64 value. This rules out a
numerical change from those initial parse/save differences; it does not prove
that every unobserved internal startup variable is equal across platforms.

After 301 steps the previous canonical native run differs in 75 of these
records: three tiny Columbia chemical values, and 66 Eagle chemical records,
three Eagle radiator temperatures and three Eagle battery energies. The raw
native-minus-Windows differences include:

| Eagle record | Difference after 5.016666666666654 s |
| --- | --- |
| DSC_BATTERY_C energy | -0.3883 J |
| DSC_BATTERY_D energy | -0.1941 J |
| ASC_BATTERY_B energy | -0.1849 J |
| LM-ATCA-INV-Plate temperature | +0.000107 K |
| LM-SCERA-ECA-Plate temperature | +0.000001 K |
| LM-SCERA-Plate temperature | +0.000001 K |

Battery temperature tokens agree. Chemical energy differences include values
larger than one floating-point unit; no blanket rounding classification or
waiver is made. Independent native processes also vary in a few final Columbia
chemical decimals, so a claim of complete byte identity is inappropriate.

An ephemeral observer records all six LM batteries (capacity, load, voltage,
current, temperature and heat), inverter 1 load/heat/voltage/current, four
pending heat loads and three radiator temperatures at full double precision.
It executes the real native host with the original scenario. It observes 302
points from load through step 301 and saves raw scenarios at steps 0, 1, 2, 3,
11, 31, 61, 121, 241 and 301. The observer and analysis stay outside production
source under the evidence directory; their removal is deleting that diagnostic
directory after preserving its evidence. No test or runtime patch is justified
until an actual defect is demonstrated.

Two controlled native runs use the same observer binary. One loads the
unchanged scenario; the other changes only the two mission-clock input tokens
to the exact floats stock Windows loads. This second seed is explicitly a
diagnostic counterfactual, not an acceptance scenario. All observed battery,
inverter, pending-heat and radiator values are identical at every observation,
with identical simulation time. The earlier all-278-record comparison also
finds identical serialized coast records between native clock cases. The known
13.763 ms mission-clock offset does not explain the observed differences within
this scope.

A separate pair uses the same binary with and without one `GetDisplayValue()`
call per completed step for the 35 differing Columbia meters and Eagle helium
meter. The polled coast matches all 36 stock Windows meter records exactly,
including `DCVoltMeter` and `MainHeliumPressureMeter`. All observed battery,
inverter, pending-heat and radiator values stay identical at every observation.
The calls deliberately update original display filters; no equations or control
inputs change. This isolates the saved meter discrepancy as display cadence and
excludes those 36 polls as the cause of the observed energy differences. Actual
graphics and other graphical callbacks were not run or equivalently exercised.

Twelve relevant functions are byte-identical to pinned NASSP after newline
normalization: battery load/save/update, tank load/save, heat-load generation
and refresh, `PanelSDK::SimpleTimestep`, thermal radiation and inverter
update/heat curve/system timestep. The original sequence consumes pending
thermal/electrical loads before devices accumulate the next loads. Native
capacities stay at their loaded values through step 6 and first decrease at
step 7. Inverter heat loss changes from 40 W at step 1 to 50.89343667056608 W at
step 2 and 53.78470776946478 W at step 3. Observing only the first startup step
cannot test the initial electrical propagation.

The next smallest useful Windows measurement is two fresh original-scenario
runs ending at total steps 3 and 11. Use the same established fixed-step stock
workflow, with `--maxsimtime=0.041666666666666664` and `--maxsimtime=0.175`
respectively. Do not obtain the second by reloading the first. Native AGC
counter cross-checks are 4266 and 15644 for both CSM and LM. The complete request,
configuration hashes and collection requirements are preserved at
`evidence/thermal-diagnosis/windows-minimum-capture-request.md` in the task
workspace. These saves are still needed to locate the first differing update;
no native thermal/battery defect has yet been isolated.

The machine-readable results and evidence hashes are in
[validation-v9-thermal-diagnosis-macos-2026-10-11.json](../upstream/validation-v9-thermal-diagnosis-macos-2026-10-11.json).
The optional-texture harness issue is separately fixed by `f8d4ff8`; the real
loader/coast/reload check passes with an empty texture argument and configured
textures. This bounded phase runs no new full suite, GPU, VM, push or hosted CI.
