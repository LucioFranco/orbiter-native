# Native macOS: locked OpenOrbiter and NASSP v9

The current bounded demo closes with verified manual START/release, separation
and initial climb. Use the [operator guide](native-v9-operator-demo.md) and
[acceptance matrix](native-v9-acceptance.md) for the final 2026-10-12 checkpoint.
The current complete suite is 103 pass / 1 strict failure / 0 skip / 104 total;
the earlier counts below are dated migration evidence. Portable commands and
project context are in the [project index](../../docs/README.md).

This isolated upgrade starts from the clean `macos-pdi-protocol` demo at
`9e8d452e4a41c9b73bbc516e77b13a49d79f7c77`. The original checkout remains at
`<PRESERVED_POC>`. Earlier baseline/candidate reports in
`upstream/` are historical results for their recorded pins, not v9 validation.

| Input | Locked source | Release reference |
| --- | --- | --- |
| OpenOrbiter | `orbitersim/orbiter@bd1d101f348da66e8c5b986633127c05e18d42c8` | [Development release, 2026-10-09](https://github.com/orbitersim/orbiter/releases/tag/latest) |
| NASSP v9 | `rcflyinghokie/NASSP@e731c28c0ebc1a4fe3b63f99e2006a1a8a2b42a9` | [NASSP-V9.0-Orbiter2024-434](https://github.com/rcflyinghokie/NASSP/releases/tag/NASSP-V9.0-Orbiter2024-434) |

The hashes select the source. Moving `latest` and release tags are provenance
labels only. The separate Folgers/J-mission experiment is not selected.

## Build and stage

The adapted v9 source is tracked under `Addons/NASSP`. Use a separate pristine
checkout of the locked NASSP revision for original data; source and data remain
independent. From this checkout's `orbiter` directory:

```sh
git init /absolute/path/to/nassp-v9-data
git -C /absolute/path/to/nassp-v9-data remote add origin https://github.com/rcflyinghokie/NASSP.git
git -C /absolute/path/to/nassp-v9-data fetch --depth 1 origin e731c28c0ebc1a4fe3b63f99e2006a1a8a2b42a9
git -C /absolute/path/to/nassp-v9-data checkout --detach e731c28c0ebc1a4fe3b63f99e2006a1a8a2b42a9

export NASSP_V9_DATA=/absolute/path/to/nassp-v9-data
export ORBITER_V9_TEXTURES=/absolute/path/to/orbiter-2024/Textures
cmake --preset native-v9 \
  -DNASSP_DATA_DIR="$NASSP_V9_DATA" \
  -DORBITER_TEXTURE_DIR="$ORBITER_V9_TEXTURES"
cmake --build --preset native-v9 --parallel 2
ctest --preset native-v9 --parallel 1 --output-on-failure

python3 tools/nassp_demo.py check-data --profile v9 \
  --source-addon Addons/NASSP --nassp-data-root "$NASSP_V9_DATA" \
  --verify-all-assets --orbiter-textures "$ORBITER_V9_TEXTURES"
python3 tools/nassp_demo.py stage --build build/native-v9 --profile v9 \
  --nassp-data-root "$NASSP_V9_DATA" --orbiter-textures "$ORBITER_V9_TEXTURES" \
  --mission insertion
python3 tools/nassp_demo.py stage --build build/native-v9 --profile v9 \
  --nassp-data-root "$NASSP_V9_DATA" --orbiter-textures "$ORBITER_V9_TEXTURES" \
  --mission liftoff
```

Run the staged helper's printed `launch_command` from the same `orbiter`
directory. For the bounded uninterrupted PDI/P64 diagnostic, use
`python3 tools/nassp_pdi_window.py --build build/native-v9`; saves do not promise
faithful processor resume. The inherited flight/input controls and scope remain
documented in [macos-pdi-protocol.md](macos-pdi-protocol.md).

Apple's compiler/SDK, pinned CMake/Ninja/Rust tooling and cached dependencies
follow [clean-macos-demos.md](clean-macos-demos.md). An offline build supplies
the six explicit `FETCHCONTENT_SOURCE_DIR_*` caches, enables
`FETCHCONTENT_FULLY_DISCONNECTED`, and selects a writable populated Cargo cache
using `ORBITER_CARGO_HOME`. Do not reuse a different checkout's CMake cache.

## Compatibility boundaries and removal

The OpenOrbiter base delta from `4137930` contains one source file: upstream
reorders `FetchContent_Declare` arguments in `Sound/XRSound/CMakeLists.txt`.
The complete file is byte-identical to the locked release. The inherited native
simulation equations are unchanged by this engine update.

The v9 native build reads authoritative upstream CMake source/include lists
through Orbiter's external build policy. It avoids executing NASSP's Windows
architecture (`x64` for every 64-bit host) and `lua.dll` installation detection.
Three modules still require upstream VC2017 XML source-list fallback. The 14
source manifests have upstream blob IDs and normalized hashes under
`upstream/nassp-manifests/v9`.

`upstream/nassp-v9/manifest.json` inventories the removable native NASSP patches.
Groups 01–07 remain separate from the five-file `08-state-persistence.patch`.
Reconstruction uses a pristine checkout of the exact v9 hash and all nonempty
numbered patches in order. Each group records its upstream adoption/removal
path. `upstream/openorbiter-native.json` inventories native engine, SDK, graphics,
build and regression-tool changes separately; regenerate it with
`tools/orbiter_upstream.py <pristine-locked-orbiter> --output
upstream/openorbiter-native.json`.

The engine inventory compares the union of both Git tracked trees. It includes
eight pre-existing upstream omissions: five scenery PDFs and three Windows Qt
dependency files (two DLLs and one PDB). Each has an upstream hash, a `deleted`
status and an explicit restoration path. An untracked local copy cannot hide a
committed omission. Run the focused coverage regression with
`python3 Tests/OrbiterUpstream.py`; it is separate from the 99 registered runtime
checks. The review follow-up records 4,003 unchanged files and 312 differences,
including these omissions and the new regression.

Original v9 data changes from the previous October candidate include Earth and
Moon gravity-model selection and six scenario files. ROM and checklist binaries
are byte-identical. The v9 Earth configuration selects degree cutoff 10 (65 coefficients loaded);
Moon selects cutoff 120 (7,380 coefficients loaded). Scenario updates include URL help metadata and supplied AGC memory
words. No supplied configuration, equation, mission state, ROM or checklist is
retuned to pass the native checks. These fixtures have no `Orbiter.cfg` override
and retain the inherited `NonsphericalGravitySources = FALSE` default. The logs
verify table loading; they do not validate the new harmonic gravity forces.

The engine and addon inventories describe inherited adaptations as well as this
migration. They do not certify that every inherited hook is ready for upstream
acceptance. Native behavioral parity, complete missions, Windows comparisons,
physical-controller calibration, flight-view correctness and human landing
remain distinct validation work.

## Evidence

Local build, test, staging, reconstruction and backup results are recorded in
`upstream/validation-v9-macos-2026-10-11.json`. Preserve raw failing logs along
with passing logs. The existing faithful AGC resume blocker (cycle timing and
pending FIFO state) remains a strict gate; broad snapshot redesign is paused.
No hosted CI, push or repository visibility changes are part of this upgrade.

The native build passed at implementation commit `1f737d8`. Final-tree build
validation at `25d4abe` passed with all 41 recorded runtime binary hashes
unchanged. With desktop access, 89 CTest checks passed before the parent's VM
setup requested window/GPU release. Its active descent child completed every
verifier check, but the paused coordinator was terminated before collecting
that CTest result. Those partial logs and the first 6/7 headless group remain
preserved. After the VM released the window, the descent CTest gate was rerun
and passed in 70.15 seconds. The completed graphical group passed 4/4 checks
in 570.84 seconds, including its repeated staging fixture.

All 99 registered CTest checks now have recorded results: **98 pass, one strict
resume gate fails**. This coverage combines the initial desktop phase and final
graphical/headless groups, counting repeated fixtures once. The final tree's
bounded rerun passed 11/12 CTest checks: 4/4 graphical and 7/8 headless in 234.61
seconds. The two standalone inventory regressions passed separately. No single
uninterrupted 99-test invocation was completed. Raw logs, JUnit results, telemetry,
checkpoint scenarios and four cockpit captures are retained under
`evidence/graphics-final` in the task workspace.

Original insertion passed its 60-second coast, fresh two-second continuation,
and three bounded graphical launch/save/reopen processes. Original lunar liftoff
passed cockpit-commanded ascent, descent-stage creation, fresh continuation and
the graphical launch checks. MCC display and resume gates passed. The simplified
descent diagnostic obtained soft first contact at 1605.83 seconds, vertical speed
-0.894745 m/s and groundspeed 0.932137 m/s, followed by engine stop and ten seconds
of fresh contact continuation. Its explicit 2000-foot radar bound and omitted
pilot actions remain diagnostic limits, not complete checklist or manual landing
validation. Bounded first-N68 and uninterrupted P64 ACA input/release checks passed.

`native_pdi_pilot` remains red: the saved LGC cycle counter 124,205,511 reloads as
zero. The current loader intentionally skips it while CDU FIFO state is not
serialized. The original strict verifier, failing report, telemetry and saves are
retained. No rule is waived and no snapshot redesign is added.

`native_descent_visual` passed in 18.02 seconds. It verified the landed Eagle,
P65 DSKY, both X-pointers, stopped DPS and an actual LGC display command changing
the cockpit texture. `native_pdi_window` passed in 481.98 seconds with two
independent launches. Both reached the same live P64 handoff at mission time
370206.157069993, cycle counter 124,734,577 and altitude 1,611.71 feet. All twelve
synthetic SDL lifecycle checks passed, covering input isolation, pause/unpause,
keyboard ACA input/release, focus loss, continuation guards and both close paths.
The virtual stick was removed without advancing or resetting the processor at
handoff. Original-scenario and P64 cockpit captures are retained for both runs.

To reproduce that graphical group with hardware desktop access:

```sh
ctest --test-dir build/native-v9 --parallel 1 --output-on-failure \
  -R '^(native_descent_visual|native_pdi_window)$'
```

CTest restages and reruns required fixtures for those gates. The v9 graphical
PDI handoff is now validated for synthetic events; human controls, physical
controllers, exterior flight-view correctness and manual landing remain
unverified. The earlier `9e8d452` handoff evidence applies to its recorded old
source/data profiles. No native GPU test or window remains active.

A normal sandbox run could not see a hardware wgpu adapter and was interrupted
by a user app restart. Its partial logs are retained. The same GPU binary passed
with approved desktop access; the subsequent desktop run removed those sandbox
failures without source or verifier changes. An independent clean clone passed
all-asset/source preflight and configured successfully with all 14 tracked source
manifests. Verified Git bundles preserve both `9e8d452` and the implementation;
raw evidence and hashes are retained under the task workspace's `evidence` and
`backups` directories.
