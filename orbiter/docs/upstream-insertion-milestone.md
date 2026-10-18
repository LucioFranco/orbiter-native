# Upstream provenance and original mission milestones

Active source remains on main at b5f31350e25b49c5b531fdb3947ceb49949040ce.
Implementation changes are uncommitted. The separately published clean snapshot
is 8cdb481d37b689d89f322e109a645de88aad0983; it predates this implementation.
No active branch, shared Git index, history or remote is changed by these checks.

## Verified provenance and patch inventory

Orbiter: orbitersim/orbiter at 4137930c5126cf848212139cd138a9ef2ade7660.
NASSP: CaptainSwag101/NASSP CMake at 96b6380943c382a297f969842be9344ef969a6f4.
Refs were fetched into isolated reference trees and content compared. See
../upstream/baselines.json for tree hashes and limitations.

The seven NASSP patch groups contain 100 changed/added paths and 1328 unchanged
files in the Orbitersdk/Config/Scenarios scope. The latest inventory includes the
LVDC flight-sequence path/line-ending correction. Every patched blob has been
reconstructed in an isolated Git index against the baseline and its normalized
SHA256 checked against manifest.json. Each group records reasons, test gates,
and removal triggers/paths. This proves reproducibility, not the necessity or
Windows behavioral parity of every inherited vessel adaptation.

Regenerate from a pristine pinned NASSP checkout:

    python3 tools/nassp_upstream.py <reference-checkout> --output upstream/nassp

Generated modules, downloads and large release assets stay outside this patch
scope and publication. The candidate moves native build policy to Orbiter so the addon can follow
upstream source manifests without restoring removed NASSP CMake changes.

## Original insertion

Source: Addons/NASSP/Scenarios/Project Apollo - NASSP/Apollo - Mission Scenarios/
Apollo 11/Apollo 11 - 02 - Insertion.scn.

The staging tool copies original scenario bytes, all five vessels, and original
Sol_VirtualAGC planetary configuration. MCC, SII, configured planets/moons and
ephemeris dependencies build as native modules. Engine SDK coordinate transforms,
annotations and VC MFD registration metadata were restored where missing.
MCC cockpit display rendering itself is not fully verified.

The 60-second gate passed together with coordinate and rendered annotation
regressions: 5/5 in 12.39 seconds (original-insertion-sdk.log). A later sustained
run exposed an empty LVDC switch-selector table: direct std::ifstream could not
resolve the scenario's Windows-style flight-sequence path, and native getline
retained CRLF carriage returns. Both LVDC sequence readers now use the existing
native VFS and Windows-equivalent line handling, with failed loads explicit.
No sequence-file bytes, guidance ROMs, equations or scenario states were tuned.

The updated check passes a 900-second coast, AGC save/reload and two-second
fresh-process continuation (original-insertion-sustained.json/.log). All five
original vessels load. During the longer coast, the original Saturn KillAlt
callbacks delete discarded S-II/interstage below 1000 m; verification permits
only those removals and retains Columbia, MCC and Floodlight through reload.
The default 60-second gate still requires all five vessels. Orbital band, finite
state and clock assertions are sanity checks, not a Windows trajectory match.

## Original lunar liftoff

Source: unchanged Apollo 11 - 11 - Lunar Liftoff T-1m.scn. The new staging/test
pair passed 2/2 in 24.78 seconds (original-liftoff.log). All ten starting vessels
load. Real cockpit master-arm, helium-pressure and ascent-engine controls trigger
separation/climb without the isolated fixture's artificial drop. The resulting
11 vessels, including Eagle-DESCENTSTG, survive save/reload; a fresh process
continues climbing. Evidence: runtime-original-liftoff/liftoff-verification.json.

Visual QA is limited: the saved cockpit capture has readable timers, FDAI and
engine-pressure labels, but a solid white X-pointer center remains unexplained.
Astra's interactive app-discovery/tool session stalled and was cancelled; no UI
input occurred, and no QA process remains. This is static visual evidence only.

## Real upstream update rehearsal

Candidate: orbiternassp/NASSP Orbiter2016 at
5dffae9a73366604a66308617b7b9f6e7dbf6c60 (2026-10-06). Applying the refreshed
seven-group stack with git apply --cached --3way in an isolated index identifies
three source conflicts: src_csm/saturn.cpp and the ProjectApolloMFD/ChecklistMFD
headers. Input, audio, guidance-portability and compiler-portability groups apply
cleanly. The candidate removes the CaptainSwag CMake manifests used by this
baseline. Evidence: build/native-poc/nassp-update-rehearsal.json.

The isolated candidate now builds and passes 4/4 original mission gates in
36.42 seconds, plus a 900-second coast/AGC save/reload/two-second continuation.
The three source conflicts are resolved: upstream Autosave and menu changes are
retained, MFD declarations/definitions preserve pointer-sized SDK returns, and
Autosave creates native directories through the existing VFS. Its cadence and
mission behavior are unchanged. The existing size_t seed cast is retained.

The engine-owned adapter reads original VC2017 source manifests. C++ compatibility
preincludes apply only to C++; native input/audio/FDAI implementations replace
only sources actually selected by a module. Nine NASSP CMake patch paths are
removed in the candidate; 92 source adaptation paths remain (one added Autosave
portability path). The candidate-specific seven-group stack in
../upstream/nassp-candidate reconstructs all 92 paths against its exact commit.
The pinned baseline keeps its prior build path and 100-path stack.

The candidate scenarios have upstream changes, including AGC preset and suit-flow
fields. Each gate checks the candidate's own unmodified scenario bytes; it does
not transplant May state into October source. Meshes/textures/sounds and Missions
are unchanged between the verified upstream revisions. Changed checklists were
fetched and the real asset loader verifies the candidate Apollo 11 workbook/ROM.
The first complete candidate suite exposed a fixture revision mismatch: old
LEM system configuration lacked new helmet pipes. Smoke/terrain fixtures now
select matching candidate scenarios/configuration/checklists and record hashes.
All representative failures pass after that correction, removal of owned
controller files left by the crashes, and menu-action selection by labels.
The final complete baseline suite passes 82/82 in 154.09 seconds and the
candidate passes 82/82 in 136.65 seconds. The 900-second candidate coast,
save/reload and two-second fresh-process continuation also pass with its updated
checklists. The active pinned addon is preserved;
no branch switch, candidate promotion or publication has occurred.

Evidence: build/native-poc/upstream-candidate-resolution.json,
upstream-candidate-missions.log, upstream-candidate-sustained.json/.log and
upstream-candidate-checklists.log. Runtime/build: /tmp/nassp-upgrade-build;
source: /tmp/nassp-upgrade-candidate. See upstream-native-adoption.md for
reproduction and exact validation limits.

## Validation limits and next gate

The current full-tree build succeeds and the complete suite passes 82/82 in
154.09 seconds after the fixture/menu corrections (adoption-final-baseline-suite.log).
The candidate suite passes 82/82 in 136.65 seconds (adoption-final-candidate-suite.log).
Earlier baseline suites passed 82/82 in 153.54 and 168.70 seconds. These results are
distinct from historical 78/78. No working Windows/Wine reference runtime is available.
Full mission parity, guided landing, complete legacy 2D panels, sustained GPU
performance/terrain seams and physical-controller coverage remain unverified.
Guided descent still has the previously recorded radar-incorporation/impact
blocker; do not retune guidance to hide it.

The compact run guide is [original-mission-demo.md](original-mission-demo.md).
Portable test results and local evidence digests are recorded in
[validation-2026-10-07.json](../upstream/validation-2026-10-07.json).
