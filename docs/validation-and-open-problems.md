# Validation and open problems

The bounded native demo is complete at the stated scope. The project is not yet
a fully validated mission simulator or a cleared multi-OS binary distribution.
The [acceptance matrix](../orbiter/docs/native-v9-acceptance.md) is authoritative
for the 2026-10-12 manual closeout; these sections connect its limits to next work.

## Local publication candidate check, 2026-10-18

A fresh AppleClang/macOS arm64 build of the selected public candidate completed
all 819 build steps. Its capability-declared CPU subset selected 63 of 104
registered tests and passed 63/63 in 48.66 s after four OpenOrbiter star and
constellation catalog files were restored. The first run (62/63) failed only
`star_catalog` because the initial export rule had omitted those data files;
that failed result is retained in the private preparation evidence. No GPU,
window, physical controller or complete mission was run for this candidate.
This subset does not replace the strict 104-test result below.

Preview planning checks all 617 selected source/staging inputs by hash and
finds all 42 candidate binary paths, including `Extern/Lua/liblua.dylib`.
Readiness still exits 2 because artifact rights, corresponding source/notices,
installation and platform validation gates remain open. No binary preview was
created or uploaded.

## What passed

One unfiltered serial CTest invocation on clean source
`0bd60e8105910096ece760cd614517bb5d0dbd5c` produced 103 pass, 1 fail, 0 skip,
104 total in 1191.33 s. Every registered test has a result and all 93 inventoried
binary hashes match before/after. [The full manifest](../orbiter/upstream/validation-v9-final-current-macos-2026-10-12.json)
retains individual outcomes and the strict failure. Later milestone/documentation
commits do not change the tested production implementation.

Bounded original insertion coast, initial lunar liftoff, guided descent diagnostic,
MCC/instrument/resource/input tests and uninterrupted PDI/P64 graphical handoff
passed their recorded gates. `native_pdi_window` completed in 469.48 s in the
full run. Its virtual/synthetic SDL inputs establish those lifecycle checks,
not physical-controller calibration or a human landing.

Live QA verified the paused cockpit, DSKY/FDAI/X-pointers, F2 instruments,
seat/view/zoom controls, F5 resume/pause and normal close. The manual liftoff
retry reached 10.8167 total simulated seconds, retained all ten original vessels
and added the descent stage. The named save shows Eagle configuration 2, full
ascent thrust and radial speed 4.666119 m/s; vessel origins are 10.162618 m apart.
Actual images show plume/separation and MFD +4.67 m/s, ALT-R 0.01 km. All 953
protected hashes match and the owned app exited. These are initial-climb results;
the save label `Orbiting Moon` means free flight here, not achieved lunar orbit.

The matched native sunlight pair used the same state, asset hashes, camera and
FOV before/after the narrow visibility correction. The corrected capture helper
rebuilt camera position and measured distances 28.000001 and 10.703977 m. A first
helper attempt did not rebuild camera position; its failed setup is retained and
excluded from the matched claim. Corrected runs exited 0 with identical saved
scenarios, 1732 input hashes and terrain traces. Lit pale LM/gold foil agree
qualitatively with the stock Windows reference; this is not photometric parity.

## Strict failures and unverified behavior

| Issue | Actual evidence | Gate for closure |
| --- | --- | --- |
| Faithful AGC save/reload | `native_pdi_pilot` saves 124205511 cycles, reloads 0; pending CDU FIFO/scheduling state is incomplete | Explicitly approved snapshot design, complete processor/queue ownership, strict same-boundary resumed state and uninterrupted comparisons; broad redesign remains paused |
| Exact original 60 s repeatability | Separate serialized-repeatability check fails; wall-time mission RNG and randomized body scheduling produce the first observed saved difference at step 920 | Isolate effects under unmodified production semantics; retain original failure and clarify required deterministic contract before changing RNG policy |
| Full state/trajectory parity | Saved trajectory and complete AGC/DSKY records match at bounded sampled Windows/native boundaries; chemical/thermal/PIPA/clock/display residuals remain | Full-precision observer design and independently justified acceptance bounds, broader durations and mission transitions |
| Fresh HE PRESS/ASC proof | Timed left click returns; released SAFE is expected; transient FIRE and area 3818 delivery were not certified; source scenario already primed | Observe actual press/hold/release and system update on a reviewed suitable original scenario; do not infer from existing successful climb |
| Human PDI authority | 250 ms L request delivered during 0.65 s resumed inspection | Isolated yaw response, channel/neutral ACA readback and repeatable release observations |
| Physical controllers | Synthetic devices/events pass | Real hardware mapping, calibration/dead zones, focus loss, release, disconnect/reconnect and documented devices |
| F4 live focus | Earlier CUA timeout | Bounded operator focus cycle with process identity retained, failure recovery and no unintended reacquisition |
| Terrain/material fidelity | Orbital PDI terrain visible; landed Moon uses level 9 (~333 m/texel) and native gray versus Windows black ground | Asset/tile availability versus renderer behavior isolated; material/specular/normal maps/solar disc/horizon investigated separately |
| Windows reference fidelity | ARM64 Windows emulates official x64 binaries; liftoff uses software D3D9; three experiment-module failures and material/shutdown warnings retained | Hardware Windows validation, correct experiment modules and clean lifecycle; no blanket successful-scenario claim |
| Complete missions | Short coast, simplified descent and initial climb only | Full original checklist/flight, full ascent/orbit/rendezvous, manual landing and sustained performance |
| Other native OS/build distribution | macOS demo tested; stock Windows is a reference, not this fork's native Windows build | Fresh native Windows/Linux builds and staged original mission tests; relocatable install/package, notices, source and signing |

The simplified descent diagnostic's soft first contact and continued stopped
engine do not establish a pilot checklist or human landing: it uses an explicit
2000-foot radar bound and omits actions described in its original report.
Do not promote that result into broader acceptance.

## How Windows comparisons constrain claims

Neutral/pulse captures at steps 319 and 559 have identical saved RPOS/RVEL/AROT/
VROT/propellant tokens and normalized AGC/DSKY blocks on both platforms. The
requested numpad3 interval is 302–319, released before 320. At 319, pulse minus
neutral yaw is -0.6016 degrees/s; channel 031 and RCS consumption independently
confirm input response. Saved precision and unsampled behavior remain limits.

At 3601 total steps (startup plus 3600 coast), both save MJD
40422.8324568111 and AGC counter 5121422. Chemical energy, fractional guidance,
mission-clock and display-cadence residuals remain. Native correctly loads saved
running gimbal motors that stock skips, creating a known startup difference.
Private motor-disabled counterfactuals are diagnosis only and alter mission
state; they are never production acceptance inputs. Tiny remaining chemical
differences and an observer-control failed equality assumption are preserved.
See [the complete bounded coast report](../orbiter/docs/native-v9-coast60-comparison.md).

Raw reference collection must preserve official binaries/configs/quick saves,
log observer identity and restore temporary plugins/tasks/configs afterward.
[Windows reproduction](windows-reproduction.md) documents the actual environment
and protocols; GPU/software/emulation limitations travel with each result.

## Exit criteria

This milestone exits when locked source/data, original scenarios, native build,
complete honest suite results, bounded operator observations and removal ledgers
are preserved and reviewable. It has met that bounded exit. It does not require
concealing the resume/repeatability failures. Full mission acceptance, public
release and a new architecture are separate gates with their own evidence.
