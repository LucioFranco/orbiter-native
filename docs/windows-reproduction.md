# Reproduce the stock Windows reference comparisons

The Windows reference runs official OpenOrbiter/NASSP v9 Windows packages; it
does not build this fork's native Windows implementation. Both reference sources
are locked to the same hashes as the native baseline. The collection environment
is Windows 11 ARM64 under UTM, executing x64 Orbiter/NASSP through Windows
emulation. This environment and the bounded observation limits belong to every
comparison result. A hardware Windows run remains a separate validation gate.

## Retained reference protocols

| Comparison | Original scenario / protocol | Supporting reproduction and results |
| --- | --- | --- |
| Paused startup / 301 total steps | Original Apollo 11 PDI T-15m, no checklist input, full-precision 1/60 step | [Matched parity protocol](../orbiter/docs/native-v9-matched-parity.md) |
| Neutral/pulse step 319 and 559 | Numpad3 requested for 302–319, release before 320, neutral settle through 559 | [Input observer sources and instructions](../orbiter/upstream/input-diagnostic/README.md), [actual comparison](../orbiter/docs/native-v9-step-input-comparison.md) |
| Sixty-second coast | 1 startup + 3600 neutral steps, original scenario and force configuration | [Coast comparison](../orbiter/docs/native-v9-coast60-comparison.md), [repeatability observer](../orbiter/upstream/repeatability-diagnostic/README.md) |
| Landed exterior sunlight | Original lunar liftoff T-1m, paused at initial boundary, FOV 72 degrees, requested camera 28 m and then about 10.7 m | [Native matched images and Windows scope](../orbiter/docs/native-v9-sun-occlusion.md) |

The instructions below are reconstructed from retained setup/launch/build
manifests and scripts, without running a VM or simulator again. The compact
[Windows reference inventory](../orbiter/upstream/windows-reference/environment.json)
records the inspected identities. The original setup manifests are dated stages:
an early `guest unavailable` or `execution unvalidated` field does not override
the later completed capture/cleanup records.

## Packages, guest and graphics route

The actual host was an Apple M4 MacBook Air with 32 GiB RAM, macOS 26.2 (25C56),
UTM 5.0.5 build 124. The QEMU/HVF aarch64 VM used four CPUs, 8192 MiB RAM,
80 GiB expandable NVMe disk, UEFI/TPM, shared networking and guest-agent transport.
Installed guest evidence identifies Windows 11 Home ARM64 10.0.26300.9457;
OpenOrbiter build `261009` is the simulator's build label, not the Windows build.
The original VirtIO 3D driver was 100.6.101.58000. The successful reference route
used Microsoft Basic Display Driver 10.0.26100.8521 with software Microsoft Basic
Render Driver D3D9; the retained VirtIO device was disabled (problem 22), not
uninstalled. Hardware acceleration was false.

| Input | Recorded SHA256 / verification |
| --- | --- |
| OpenOrbiter Oct 9 x64 ZIP, 377618438 bytes | `f00f767089e93d443252d35da58fcd0ccfb223c3b9f548437428cfc6e44cf00d`; publisher digest matched |
| NASSP v9 release 434 x64 ZIP, 355611892 bytes | `94c51753285d6682f391797910995586c30a3a70db2b96e1c55ac6ad6a52501f`; publisher digest matched |
| Orbiter 2024 textures-source ZIP, 2872278397 bytes | `6c5d113a07e57f20d5ca931677633dd4f6bdb93f7c4240779c8175503882b52e`; observed hash, no publisher digest recorded |
| UTM tools 0.1.273 ISO, 140195840 bytes | `7d2c0343e92358ad5e65078b08ec1dad873eb91b3c154aef60531bf6c2f04601`; publisher digest matched |
| Microsoft DirectX June 2010 redist | `053f76dcbb28802e23341b6a787e3b0791c0fa5c8d4d011b1044172dbf89c73b` |
| Microsoft VC++ x64 redist | `843068991daaa1f73ad9f6239bce4d0f6a07a51f18c37ea2a867e9beca71295c`; exact installed product version not retained in this inspected set |
| Windows ARM en-US 26300.9457 ISO | `8992f2d2cfd2fa647e579e9eb31fc21f34da8333c29b6170fde17d2ed6623e9c`; see discrepancy below |

The Windows ISO came from Microsoft's generated download link. Independent full
file hashes and internal metadata identified en-US ARM64 build 26300.9457, but
the matching Microsoft published hash appeared in its Arabic row rather than
the expected English row. The original installation proceeded under informed
user approval of that discrepancy. Do not claim the English-row check passed,
reuse that approval for a new user, or distribute the media. New installation
requires its own trusted media verification and license decisions.

Acquire official packages and verify hashes before extraction. Overlay the
OpenOrbiter ZIP first, **only `Textures/`** from the Orbiter 2024 ZIP second, and
NASSP release ZIP third into a fresh Windows reference directory. Do not overlay
the old Orbiter 2016 core. The original read-only UDF transfer ISO contained 4054
staged files and verified core/scenario/runtime hashes; a transfer ISO is optional
for reproduction and remains external. Install required Microsoft runtimes through
their normal licensed installers. Observed d3dx9_43/D3DCompiler_43 version is
9.29.952.3111; no security/driver-signing bypass is part of the recipe.

VirGL/VirtIO, a DX12 wrapper and Vulkan investigations did not yield the accepted
route and retained crashes/unsupported capability evidence. Do not present them
as accelerated successes. On another machine, use supported native D3D9 hardware
where available and record the changed backend. Recreating the disabled-device
fallback is a host-specific setup decision, not a mandatory security change.

## Guest transport and launch configuration

[`tools/utm_guest.py`](../tools/utm_guest.py) parameterizes the retained helper.
Supply an existing VM UUID from `utmctl list` and a caller-selected UTM executable;
it does not start a VM, use credentials or alter execution policy. It sends
PowerShell as UTF-16LE EncodedCommand through `utmctl exec`, and file bytes through
`utmctl file pull/push`. QEMU guest agent must already work. Stderr is treated as
failure even if utmctl returns zero; a guest script must report its own result.
New local pull destinations protect existing evidence.

```sh
python3 tools/utm_guest.py --vm YOUR_EXISTING_VM_UUID ps /absolute/read-only-probe.ps1
python3 tools/utm_guest.py --vm YOUR_EXISTING_VM_UUID \
  pull 'C:\ReferenceEvidence\case\saved.scn' /absolute/new/saved.scn
```

Guest-agent PowerShell may run in session 0 and cannot itself establish an
interactive cockpit session. Actual captures used temporary Scheduled Tasks with
an **Interactive** principal for the already signed-in user, no stored password,
a `.cmd` child action, simulator wall limit and task limit. Confirm the intended
user/session before dispatch; another user's desktop is not a valid target.
The helper supports transport only; no new live transport or task is exercised
by this documentation checkpoint.

The exact retained [Orbiter config](../orbiter/upstream/windows-reference/Orbiter.fixed.cfg)
has SHA256 `d5f4f65e16c3a66925e1c8e543eee2aa5f82b737abec12e39d4c1aa7578495e3`;
the [D3D9 config](../orbiter/upstream/windows-reference/D3D9Client.cfg) has
`332f9bf15f492b193fcb963fa72232ca5942e74f9d00613ca1d5a929a7540c6e`.
Preserve existing config/state files before using them. They select 640×480,
StartPaused TRUE, fixed step 0.016666666666666666, elevation mode 1, gravity
harmonics/distributed mass/radiation/orbit stabilization TRUE, wind/refuel/unlimited
fuel FALSE and local lights TRUE. Active modules are D3D9Client, ExtMFD, XRSound,
ProjectApolloMFD and ApolloRTCCMFD. DX9MFD is not selected. D3D9 wrapper is disabled;
normal mapping, absolute animations and package-defined texture mip handling are
retained. Post-close config rewrites may round FixedStep; the captured launch
config and command-line argument are the timing inputs.

## Build and identify the external input module

The retained [StepInputParity.cpp](../orbiter/upstream/input-diagnostic/StepInputParity.cpp)
SHA256 is `9212d36284056cfb3e7e75a3fb542a8c5a3222017a7f4500da9d6cc558dd15c2`;
[WinPrelude.h](../orbiter/upstream/input-diagnostic/WinPrelude.h) is
`4fd4325d7521bb5d453665a576efb7103c0522cca36e99aab6a8a02644ae7605`.
The public official ModuleAPI.h is unchanged (hash in the inventory). The source
uses public callbacks and no NASSP headers/physical-vector getters or intermediate
saves. Windows imports only Orbiter.exe and kernel32.dll.

Actual build used macOS-hosted clang-cl 21.1.8 targeting x86_64-pc-windows-msvc
and LLD 21.1.3 supplied with rustc-stable-2025-12-11. No Visual Studio or Windows
SDK installation was found in the guest. From repository root, a portable form
of the retained compile/link commands is:

```sh
export STOCK_SDK=/absolute/extracted-stock/Orbitersdk
export DRIVER_BUILD=/absolute/fresh/driver-build
export KERNEL32_IMPORT=/absolute/path/to/windows.0.52.0.lib
mkdir -p "$DRIVER_BUILD"
clang-cl --target=x86_64-pc-windows-msvc /std:c++17 /O2 /GR- /GS- \
  /EHs-c- /Zl /clang:-fno-builtin /clang:-ffp-contract=off \
  /I"$STOCK_SDK/include" /Iorbiter/upstream/input-diagnostic /c \
  /Fo"$DRIVER_BUILD/StepInputParity.obj" -- \
  orbiter/upstream/input-diagnostic/StepInputParity.cpp
rust-lld -flavor link /dll /machine:x64 /nodefaultlib /entry:DllMain \
  /out:"$DRIVER_BUILD/StepInputParity.dll" \
  "$DRIVER_BUILD/StepInputParity.obj" "$STOCK_SDK/lib/Orbiter.lib" \
  "$KERNEL32_IMPORT"
```

The recorded kernel32 import archive comes from `windows_x86_64_msvc-0.52.6`,
hash `24d8cbc445955b0d48041948a3c71ce2cddb948c089b25ec7106fadd9f3efde0`.
Record your actual compiler/linker versions, flags and output imports/hash.
Historical DLL is AMD64, 9216 bytes, SHA256
`e973659c2cf0a4c5202b86793ef08629958315e1d7a617abef010668c0a5b2b8`.
Tool/path changes may change binary bytes; validate structure and behavior rather
than relabel a different build as this original artifact. The current native
adapter has a later StartPaused/progress correction; historical package adapter
hashes are not claimed for that updated source.

## Neutral observer gate, then pulse captures

First collect four **fresh original** 559-step runs: `off-a`, `off-b` with no
input module, `neutral-a`, `neutral-b` with it. Do not load checkpoint saves.
Windows StartPaused performs step 1 before pause; resume with Ctrl+P, then send
no other physical input. Keep focus Eagle, dialogs closed, time acceleration 1.
The module callback prepares a subsequent input, rather than performing physics.

Temporarily place the verified DLL under `Modules\Plugin`, preserving any existing
file. Select it only with `--plugin=StepInputParity`, not an added persistent
ActiveModules entry. Child environment must be set in the task's `.cmd` action;
setting controller PowerShell environment alone does not reliably reach a separate
Scheduled Task. A caller-adjusted neutral action is:

```bat
@echo off
set "STEP_INPUT_MODE=neutral"
set "STEP_INPUT_TRACE=C:\ReferenceEvidence\neutral-a\trace.tsv"
cd /d "C:\Reference\OpenOrbiter-NASSP9"
orbiter.exe -s "Project Apollo - NASSP\Apollo - Mission Scenarios\Apollo 11\Apollo 11 - 10 - PDI T-15m" --fixedstep=0.016666666666666666 --maxsimtime=9.308333333333334 --maxsystime=300 --plugin=StepInputParity
exit /b %errorlevel%
```

Create a fresh case evidence directory first. Trace creation is exclusive. For
off runs omit plugin and child mode/trace. Use `cmd.exe /d /c` on the action from
the existing user's Interactive Scheduled Task; actual task wall limit was 600 s,
simulator system-time limit 300 s. Preserve task action, child environment, launch
config and recorded task/simulator outcomes. The stock CLI has **no `--headless`
flag**; omitting graphics modules selects its ConsoleNG path and `-s` enables
fast exit. A session-0/headless run is not cockpit/control evidence.

Only after exact neutral comparison passes collect `neutral-319`, `pulse-319`
and `pulse-559`, fresh original input each time. Use mode `pulse` for pulse cases:

| Total steps | Stop threshold `--maxsimtime` | Saved MJD | Both AGC counters |
| ---: | --- | --- | ---: |
| 319 | 5.308333333333334 | 40422.8318237093 | 453688 |
| 559 | 9.308333333333334 | 40422.8318700056 | 795022 |

Pulse requests numpad3 for steps 302–319 inclusive, eighteen active frames.
Release is prepared before step 320; step 559 is after 240 settling steps.
The 319-step run also performs its final input poll preparing neutral step 320.
Do not infer exact count from a rounded clock alone.

Trace schema is tab-separated: `start MODE`; `post COMPLETED SIMT_BITS DT_BITS
MJD_BITS`; `input PREPARED_STEP DOWN FAULTS`; `end COMPLETED PREPARED_NEXT
ACTIVE_FRAMES DOWN_REQUESTS FAULTS`. Hex values are IEEE double bit patterns.
DT must be `0x3f91111111111111`. Neutral ends `[559,560,0,0,0]`. Pulse ends
`[N,N+1,18,18,0]`, requests exactly 302–319, reports input320=0 and no pending
key after exit. All faults must be zero. The mode/environment and logger are
part of the capture profile; a completed callback count alone is insufficient.

## Cleanup and offline verification

For every case preserve ordinary final save, Orbiter/D3D9 logs, trace, launch and
result JSON, original/launch configs and raw hashes. Verify normal exit, freshly
written state, matching host/guest save hash, unchanged core binaries/quicksaves,
restored original configs/CurrentState/plugin, removed temporary DLL/wrapper/task,
and no simulator process. Cleanup should target the exact owned process/task,
not unrelated work. The actual final records certify these fields for all cases.

The parameterized [offline comparison helper](../tools/compare_windows_step_input.py)
reads case directories and writes only a separate fresh derived-output directory:

```sh
python3 tools/compare_windows_step_input.py \
  --evidence-root /absolute/extracted/evidence/step-input-windows \
  --output /absolute/fresh/neutral-analysis --require-neutral-gate \
  off-a off-b neutral-a neutral-b
python3 tools/compare_windows_step_input.py \
  --evidence-root /absolute/extracted/evidence/step-input-windows \
  --output /absolute/fresh/pulse-analysis neutral-319 pulse-319 pulse-559
```

This reproduces endpoint/trace/cleanup gates and complete-vessel diffs from retained
`launch.json`, `result.json`, `saved.scn`, `trace.tsv`. The neutral gate requires
all four exact complete normalized vessel blocks. Pulse differences are expected;
independently inspect channel 031/VICHAN025, yaw and RCS consumption rather than
accepting requests as proof of vessel response. Cross-platform comparisons retain
all finite-precision and systems residual limits in the linked reports.

The original Windows validation archive SHA256 is
`6ae7b917e326e5d3a195334c95db390ca4fd14f46763b61612aedcf52bfb6bde`.
Its CRC and all 153 listed file hashes/sizes were reverified during this
documentation checkpoint. All four Windows neutral raw saves are byte-identical,
SHA256 `a467c9b4bb07618cbb5a0b2f818f51c8423c2f4f7b7c8b8b66cf95f0682e6df6`.
The new portable helper reproduces their strict neutral gate without a new run.

## Match inputs and preserve ownership

Start with official packages corresponding to OpenOrbiter
`bd1d101f348da66e8c5b986633127c05e18d42c8` and NASSP
`e731c28c0ebc1a4fe3b63f99e2006a1a8a2b42a9` (release 434). Preserve download
identity/checksums, extracted stock binary hashes and original configs/scenarios.
Use matching Earth/Moon configuration, ROM/checklist data and planetary archives.
The two Moon archive hashes are in [native reproduction](native-reproduction.md).
Original PDI SHA256 is
`cef66d6824503ccc6c36a6cad46fcc967d33c53323565a3857dd6e7d238e7203`;
liftoff SHA256 is
`16b02ad534ee76dd07d98eb1e1b6c3c949d8a0346a03a305dbb8c420edb20f5b`.

Use isolated original scenarios/runtime copies and one simulator at a time.
Private comparison observers may be installed temporarily, but their identity,
configuration and input interval must be recorded. They request inputs; actual
channel/yaw/propellant response independently verifies delivery. Restore original
configs, CurrentState and plugin state afterward; remove temporary diagnostic
DLLs, wrappers and scheduled tasks, preserve stock binaries/quicksaves and
confirm no capture process remains. Do not distribute the guest/installer or
proprietary runtime setup as part of the source checkpoint.

For timing comparisons, distinguish the Windows paused startup step from the
manually counted neutral steps. Native manual drivers now use StartPaused FALSE;
simply resuming a TRUE-configured native host would add an extra startup step.
Record force settings and compare actual MJD/AGC boundaries as well as counts.
Normalize CRLF only in comparison copies; hash and retain raw Windows saves too.

## What the visual reference establishes

The 2026-10-12 liftoff exterior reference used D3D9Client with
Microsoft Basic Render Driver. The pale LM face and gold foil are visibly lit;
ground remains black. Requested exterior distances were 28 m and about 10.7 m,
with FOV 72 degrees and initial MJD 40423.7519591566. Complete stock camera/Sun
vectors were not measured, so this is qualitative confirmation beside the native
matched before/after pair, not pixel or photometric equality.

All three launch/save/close processes exited 0, but three surface-experiment
modules failed and material-index/shutdown surface/reset warnings remain in logs.
Exit 0 is not evidence that the whole original scenario loaded without errors.
Do not substitute a software-rendered/emulated reference for native Windows GPU
performance, complete mission validation or renderer parity.

## Evidence policy

Preserve package checksums, VM/OS/backend identity, exact commands, stock and
observer binary hashes, original input hashes, configs, raw logs/saves, input
requests, capture dimensions/encoding and cleanup outcomes. Commit compact
manifests and reusable observer source; retain larger evidence in external
hash-verified archives. State comparison residuals stay visible, including
gimbal-loader startup differences, fractional PIPA/chemical/thermal differences
and incomplete AGC snapshots. Counterfactual modified-state runs are diagnosis
only and must not replace production acceptance inputs.
