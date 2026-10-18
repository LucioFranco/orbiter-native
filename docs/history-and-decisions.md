# Project history and adopted decisions

On 2026-10-18 the user chose a fresh public `orbiter-native` development
repository, retaining `nassp-universal` as a private POC archive. Original
independent code uses MIT; inherited grants remain. Three-OS CI and gated previews
are approved in principle. See [current publication treatment](publication-treatment.md)
for exact source scope and the proposed, unresolved Architext publication handling.
The private feature-branch publication plan below records the earlier phase.

The goal is to run real original NASSP missions on a native host, preserve
source/data provenance and identify changes that can be adopted upstream. A
successful demo is bounded evidence toward that goal, not permission to alter
mission physics or claim a full Apollo flight.

## Evolution of the baseline

| Stage | Recorded sources / milestone | What remains relevant |
| --- | --- | --- |
| Initial engine investigation | OpenOrbiter `4137930c5126cf848212139cd138a9ef2ade7660` | Windows ABI/platform coupling and renderer boundary; later rewrite recommendations remain proposals |
| Native original missions | NASSP CMake baseline `96b6380943c382a297f969842be9344ef969a6f4`; milestone `cd00b2f` | External original data, native services and bounded insertion/liftoff gates |
| Clean checkout / adoption | October candidate `5dffae9a73366604a66308617b7b9f6e7dbf6c60`; `macos-clean-demos` | Independently selected data and source, manifest-driven build policy, removable patch inventory |
| Old-pin PDI/P64 protocol | Clean `9e8d452e4a41c9b73bbc516e77b13a49d79f7c77` | Uninterrupted graphical handoff and synthetic lifecycle coverage; preserved independently |
| Current release migration | OpenOrbiter `bd1d101f348da66e8c5b986633127c05e18d42c8`; NASSP v9 `e731c28c0ebc1a4fe3b63f99e2006a1a8a2b42a9` | Current supported experiment, aligned with official stock Windows packages |
| Current complete regression | `0bd60e8105910096ece760cd614517bb5d0dbd5c` | One unfiltered 104-test run: 103 pass, strict AGC resume failure retained |
| Bounded manual closeout | Documentation checkpoint `49c0046ab581a78296edfa92ae5cf45c71be2b14` | Original liftoff START/separation/initial climb; precise acceptance and remaining limits |

The request for current versions corrected the earlier 2016/v8 selection. The
active NASSP source comes from rcflyinghokie's v9 Orbiter2024/OpenOrbiter release,
not the separate experimental Folgers/J-mission work. Moving tags are never the
lock. Historical reports remain available with their actual pins and count;
their 82/82, 94/95 or 98/99 outcomes are not the current 103/104 result.

## Adopted implementation choices

1. Preserve original C++ simulation and NASSP systems/guidance behavior. Port
   platform, host and graphics services; do not retune equations, mission state,
   ROMs or inputs to make a test pass.
2. Keep source adaptations separate from pristine matching original data and
   external planetary archives. Verify critical hashes, source exports and
   runtime bindings before launching.
3. Put native build selection in external engine CMake policy. Preserve
   authoritative upstream source lists and narrowly grouped removable patches.
4. Retain strict failure evidence. The AGC snapshot redesign is explicitly
   paused; a valid uninterrupted PDI handoff does not waive restart correctness.
5. Distinguish deterministic manual-step drivers from interactive startup.
   Drivers own every step with StartPaused FALSE; paused GUI initialization
   follows the original host sequence.
6. Correct below-datum daylight using actual sun/body geometry and regression
   evidence. Do not change Moon radius, vessel altitude, scene state or material
   brightness to produce a preferred screenshot.
7. Use stock Windows binaries as a bounded reference, preserve original configs
   and assets, label private observer/counterfactual runs, and retain full raw
   differences rather than inventing numerical acceptance tolerances.
8. Publish a reviewed feature branch first. Keep repository visibility and hosted
   CI unchanged; decide licensing and distribution scope separately.

## Proposals and paused work

The [historical Rust investigation](../orbiter/docs/rust-rewrite-investigation.md)
contains a staged Rust-core proposal. The current renderer uses Rust; a core
rewrite, stable replacement C ABI, addon SDK redesign or multi-world ownership
model has not been adopted. Some historical documents describe a private/local
phase or outdated upstream policy; this current decision record supersedes those
planning statements, while preserving their technical observations.

Full mission validation, faithful AGC snapshot redesign, broader parity,
physical-controller calibration, native builds on other OS/architectures,
licensing choices and release automation remain work to evaluate. See
[the research gates](research-baseline.md) and
[publication requirements](publication-and-release.md). No merge, release,
license replacement, visibility change or hosted build activation is authorized
by the documentation checkpoint alone.
