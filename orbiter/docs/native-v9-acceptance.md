# Bounded native v9 acceptance — 2026-10-12

The operator entry point is [native-v9-operator-demo.md](native-v9-operator-demo.md).
“Verified” below applies only to the described bounded observation.

| Check | Status | Evidence / limit |
| --- | --- | --- |
| Locked OpenOrbiter / NASSP v9 and original data | Verified | Exact hashes in the operator guide; scenario bytes unchanged. Earlier 2016/v8 reports retain their historical pins. |
| Complete registered suite | 103 pass / 1 fail / 0 skip / 104 | Single serial run on `0bd60e8`; 93 binary hashes unchanged. [Full result](../upstream/validation-v9-final-current-macos-2026-10-12.json). |
| Native window, cockpit, DSKY/FDAI/X-pointers, F2 instruments | Verified live | Final PDI/liftoff inspection; freeze, Ctrl seat navigation, F1 view switches and wheel zoom. |
| Startup freeze, F5 resume/pause, normal close | Verified live | PDI 0.65 s; liftoff retry 10.8167 s; owned processes exited; 953 protected hashes match. Open return code is not presented as simulator exit status. |
| PDI L press/release | Delivery verified; authority unisolated | Requested 250 ms, with resumed displays/altitude change. No isolated yaw or neutral ACA telemetry claim. |
| Liftoff control visibility, MASTER ARM ON, ENG ARM ASC | Verified live | Ordinary Down-arrow camera movement exposes EDS; visual and saved arm readbacks. |
| Fresh HE PRESS/ASC actuation | Unverified | Timed left click completed; transient FIRE/hit on 3818 not proven. Released SAFE is normal. Original system was already primed. |
| Manual START/release, separation, plume, initial climb | Verified bounded | Requested 500 ms START; saved configuration 2/full ascent thrust, eleven vessels, radial speed 4.6661 m/s; actual MFD +4.67 m/s and ALT-R 0.01 km. [Save verification](../upstream/validation-v9-bounded-demo-macos-2026-10-12.json). |
| Exterior sunlight | Verified bounded | Equal state/assets/camera native before/after pair; qualitative Windows agreement. [Rendering evidence](native-v9-sun-occlusion.md). |
| Windows comparisons | Bounded / partial | Locked packages, state/input comparisons and visual reference. Numerical differences/limits remain documented; software D3D9 warnings and failed experiment modules retained. [Input comparison](native-v9-step-input-comparison.md), [coast comparison](native-v9-coast60-comparison.md). |
| Terrain / material / photometric parity | Open | PDI orbital terrain visible; landed native ground remains flat/dark gray versus black Windows ground. Site surface source falls back to level 9 (~333 m/texel). Material/normal-map/partial-solar-disc fidelity incomplete. |
| Faithful AGC save/reload | Failing; redesign paused | Cycle counter/CDU FIFO/scheduling state not serialized faithfully. Strict assertion is retained. |
| Original exact 60 s repeatability | Failing / unresolved | Separate from registered suite; wall-time RNG/body scheduling diagnosis is retained. No production RNG change or waiver. [Diagnosis](native-v9-repeatability.md). |
| Physical controllers / F4 live focus cycle | Open / tool-blocked | Synthetic input tests do not calibrate real hardware. Earlier F4 CUA timeout remains unresolved. |
| Full ascent, lunar orbit, rendezvous, full mission, manual landing | Unverified | No expansion beyond initial liftoff and short PDI inspection. Free-flight save status does not establish orbit. |

## Removable source inventory

This milestone adds no code feature, simulation equation, mission-state repair
or new test/build/run. The existing patches remain separate from original data.

| Ledger / group | Current inventory and upstream removal path |
| --- | --- |
| [NASSP v9 manifest](../upstream/nassp-v9/manifest.json) | 92 path entries, numbered patches 01–08. Reconstruct against exact `e731c28...`; retain matching original scenario/ROM/checklist data. |
| 01 build / 07 compiler portability | Adopt equivalent upstream source-selection, linkage, initialization and portability support; replace external native policy and remove corresponding hunks after gates pass. |
| 02 mission integration / 03 input / 04 audio / 05 graphics | Move reviewed hooks behind equivalent upstream platform/input/audio/graphics services; remove native vessel branches with equivalent behavior and coverage. |
| 06 guidance portability | Adopt shared SDK/VFS and fixed-width/resource/process support; retain ROM/state tests, without guidance tuning. |
| 08 state persistence | Five-file loader/throttle/cached CoG-PMI-fuel overlay. Adopt equivalent upstream fixes and pre-step lifecycle, then remove the overlay. This does not solve the paused AGC snapshot redesign. |
| [Engine/build/test ledger](../upstream/openorbiter-native.json) | 4002 unchanged files; 322 added/modified/omitted entries, including 8 pre-existing upstream omissions. Each group records adoption/removal gates; omissions require explicit restoration. |
| Recent host/graphics corrections | Startup pause uses the original initialization/pause lifecycle; manual drivers explicitly own their steps. Sunlight guard follows pinned Windows sunlit-side handling. Remove each with equivalent upstream native behavior and the corresponding regression coverage. |

Detailed build/source policy is in [native-v9-upgrade.md](native-v9-upgrade.md).
The milestone closes this bounded demo; it does not waive any open row above.
