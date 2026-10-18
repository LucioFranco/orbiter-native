# Project documentation

Read [the root overview](../README.md) first. This index describes the current
locked native v9 baseline. Reports under `orbiter/docs` retain their recorded
date, source revision and experimental scope; an older green report does not
override a newer strict failure.

## Build, operate and reproduce

- [Native macOS reproduction](native-reproduction.md): prerequisites, data,
  build, the complete suite, fresh paused CLI launches and evidence collection.
- [Windows reproduction](windows-reproduction.md): official package reference
  runs, VM/emulation limits and exact comparison protocols.
- [Bounded operator demo](../orbiter/docs/native-v9-operator-demo.md) and
  [acceptance matrix](../orbiter/docs/native-v9-acceptance.md).
- [Locked source migration](../orbiter/docs/native-v9-upgrade.md) and
  [current complete suite](../orbiter/docs/native-v9-final-current.md).

## Architecture, decisions and next work

- [Architecture and portability boundaries](architecture.md).
- [History and adopted decisions](history-and-decisions.md).
- [Validation and open problems](validation-and-open-problems.md).
- [Publication, licensing and release gates](publication-and-release.md).
- [Current public-candidate treatment](publication-treatment.md),
  [component licenses](../LICENSE.md) and the dated [license audit](licensing/PUBLICATION-LICENSE-AUDIT.md).
- [Architecture research brief](research-baseline.md): proposed work, with
  evidence gates before changing implementation.

## Source and evidence ledgers

| Ledger | Purpose |
| --- | --- |
| [Portable baseline summary](../orbiter/upstream/project-baseline-2026-10-12.json) | Pins, scenario/archive hashes, outcomes and repository-relative supporting records |
| [OpenOrbiter native inventory](../orbiter/upstream/openorbiter-native.json) | Every tracked-tree difference, group and adoption/removal path |
| [NASSP v9 patch inventory](../orbiter/upstream/nassp-v9/manifest.json) | 92 adapted paths and the separate numbered patch groups |
| [NASSP v9 original data](../orbiter/upstream/nassp-data-v9.json) | Original data blob IDs and critical hashes |
| [Upstream source lists](../orbiter/upstream/nassp-manifests/v9/manifest.json) | 14 source-list exports with upstream identity and normalized hashes |
| [Complete suite record](../orbiter/upstream/validation-v9-final-current-macos-2026-10-12.json) | All 104 results, binary hashes and strict failure |
| [Bounded manual demo record](../orbiter/upstream/validation-v9-bounded-demo-macos-2026-10-12.json) | Saved ascent observations, input timing, integrity and limits |

Raw screenshots, VM disks, archives, full telemetry, installer media and local
backup bundles remain outside the source repository. Dated manifests retain
original local locations for provenance; those are evidence locators, not
prerequisites. New reproduction uses caller-selected paths. Some original CUA
captures have `.png` names but contain JPEG bytes; inspect encoding and dimensions
before reusing them. Hash the original bytes, without silently converting them.

## Detailed bounded investigations

- [Step-indexed keyboard/ACA comparison](../orbiter/docs/native-v9-step-input-comparison.md).
- [Sixty-second stock/native coast](../orbiter/docs/native-v9-coast60-comparison.md).
- [Repeatability diagnosis](../orbiter/docs/native-v9-repeatability.md).
- [Startup pause lifecycle](../orbiter/docs/native-v9-startpaused.md).
- [Below-datum sunlight and matched render evidence](../orbiter/docs/native-v9-sun-occlusion.md).
- [Gimbal motor loading](../orbiter/docs/native-v9-motor-load-comparison.md) and
  [thermal diagnosis](../orbiter/docs/native-v9-thermal-diagnosis.md).
- [Earlier original-mission source adoption](../orbiter/docs/upstream-native-adoption.md),
  [clean baseline/candidate builds](../orbiter/docs/clean-macos-demos.md),
  [PDI/P64 protocol](../orbiter/docs/macos-pdi-protocol.md) and
  [historical Rust investigation](../orbiter/docs/rust-rewrite-investigation.md).
