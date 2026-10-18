# Stock Windows comparison after sixty seconds

The fresh stock Windows and production native captures align at 3,601 total
1/60 s steps: one startup step plus 3,600 neutral coast steps. Both save MJD
`40422.8324568111` and AGC counter `5121422` for Columbia and Eagle. The stock
raw save SHA-256 is
`6a5a703638f476c3aa6407263b46a72d9608a8e6c8cf584ff36720839d5e2115`.
Original source pins, scenario, force settings and no-input convention are
unchanged; production retains the correct gimbal loader and double clocks.

At the sixty-second boundary, both vessels' complete saved AGC and DSKY blocks
match after CRLF normalization. Position, velocity, attitude and propellant
tokens match exactly. Columbia's VROT tokens also match; Eagle's VROT is absent
in both saves. Orbiter omits VROT when angular-speed squared is at most 1e-8
rad²/s² (`Src/Orbiter/Vessel.cpp:6164`); this does not establish zero or equal
full-precision Eagle angular velocity. Position/velocity/attitude tokens have
finite save precision, so matching them is a boundary observation rather than
proof of full-precision trajectories or full mission correctness.

Residuals grow between the five-second and sixty-second boundaries of the same
production native run:

| Observed difference | Five-second coast | Sixty-second coast |
| --- | --- | --- |
| Differing Columbia chemical records | 3 | 50 |
| Maximum Columbia chemical mass difference | 0 g at saved precision | 3.49e-10 g |
| Maximum Columbia chemical vapor-mass difference | 0 g at saved precision | 3.1e-11 g |
| Maximum Columbia chemical energy difference | 2.33e-10 J | 1.48585e-7 J |
| Maximum Columbia PIPA fractional remainder difference | 6.3e-11 pulse | 1.23e-10 pulse |
| Maximum Eagle PIPA fractional remainder difference | 1.70e-10 pulse | 5.63e-10 pulse |
| Maximum Columbia saved LASTGLOBALVEL component difference | 8e-12 m/s | 6.38e-10 m/s |
| Maximum Eagle saved LASTGLOBALVEL component difference | 8e-12 m/s | 6.59e-10 m/s |
| Maximum Eagle ASA remaining-delta-velocity component difference | 2e-12 m/s | 6e-12 m/s |

The three differing sixty-second IMU fields for each vessel are RPX/RPY/RPZ.
Using unchanged upstream pulse scales, the maximum remainders correspond to
7.1955e-12 m/s for Columbia and 5.63e-12 m/s for Eagle. These conversions and
magnitudes are measurements, not acceptance bounds. The precise cross-platform
arithmetic cause remains unisolated. Equal current AGC integer records do not
rule out later divergence near a pulse or logic threshold.

The known motor-loader startup difference remains visible in production. At
sixty seconds native-minus-stock battery energy tokens are -0.3883 J for descent
C, -0.1941 J for descent D and -0.1848 J for ascent B. Their temperatures and ten
radiator records contain additional differences. Seventy-one Eagle chemical
records differ; the largest component differences are 0.009525218271 g of mass
and 11.905594720855 J of energy in PRIMREG water. These are individual tank
redistribution differences, not a measured whole-vessel energy imbalance.
No equation, input or initial-state change is made to conceal them.

A separate diagnostic reproduces stock's ignored initial motor flags. It
changes only the two original `MOTORRUNNING 1` values to zero and uses the same
private observer binary as a fresh original-state control. It is explicitly
modified mission state and is never accepted as the production scenario.
All its Eagle battery/radiator records and chemical mass/vapor tokens match
stock at sixty seconds, while **four chemical energy records still differ**:
PRIMGLYCOLACCUMULATOR -4.65e-10 J, PRIMGLYCOLPUMPMANIFOLD -6.98e-10 J,
PRIMGLYCOLLOOP1 +2.33e-10 J and DESBATCOOLING -1.16e-10 J. Its trajectory and
complete AGC/DSKY blocks still match stock. The five-second claim that the motor
counterfactual eliminates every selected Eagle difference therefore does not
extend to sixty seconds.

The added original-state observer control itself differs from production in
three Eagle chemical energy tokens, with maximum absolute difference 4.66e-10 J.
An analysis assertion expecting all selected Eagle records to be identical
failed. Its original script and exact mismatch records are retained; the
comparison now reports that failed assumption without replacing it with a
numerical tolerance. This limits the stronger causal claim to matching saved
battery/radiator and mass/vapor results; the remaining chemical decimal cause
and run/observer variation remain unresolved.

Native mission-clock offsets remain -0.013763098687 s in Columbia's saved token
and -0.013763 s in Eagle's saved token. Headless display filters remain visible
in complete raw diffs; their values do not establish equal rendered cockpit
readings. Existing extra native state records also remain explicit. No new
registered test, production source/library change, GPU or VM operation, broad
snapshot redesign, push or hosted CI occurred. Shared AGC resume failure,
physical controls and graphical/manual-landing acceptance remain unresolved.

The sparse evidence consists of boundary saves, 61-point traces and full raw
differences, rather than a per-frame save set. Complete Windows evidence,
production original-state results, original-state observer control and labeled
motor diagnostic are retained separately under `evidence/coast-60s` in the task
workspace. Exact hashes, numeric differences and validation scope are in
[validation-v9-coast60-comparison-macos-2026-10-11.json](../upstream/validation-v9-coast60-comparison-macos-2026-10-11.json).
