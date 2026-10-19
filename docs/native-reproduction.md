# Reproduce the locked native macOS baseline

Commands run from this repository's `orbiter/` directory. Choose writable paths
for data, dependencies, build outputs and evidence. No command requires the
original operator's home directory or the isolated QA app. The tested host was
Apple Silicon macOS; other hosts need their own recorded results.

## Prerequisites and original inputs

Install Apple's Command Line Tools, then use the pinned Nix development shell
for CMake, Ninja, Python, Cargo and Rust. The checked-in
[`nix/flake.lock`](../orbiter/nix/flake.lock) pins Nix inputs; Apple Clang/SDK are
selected with `xcrun`. Record `sw_vers`, `uname -m`, `xcrun clang --version`,
`xcrun --show-sdk-path`, `cmake --version`, `ninja --version`, `rustc --version`
and `cargo --version` alongside each new result. Tool availability is not a
claim that a first uncached build or another OS has passed.

```sh
cd orbiter
nix develop path:./nix

export NASSP_V9_DATA=/absolute/path/to/pristine-nassp-v9
export ORBITER_V9_TEXTURES=/absolute/path/to/orbiter-2024/Textures
git init "$NASSP_V9_DATA"
git -C "$NASSP_V9_DATA" remote add origin https://github.com/rcflyinghokie/NASSP.git
git -C "$NASSP_V9_DATA" fetch --depth 1 origin e731c28c0ebc1a4fe3b63f99e2006a1a8a2b42a9
git -C "$NASSP_V9_DATA" checkout --detach e731c28c0ebc1a4fe3b63f99e2006a1a8a2b42a9
git -C "$NASSP_V9_DATA" status --porcelain
```

Keep the data checkout pristine. The repository already contains the adapted
source in `Addons/NASSP`; do not apply native patches to the original data copy.
Acquire planetary data separately under its actual distribution terms. The
stager requires nonempty Earth and Moon `Archive/Surf.tree`/`Elev.tree` files.
It checks presence and sizes, not an arbitrary package's authenticity. The
observed same-archive native/Windows hashes are:

| Archive relative to Textures | Bytes | SHA256 |
| --- | ---: | --- |
| `Moon/Archive/Surf.tree` | 256358063 | `0ed3d152d5bddbeda3a9b019c7fc8bf98400e934085d4b829a87429f1e44a798` |
| `Moon/Archive/Elev.tree` | 272443927 | `dd57a09d47b60964cdb4fbc5917684cdf9926fe7a8229e6b9e89a12873059744` |

Run the all-asset/source preflight before configure:

```sh
python3 tools/nassp_demo.py check-data --profile v9 \
  --nassp-data-root "$NASSP_V9_DATA" --source-addon Addons/NASSP \
  --verify-all-assets --orbiter-textures "$ORBITER_V9_TEXTURES"
```

Critical source/data checks allow the recorded LF/CRLF normalization for text;
binary assets use original bytes/Git blob identity. All 14 upstream source-list
exports and 93 adapted paths are verified. Keep the printed JSON as evidence.

## Configure and build

```sh
cmake --preset native-v9 \
  -DNASSP_DATA_DIR="$NASSP_V9_DATA" \
  -DORBITER_TEXTURE_DIR="$ORBITER_V9_TEXTURES"
cmake --build --preset native-v9 --parallel 2
```

Native dependency FetchContent commits and the renderer Cargo lockfile are
tracked. An uncached build requires network access to those sources/crates.
For an offline build, supply verified existing sources for
`FETCHCONTENT_SOURCE_DIR_SDL3`, `IMGUI`, `CORROSION`, `LUA`, `MINIAUDIO`, `STB`
(each full CMake option begins `FETCHCONTENT_SOURCE_DIR_`), set
`FETCHCONTENT_FULLY_DISCONNECTED=ON`, and select a writable populated Cargo
cache with `-DORBITER_CARGO_HOME=/absolute/path/to/cargo-cache` and
`CARGO_NET_OFFLINE=true`. Disconnected mode does not fetch missing dependencies.
Do not reuse another checkout's CMake cache or edit shared dependency sources.

## Complete suite and focused regressions

Use desktop/GPU access for graphics tests and keep one native GPU session at a
time. Coordinate with any VM/interactive QA using the same host. Save the current
Git commit/status and runtime binary SHA256 inventory before and after.

```sh
export REPRO_EVIDENCE=/absolute/fresh/evidence-directory
mkdir -p "$REPRO_EVIDENCE"
git rev-parse HEAD > "$REPRO_EVIDENCE/source-commit.txt"
git status --porcelain > "$REPRO_EVIDENCE/source-status-before.txt"
ctest --test-dir build/native-v9 -N > "$REPRO_EVIDENCE/registered-tests.txt"
ctest --test-dir build/native-v9 --parallel 1 --output-on-failure \
  --output-junit "$REPRO_EVIDENCE/suite-junit.xml" \
  > "$REPRO_EVIDENCE/suite.log" 2>&1
```

Preserve the exit status even on failure; shells using `set -e` must arrange
collection before exiting. The recorded run on source `0bd60e8` completed all
104 tests, 103 pass/1 fail/0 skip, CTest exit 8, 1191.33 seconds. The failure is
`native_pdi_pilot`, not a reason to disable it or add `WILL_FAIL`. All 93 recorded
runtime binary hashes were unchanged. Newly generated runtimes, saves and logs
remain evidence, separate from pristine inputs. A future run must report its own
actual count and result, rather than copy this expected checkpoint.

Focused checks reuse CTest fixtures:

```sh
ctest --test-dir build/native-v9 --parallel 1 --output-on-failure \
  -R '^(native_startup_pause|native_manual_step_alignment|sun_occlusion)$'
ctest --test-dir build/native-v9 --parallel 1 --output-on-failure \
  -R '^(native_descent_visual|native_pdi_window)$'
python3 Tests/OrbiterUpstream.py
```

The original exact sixty-second repeatability investigation is separate from
the registered suite. Its commands and failed evidence are in
[`repeatability-diagnostic/README.md`](../orbiter/upstream/repeatability-diagnostic/README.md).
Do not seed or modify production code to force the result.

## Fresh original paused CLI demo

The local QA bundle/watchdog was host-specific and is not distributed. The
tracked scenario stager plus CLI reproduces the scenario and startup policy:

```sh
export DEMO_RUNTIME=/absolute/fresh/paused-liftoff-runtime
python3 tools/nassp_demo.py check --build build/native-v9 --profile v9 \
  --nassp-data-root "$NASSP_V9_DATA" --source-addon Addons/NASSP \
  --orbiter-textures "$ORBITER_V9_TEXTURES" --mission liftoff
python3 tools/stage_nassp_mission.py build/native-v9 --profile v9 \
  --addon "$NASSP_V9_DATA" --textures "$ORBITER_V9_TEXTURES" \
  --scenario 'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 11 - Lunar Liftoff T-1m.scn' \
  --output "$DEMO_RUNTIME"
cat > "$DEMO_RUNTIME/Orbiter.cfg" <<'CONFIG'
StartPaused = TRUE
FixedStep = 0.016666666666666666
CONFIG
build/native-v9/orbiter --root "$DEMO_RUNTIME" \
  --scenario 'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 11 - Lunar Liftoff T-1m' \
  --graphics WgpuClient --module ProjectApolloMFD --window --vc \
  --laptop-controls --save PausedQALiftoff
```

Select a genuinely fresh output directory; the stager may refresh its owned
bindings on reuse, so fresh evidence is the caller's responsibility. The config
contains only the recorded pause/fixed-step policy; other settings use inherited
defaults. Original scenario bytes remain unchanged. Confirm PAUSED Eagle and a
frozen clock at the one-step initialization boundary. Close normally and record
the simulator process exit, console, Orbiter log and named save. The local app's
30-minute watchdog/unique registration are not properties of this direct CLI
command. The operator must enforce a 30-second resumed liftoff cap.

For PDI use the original `Apollo 11 - 10 - PDI T-15m.scn`, a separate fresh
runtime, scenario name without `.scn` and save `PausedQAPdi`; use a 10-second
resumed cap. Actual action/readback sequence and unverified HE actuation are in
[the operator guide](../orbiter/docs/native-v9-operator-demo.md). Saves are
observations, not faithful AGC restart checkpoints. Do not use a final PDI save
as the starting original scenario.

For insertion, `nassp_demo.py stage ... --mission insertion` stages the unchanged
original scenario and prints the command. For step-indexed comparisons, use
[`stage_nassp_manual.py`](../orbiter/tools/stage_nassp_manual.py), which requires
fresh output and StartPaused FALSE. Do not combine the two timing policies.

## Reconstruct and preserve

To audit the addon independently, check out pristine NASSP at the locked hash,
apply all nonempty `upstream/nassp-v9/0*.patch` files in numeric order and verify
the manifest's normalized patched hashes. Retain the original group 08 boundary
when regenerating. The engine ledger requires a pristine, clean OpenOrbiter
checkout at its locked hash; its exporter compares the union of tracked paths.
See [architecture/removal policy](architecture.md).

Before sharing a result, retain commit, build settings/tool versions, input
hashes, exact command/exit status, every strict failure, raw saves and relevant
capture dimensions/encoding. Keep raw large evidence in an external archive
with a verified hash; commit compact manifests and reusable scripts. Use inert
archives for `.app` backups so LaunchServices cannot discover duplicate apps.
