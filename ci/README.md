# Initial CI foundation

This capability-limited foundation targets the future public
`LucioFranco/orbiter-native` repository. It does not fulfill the goal of runnable,
installable builds on every OS. The root workflow schedules nothing in the
private predecessor or other repositories. The user/main worker owns activation.

| Standard hosted target | Checks performed | Claims still unavailable |
| --- | --- | --- |
| macOS 15 arm64 | Full native v9 engine/addon/wgpu compilation; CPU subset with pinned NASSP data | Full acceptance, hosted GPU/window/input/audio hardware, installable v9 preview |
| Ubuntu 24.04 x64 | Native engine CPU probe with NASSP/wgpu linkage off; separate locked renderer compile check | NASSP mission runtime, graphics, Linux installation |
| Windows 2022 x64 | SDK ABI test; separate native host, NASSP module and wgpu compile; bounded CPU/module/mission staging smoke | Hosted GPU/window/input/audio device validation, full mission acceptance, stock D3D9 parity, Windows installation |

Unix tools use the existing Nix lock. Linux additionally needs zlib in that
shell; the overlay includes that one-line change. macOS uses Apple's compiler
and SDK with deployment target 15.0. Hosted images are maintained by GitHub.
Public source and Cargo downloads are cached by OS, architecture and lock hashes;
original NASSP data, runtimes, compiled output, credentials and broad home/
workspace paths are not cached. Only successful pushes save caches. Fork PRs
restore public downloads with read-only permissions.

`ci/fetch_inputs.py --nassp-data` explicitly fetches pinned release434 original
data for ephemeral tests, verifies every recorded asset blob/critical hash and
verifies adapted source/manifests. It never fetches planetary archives. Public
availability/hash identity is provenance, not a redistribution grant. No data,
compiled resources, dependencies or binaries are uploaded.

## Test boundaries and strict failure

`test_plan.py` reads CTest's actual registrations. Hosted selection excludes
GPU/desktop capabilities and exact external-data requirements documented in
`test-capabilities.json`. It follows both fixtures and dependencies transitively;
automatic fixture insertion cannot reintroduce unavailable tests. Every selected
and excluded name/reason is reported. New test failures propagate.
`simulation` alone is not an exclusion. No CMake test registration or strict
AGC assertion is modified.

`native_data_preflight` runs in the hosted macOS subset. Its pinned NASSP-only
staging check requires at least 1048 distinct mesh/texture links; the original
1054-link floor remains when an external Orbiter texture package is supplied.
Both modes retain the source/data hash, collision and reused-output checks.

The dated complete macOS result remains **103 passed / 1 failed / 104 total**.
`native_pdi_pilot` strictly fails processor resume: saved cycle counter
124205511 reloads as zero while CDU FIFO persistence is absent. Every job's report
and summary records this with its source revision and timestamp. A green hosted
subset never changes that result or establishes full mission acceptance.
The test is unavailable in this hosted foundation because the recorded planetary
fixture is deliberately not acquired; that is separate from its strict failure.

On a separately capable full v9 build with explicit original/planetary inputs:

```sh
python3 ci/test_plan.py /absolute/path/to/full-v9-build --suite full
```

Full mode requires the strict gate to be registered, applies no name/label/fixture
exclusions, records failure and returns CTest's failing status. It does not mark
failure expected, relax assertions or grant a preview waiver.

Each report artifact contains only declared JSON/XML files plus scope and dated
acceptance context. The collector refuses symlinks and more than 5 MiB per job.
Four report artifacts retained seven days have at most 20 MiB of payload per run,
plus ZIP metadata. There are no package/release uploads, publishing permissions
or signing secrets. Job caps are policy 10, Windows SDK 10, Windows native 90,
Linux native 60 and macOS native 60 minutes. This is a 230-minute failure
ceiling, not measured cost.

## Local validation and integration

From the final integrated source tree:

```sh
python3 ci/check_policy.py
nix develop path:./orbiter/nix --command python3 ci/fetch_inputs.py --nassp-data
nix develop path:./orbiter/nix --command python3 ci/build_native.py --nassp
```

Linux omits both NASSP flags and additionally executes the workflow's locked
Cargo check. Windows has a separate MSVC developer-shell path:

```bat
python ci/fetch_inputs.py --nassp-data
python ci/build_windows.py
python ci/test_windows.py
```

The Windows path uses Ninja, MSVC x64, pinned public sources and original NASSP
data in a local cache. It builds `orbiter.exe`, native module DLLs and the locked
Rust/wgpu renderer. DLLs are staged in their runtime module directories; the
loader searches the runtime Modules and ProjectApollo directories for dependent
DLLs. Windows mission staging copies and verifies assets into a fresh runtime
directory without symlink privileges. These are build/test inputs only, not a
redistribution approval or an installable package. The stock Windows reference
still validates a separate upstream application, not this native host.
Windows builds zlib statically from OpenOrbiter's v1.2.11 source pin because the
hosted MSVC image has no zlib development package; Unix retains system zlib.
Configure/build failures are fatal and
unsupported targets are rejected. Use a fresh checkout-specific CMake cache.
The source overlay corrects six upstream celestial-module includes from
`CelbodyAPI.h` to the actual SDK filename `CelBodyAPI.h`; case-sensitive Linux
builds need that spelling. The engine ledger records the six source differences
and their upstream adoption gate.

Integrate only the overlay paths. Append `/.ci-cache/` and `/ci-reports/` to the
final root ignore file if absent; do not replace it. Re-run policy/actionlint
against the final publication tree and verify the public destination before
activation. Rights-sensitive publication changes can affect required inputs:
missing source/resources must fail rather than trigger a broad restoration of
private history or a silent partial build.

## Concrete next work

Windows needs hosted build results reviewed after integration, then an independent
fresh-machine relocated install, full original mission and real GPU/input/audio
checks before claiming a preview. The CPU subset covers DLL lifecycle, ephemeris
simulation, NASSP save/reload and byte-preserving original scenario staging;
it does not resolve the known strict AGC resume failure. Stock D3D9 comparison
is separate; SDK success alone does not prove the native port.

Linux needs full NASSP/wgpu linkage, explicit glibc/libstdc++, X11/Wayland and
Vulkan deployment choices, then original missions, saves, graphics, input/audio
and relocated install/uninstall checks. A no-display CPU/renderer compile probe
does not establish runtime packaging.

The first macOS preview needs the exact inventory/resource/notice gates in
`packaging/README.md`, a complete matching-source rebuild, fresh relocated install
and qualified graphical checks. Other packages stay unsupported until validated.
No signing/notarization or font replacement is part of this foundation.
