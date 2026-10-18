# Build and stage original missions from a clean macOS checkout

This document preserves the earlier baseline/candidate workflow. The current
upgrade checkout uses [locked OpenOrbiter and NASSP v9](native-v9-upgrade.md).

Run these commands from the `orbiter` directory of a fresh checkout. The native
engine and adapted NASSP source are tracked. Original NASSP scenarios/textures
are excluded by the inherited addon ignore rules; the build therefore requires
an explicit, complete external data root. It never discovers data in another
workspace or downloads a planetary package automatically.

## Pinned inputs

| Input | Repository / revision | Purpose |
| --- | --- | --- |
| Orbiter base | orbitersim/orbiter `4137930c5126cf848212139cd138a9ef2ade7660` | Engine provenance before native adaptations |
| NASSP baseline | CaptainSwag101/NASSP, CMake `96b6380943c382a297f969842be9344ef969a6f4` | Default adapted source and original mission data |
| NASSP candidate | orbiternassp/NASSP, Orbiter2016 `5dffae9a73366604a66308617b7b9f6e7dbf6c60` | Isolated October source/data experiment |

`NASSP_ADDON_DIR` selects **adapted source**; its default is `Addons/NASSP` in
this checkout. `NASSP_DATA_DIR` independently selects original `Config`,
`Scenarios`, `Missions`, `Doc`, `Meshes`, `Textures` and `Sound` directories from
the matching upstream revision. A normal clone of the pinned upstream NASSP
repository provides that data. Keep it unmodified; do not apply the native patch
stack to the data clone. Preserve upstream licenses with the external package.
For example, prepare the baseline data in a directory of your choice:

```sh
git clone --branch CMake https://github.com/CaptainSwag101/NASSP.git /absolute/path/to/nassp-data
git -C /absolute/path/to/nassp-data checkout --detach 96b6380943c382a297f969842be9344ef969a6f4
```

`upstream/nassp-data-{baseline,candidate}.json` records upstream Git blob IDs and
SHA256 hashes for 282 critical files, plus 1,054 mesh/texture/sound blob IDs. The
critical set includes system configuration, ROMs, missions, the six fixtures'
Apollo 11 scenarios, checklists and FDAI textures. Preflight checks all critical
hashes and asset presence. `--verify-all-assets` also checks every asset Git blob.
CRLF/LF conversion is accepted for recorded text only. Mixed revisions, missing
files and corrupted critical inputs fail before runtime creation.

`ORBITER_TEXTURE_DIR` separately selects an extracted Orbiter 2024 `Textures`
directory. It must contain nonempty Earth/Moon `Archive/Surf.tree` and
`Archive/Elev.tree` files. The package link in NASSP's bundled Quick Start Guide
is `https://cdn.openorbiter.space/orbiter-releases/Orbiter-2024.zip`. This check
records archive sizes, **not** release-package checksum verification. NASSP data
hashes do not authenticate the planetary package. Omit the option deliberately
for a runtime without these external planetary archives.

## Baseline build

Install Apple's Command Line Tools before building. The checked-in
`nix/flake.lock` pins the Nix tool environment; enter its shell for CMake, Ninja,
Python, Cargo and Rust. macOS compilation uses Apple's Clang and SDK. Native
FetchContent dependencies are pinned to Git commits and the renderer has a
tracked Cargo lockfile. An uncached first build needs those sources/crates;
NASSP data and planetary assets remain separate explicit inputs.

```sh
export NASSP_DATA_ROOT=/absolute/path/to/nassp-data
export ORBITER_TEXTURE_ROOT=/absolute/path/to/orbiter-2024/Textures
nix develop path:./nix
cmake --preset native-poc \
  -DNASSP_DATA_DIR="$NASSP_DATA_ROOT" -DNASSP_DATA_PROFILE=baseline \
  -DORBITER_TEXTURE_DIR="$ORBITER_TEXTURE_ROOT"
cmake --build --preset native-poc -j4
ctest --test-dir build/native-poc --output-on-failure
```

For an offline build, explicitly supply verified dependency source caches using
`FETCHCONTENT_SOURCE_DIR_{SDL3,IMGUI,CORROSION,LUA,MINIAUDIO,STB}`, set
`FETCHCONTENT_FULLY_DISCONNECTED=ON`, and select a populated Cargo cache with
`ORBITER_CARGO_HOME`. Toolchain/cache paths belong to the local CMake cache;
launch commands do not require the caches or another source workspace.

Fresh checkout source-list discovery also uses 14 small upstream VC2017 XML
manifests per revision under `upstream/nassp-manifests/`. The addon `Build`
directory is ignored. Exported XML differs only by line endings; each file's
upstream path, blob ID and normalized SHA256 are recorded and verified. Relative
source paths resolve from the original virtual `Build/VC2017` directory. Remove
this fallback if upstream provides tracked usable source manifests for every
required module. Build policy and runtime dependencies remain in engine CMake.

## Stage and launch

```sh
python3 tools/nassp_demo.py check-data --nassp-data-root "$NASSP_DATA_ROOT" \
  --source-addon Addons/NASSP --profile baseline --verify-all-assets \
  --orbiter-textures "$ORBITER_TEXTURE_ROOT"
python3 tools/nassp_demo.py stage --nassp-data-root "$NASSP_DATA_ROOT" \
  --profile baseline --orbiter-textures "$ORBITER_TEXTURE_ROOT" --mission insertion
python3 tools/nassp_demo.py stage --nassp-data-root "$NASSP_DATA_ROOT" \
  --profile baseline --orbiter-textures "$ORBITER_TEXTURE_ROOT" --mission liftoff
```

Each stage command prints JSON containing `launch_command`. Run that command
from this same `orbiter` directory. Its executable/runtime paths are relative to
this checkout. Staging records provenance in `demo-preflight.json`, retains the
supplied original scenario bytes and creates runtime links to explicit asset
roots. Reusing output refreshes helper-owned links, rejects regular/unknown
mesh/texture files and foreign directory links, and records the actual resolved
runtime roots plus binary/resource hashes. A collision asks for a fresh output
directory and preserves user-owned files. The helper does not open or change an already running app. `--headless`
prints a load-only command. `check` verifies a build and prints its launch command
without staging. It rejects foreign CMake caches and inconsistent data/source
selection. Interactive controls and save names are in
[original-mission-demo.md](original-mission-demo.md).

To run the sustained insertion check after staging:

```sh
python3 Tests/NativeOriginalInsertion.py build/native-poc/orbiter \
  build/native-poc/runtime-original-insertion \
  "$NASSP_DATA_ROOT/Scenarios/Project Apollo - NASSP/Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 02 - Insertion.scn" \
  --coast-seconds 900
```

## Isolated candidate

Prepare separate candidate **source** and **data** checkouts at the candidate
commit. Apply the nonempty patches in `upstream/nassp-candidate` in numeric order
to the source checkout only; [upstream-native-adoption.md](upstream-native-adoption.md)
records reconstruction evidence and limitations. Configure a separate build:

```sh
cmake -S . -B build/native-candidate -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DORBITER_PORTABILITY_CHECKS=ON \
  -DORBITER_BUILD_NASSP=ON -DORBITER_BUILD_WGPU=ON \
  -DNASSP_ADDON_DIR=/absolute/path/to/adapted-candidate \
  -DNASSP_DATA_DIR=/absolute/path/to/candidate-data -DNASSP_DATA_PROFILE=candidate \
  -DORBITER_TEXTURE_DIR="$ORBITER_TEXTURE_ROOT"
cmake --build build/native-candidate -j4
ctest --test-dir build/native-candidate --output-on-failure
```

Use `--build build/native-candidate --profile candidate` and its data root with
the same helper. Baseline/candidate synthetic smoke and terrain tests keep their
own fixture imagery; original mission stages use the explicit planetary package.
A green suite establishes native compatibility, input/render checks and short
mission save/resume behavior. It does not establish Windows trajectory parity,
a complete Apollo mission or a guided lunar landing. The clean-checkout record
is `upstream/validation-clean-macos-2026-10-07.json`.
