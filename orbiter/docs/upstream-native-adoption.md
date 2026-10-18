# Isolated native NASSP upstream adoption

This document records the initial adoption experiment and its historical local
paths. The subsequent clean-checkout setup, independent external data selection
and current commands are in [clean-macos-demos.md](clean-macos-demos.md). The
original-mission milestone is committed as `cd00b2f` on
`native-original-missions`; later work continues on `macos-clean-demos`.

The active pinned addon remains unchanged by this adoption exercise:
CaptainSwag101/NASSP CMake 96b6380943c382a297f969842be9344ef969a6f4. The isolated
candidate is orbiternassp/NASSP Orbiter2016
5dffae9a73366604a66308617b7b9f6e7dbf6c60, dated 2026-10-06. Orbiter provenance is
4137930c5126cf848212139cd138a9ef2ade7660 plus the existing native engine changes.
At the initial experiment, the active checkout main remained at b5f31350e25b49c5b531fdb3947ceb49949040ce with
uncommitted implementation work. The candidate was not promoted to the active addon.

## Adaptation and removals

The newer upstream tree removes the CaptainSwag CMake manifests. Instead of
restoring those files, Orbiter cmake/NativeNassp.cmake reads the original VC2017
source/include lists through NativeNasspManifest.cmake. The existing pinned
CMake build path stays available. NASSP_ADDON_DIR selects a separate addon;
`NASSP_DATA_DIR` now selects matching external data separately for staging and gates.

Native C++ preincludes apply only to C++ sources. SDL input, audio and FDAI
implementations replace the matching source entries only, avoiding unused
scheduler dependencies in modules with no sound implementation. MCC and mission
support modules continue through the shared manifest reader.

The old stack has 100 adapted paths. The candidate has 92: all nine build-overlay
paths retire, and upstream Autosave adds one VFS directory portability path.
Group counts: mission integration 4, input 6, audio 5, graphics 15, guidance
portability 8 and compiler/path portability 54. These counts describe remaining
adaptations, not proof that every inherited behavioral hook is necessary.

Actual three-way conflicts occurred in saturn.cpp, ProjectApolloMFD.h and
ProjectApolloChecklistMFD.h. Their narrow resolutions keep upstream Autosave/menu
changes, native time_t formatting and pointer-sized MFD return values. The seed
cast remains size_t. Autosave uses native VFS directory resolution while its
Windows branch, cadence and mission logic remain intact. Automatic autosave
notifications/timing have not been exercised; default mission gates test explicit
save/reload. No ROM, equation or scenario-state tuning was introduced.

## Initial experiment reproduction

These `/tmp` paths describe the earlier experiment. Use the clean setup above
for new builds; add explicit `NASSP_DATA_DIR`, `NASSP_DATA_PROFILE` and optional
`ORBITER_TEXTURE_DIR` to the configuration shown below.

Prepare an isolated checkout of the exact candidate commit, with original
Orbitersdk, Config, Scenarios, Missions and Doc checklist data available. Apply
the seven patches under upstream/nassp-candidate; skip zero-byte patch files.
The stack has reconstructed all 92 patched paths in an isolated Git index with
normalized SHA256 values checked against manifest.json.
upstream/.gitattributes preserves patch bytes, including upstream CRLF context.

Configure the engine with its normal cached/pinned native dependencies and:

    cmake -S orbiter -B /tmp/nassp-upgrade-build \
      -DORBITER_PORTABILITY_CHECKS=ON \
      -DNASSP_ADDON_DIR=/tmp/nassp-upgrade-candidate \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build /tmp/nassp-upgrade-build -j4
    ctest --test-dir /tmp/nassp-upgrade-build --output-on-failure

The local verified candidate used the same Clang/Rust toolchains and dependency
sources recorded in build/native-poc/CMakeCache.txt, with no installs. Release
meshes/textures/sounds may reuse the baseline package: Git tree comparison proves
these paths unchanged between the two upstream commits. Original missions use
candidate Config, Scenarios, Missions, bitmaps and updated checklists. Legacy smoke/terrain fixtures now select the same addon checkout for scenarios,
system configurations and checklists, with matching test headers. Each smoke
runtime records source/config hashes in smoke-source.json.

Regenerate the candidate stack from a pristine checkout of this exact revision:

    python3 orbiter/tools/nassp_upstream.py <pristine-candidate> \
      --addon /tmp/nassp-upgrade-candidate \
      --commit 5dffae9a73366604a66308617b7b9f6e7dbf6c60 \
      --repository https://github.com/orbiternassp/NASSP.git \
      --ref Orbiter2016 --output orbiter/upstream/nassp-candidate

## Verified results and limits

Final pinned baseline: complete 82/82 suite passes in 154.09 seconds. The
candidate builds and its complete suite passes 82/82 in 136.65 seconds, including
original insertion/liftoff and fresh-process resume. Sustained candidate insertion
also passes 900 seconds, save/reload and a fresh process continuing two seconds
with the candidate's updated checklist data. Apollo 11 checklist/ROM loading
passes. The initial candidate run exposed May fixture system configuration paired
with October crew-status code: required new helmet pipes were absent. Fixtures
now select matching candidate data. Upstream menu positions also changed; the
MFD regression resolves controls by labels and retains its state-change checks.
These corrections are in engine fixtures/tests; NASSP behavior is preserved.

Evidence: build/native-poc/adoption-final-{baseline,candidate}-suite.log,
upstream-candidate-sustained.json/.log and upstream-candidate-resolution.json.
Portable results and evidence digests: ../upstream/validation-2026-10-07.json.

Original scenario bytes are retained per checkout. Candidate scenario presets
changed upstream, so this is adoption of the original October data rather than
unchanged May state. Both initial insertion loads include all five vessels; only
the original discarded-stage cleanup removes S-II/interstage during sustained
coast. Liftoff retains all ten vessels, creates the descent stage through real
cockpit controls, saves all eleven and continues ascent after reload.

No Windows reference run, full mission parity or guided landing is proven.
Visual QA is static only: cockpit instruments are readable in the saved capture,
with an unexplained white X-pointer face. No successful live CUA interaction was
captured. Guided descent's recorded radar/incorporation/impact blocker, full
legacy 2D panels, sustained GPU performance and physical-controller validation
remain separate work. Both checkouts/builds remain available locally; the
candidate has not replaced the active addon.

## Persistence overlay, 2026-10-10

The historical seven-group counts above predate the handoff persistence work.
Current baseline and candidate inventories contain eight groups and 102/94
patched paths. `08-state-persistence.patch` corrects three original loaders,
preserves the existing DPS command and restores cached LM mass/CoG state. It
changes five NASSP files and records its removal path in each manifest.
Independent Windows execution remains deferred. The original seven patch files
are unchanged; reconstruction and simulated upstream-first adoption both verify
all inventoried hashes.

To reconstruct an active addon, apply all nonempty numbered patches in order
from the selected stack to an isolated checkout of its exact pinned revision.
To regenerate, first reconstruct **only groups 01..07** as a separate previous
adapter tree. Keep that tree separate from the active addon containing the
persistence fixes. The original `nassp_upstream.py` exporter operates on that
seven-group tree; the second stage appends the removable overlay:

```sh
python3 tools/nassp_state_persistence.py \
  --reference <pristine-pinned-checkout> \
  --previous-tree <seven-group-adapter-tree> \
  --addon <active-fixed-addon> --stack upstream/nassp
```

For the candidate, select its exact pristine revision and `upstream/nassp-candidate`.
Running the original exporter directly against the fully fixed addon would fold
these corrections into the seven native groups and lose this removal boundary.
Adopt equivalent upstream loader/throttle/mass-state fixes before retiring the
overlay, regenerate against the new pin and rerun exact persistence plus bounded
continuation. The full processor-resume gate remains red because AGC cycle/FIFO
state is not restored; this patch does not claim to implement that broader work.
