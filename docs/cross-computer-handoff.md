# Continue development on another computer

This is a source-development handoff for the public
[`LucioFranco/orbiter-native`](https://github.com/LucioFranco/orbiter-native)
repository, checked on 2026-10-19. Clone `main` afresh on the other computer and
record `git rev-parse HEAD` with every result. The last production/CI change
reviewed here is `fc892475a38aacfce5895da0cfb444830f51411f`; this handoff
adds documentation only. The Mac, Linux and Windows validation results below
belong to that exact source revision unless another revision is named.

## What is already portable

`main` contains the native OpenOrbiter host, adapted NASSP v9 addon, Rust/wgpu
client, build and test scripts, upstream-difference inventories, original
scenario staging tools, and two hosted workflows. The original source pins are
OpenOrbiter `bd1d101f348da66e8c5b986633127c05e18d42c8` and
NASSP v9 release 434 `e731c28c0ebc1a4fe3b63f99e2006a1a8a2b42a9`, also in
[`ci/inputs.json`](../ci/inputs.json). The 93 adapted NASSP paths and removable
patch groups are inventoried in
[`orbiter/upstream/nassp-v9/manifest.json`](../orbiter/upstream/nassp-v9/manifest.json);
the engine difference/removal ledger is
[`orbiter/upstream/openorbiter-native.json`](../orbiter/upstream/openorbiter-native.json).
The original Windows reference packages are a separate comparison, not a build
of this native fork.

The inspected worker checkouts are clean. `codex/windows-validation` at
`fc89247` and `codex/linux-runtime-fixes` at `d9c8655` are ancestors of
`main`. The older, divergent `codex/windows-runtime-milestone` at `903eee3`
has its implementation integrated as `b11ce05`; its changed paths match that
commit except for the updated engine inventory. The older local
`codex/linux-runtime-milestone` at `2b33305` is likewise superseded by
`824e217` and subsequent fixes. Neither old branch should be merged over the
newer `main`. Worker checkouts and build caches are local to the first Mac and
are unnecessary for a fresh clone.

## Fresh checkout and input boundary

```sh
git clone https://github.com/LucioFranco/orbiter-native.git
cd orbiter-native
git rev-parse HEAD
python3 ci/check_policy.py
```

Run from the repository root unless a command says otherwise. Python 3, Git,
CMake, Ninja, a C++ compiler/SDK, and Rust/Cargo are needed. Fetching build
dependencies requires network access. `ci/fetch_inputs.py --nassp-data` obtains
the exact public dependency commits and a separate pristine NASSP data checkout
under `.ci-cache/`, then verifies original asset blobs, source adaptations and
source lists. That checkout is build/test input, not a grant to redistribute
NASSP media. The repository does not include planetary texture archives;
original Earth/Moon archives must be acquired separately under their terms for
the full local mission runtime. See
[`native-reproduction.md`](native-reproduction.md) for the required archive
paths, recorded hashes and all-asset preflight. Never edit the pristine data
checkout to apply native patches.

Use a fresh checkout-specific build cache. On Apple Silicon macOS, install
Apple Command Line Tools and Nix, then run:

```sh
nix develop path:./orbiter/nix --command python3 ci/fetch_inputs.py --nassp-data
nix develop path:./orbiter/nix --command python3 ci/build_native.py --nassp --build-only
nix develop path:./orbiter/nix --command python3 ci/test_plan.py orbiter/build/ci
```

The hosted Mac build uses macOS 15 arm64 and Apple's compiler/SDK. The
[`native-v9` CMake preset and original-scenario guide](native-reproduction.md)
give the complete local build, external planetary inputs, strict suite and
bounded graphical demo commands. Coordinate a desktop/GPU run with other users
of that machine.

On Windows x64, use a Visual Studio 2022 MSVC x64 developer shell with Python,
Git, CMake, Ninja and Rust/Cargo available. The commands to reproduce its
current hosted gate are:

```bat
python ci\fetch_inputs.py --nassp-data
python ci\build_windows.py
python ci\test_windows.py
```

The build currently stops before `test_windows.py`, as recorded below. The
[Windows reference guide](windows-reproduction.md) covers the independent stock
OpenOrbiter/NASSP comparison on Windows 11 ARM64 under x64 emulation; it is not
evidence that this native Windows build runs a mission.

On Ubuntu 24.04 x64, install the compiler, CMake, Ninja, Rust/Cargo and the
development libraries listed in
[`linux-runtime.yml`](../.github/workflows/linux-runtime.yml). Reproduce the
combined NASSP/wgpu runtime job with:

```sh
python3 ci/fetch_inputs.py --nassp-data
python3 ci/build_native.py --nassp --build-only
python3 ci/test_plan.py orbiter/build/ci
python3 ci/diagnose_linux_runtime.py --build orbiter/build/ci
```

Set `SDL_AUDIODRIVER=dummy` and `SDL_VIDEODRIVER=dummy` for the hosted headless
environment. `ci/test_plan.py` records selected/excluded names and reasons in
`orbiter/build/ci/test-plan.json`, JUnit in `ctest.xml`, and its status in
`test-result.json`. Preserve failures. The full suite command on a capable
build with the required external planetary inputs is
`python3 ci/test_plan.py /absolute/path/to/full-v9-build --suite full`; it applies no test
exclusions. The dated complete Mac run at `0bd60e8` is **103/104**: the strict
`native_pdi_pilot` AGC save/reload check fails because the pending CDU FIFO and
timing state are not serialized. A broad snapshot redesign is paused. Do not
change mission state or weaken that test to make a suite green.

## Latest hosted evidence and next debug steps

| Target | Verified result at `fc89247` | Next step |
| --- | --- | --- |
| [Mac arm64 foundation](https://github.com/LucioFranco/orbiter-native/actions/runs/39199645681) | Native v9 build and capability-limited CPU suite **63/63 passed**. The strict full suite and desktop/hardware checks were outside this job. | Reproduce a complete external-data build and the strict 104-test suite before changing acceptance claims; separately test graphical/manual controls. |
| [Windows x64 foundation](https://github.com/LucioFranco/orbiter-native/actions/runs/39199645681) | Policy, Linux compile probe and Windows SDK job passed. Native Windows compile fails at [`orbiter/Tests/AnimationPose.cpp:4`](../orbiter/Tests/AnimationPose.cpp) with MSVC C2059/C2143/C2447 on `static void near(...)`; the previous `interface` macro error is gone. No native smoke ran. | Inspect the preprocessed declaration for a Windows `near` macro, rename the local helper if confirmed, then rerun the native build and `ci/test_windows.py`. Keep the test's vector assertions unchanged. |
| [Linux x64 runtime](https://github.com/LucioFranco/orbiter-native/actions/runs/39199645719) | Host, NASSP modules and wgpu compile. Headless suite **51/63 passed**, **12 failed**: two Python tests end after child timeouts, nine CTest timeouts, and `landing_sound_playback` segfaults. | Reproduce focused tests with a bounded runtime and inspect the [separate diagnostic run](https://github.com/LucioFranco/orbiter-native/actions/runs/39199296610) before changing lifecycle code. |

The Linux diagnostic run captured `landing_sound_playback` SIGSEGV in
`Mesh::Clear` (`orbiter/Src/Orbiter/Mesh.cpp:458`) while loading `Tarmac2` through
`MeshObject::LoadMesh` during `Orbiter::InitializeWorld`. The failing free has
an invalid-looking pointer in the stack, but the cause is unproven. A separate
interrupted `native_pause_test` stack waits in `pthread_cond_destroy` from
`Event::~Event` (`ProjectApollo/src_sys/thread.h:61`) while tearing down
`ApolloGuidance`/`Saturn`; identify thread ownership and joins before changing
the event implementation. Isolate the mesh load and teardown with a debug or
sanitizer build; preserve the original scenario/data bytes and compare against
the passing Mac path. The Linux failures repeat in the latest push run:
`native_manual_step_alignment`, `native_solar`, `native_rcs`, `native_pause`,
`native_startup_pause`, `native_nassp_resume`, `nassp_joystick`,
`nassp_joystick_flight`, `nassp_joystick_throttle`, `native_sound`,
`landing_sound_playback`, and `native_scenario_compatibility`. Treat the
similar timeouts as a hypothesis of shared lifecycle trouble, not a proven
single root cause. The diagnostic workflow is manually dispatchable with
`diagnose_linux=true` and captures stacks before the full headless suite.

## What still lives only on the first Mac

The private, relocated Apple Silicon validation preview and its matching
source archive are held outside Git at source commit
`6aec9c3b863a325b48e25927856480e16a5052de`. The archive SHA-256 is
`4f20b2dcbb7d9db6c329bf6919ead36cc9daa78649ef9547f5b4855c42d964c5`.
Neither file has been published or copied to the new computer. Preserve those
exact bytes and transfer them privately if further validation of that pinned
candidate is needed; rebuilding a later commit is a new candidate. The source
archive is not a replacement for the public repository and separately
acquired original data. Local raw screenshots, VM disks, full telemetry,
original planetary assets and caches likewise do not travel with `git clone`.

The preview remains a bounded local validation build. Its first-run path needs
the source checkout, pristine pinned NASSP data and external planetary
textures; 397 NASSP panel bitmaps are generated into the user's runtime and
are not bundled as bytes in the 42 selected binaries. See
[`MACOS-LOCAL-PREVIEW.md`](../packaging/MACOS-LOCAL-PREVIEW.md). The exact pinned
preview has not passed physical controls, full flight/manual landing or a
second-machine relocated install. Binary distribution remains on hold pending
the source/notice/resource-rights and target-install gates in
[`packaging/README.md`](../packaging/README.md). No binary release or hosted
binary artifact is implied by this source handoff.
