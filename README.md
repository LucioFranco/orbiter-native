# Orbiter Native — native portability experiment

This project runs the original OpenOrbiter simulation and NASSP Apollo spacecraft
code natively on macOS, with a native SDL3 host and Rust/wgpu graphics client.
The goal is reproducible original mission demos and removable upstream changes,
then a validated basis for Windows and Linux builds and architecture research.

The current baseline locks OpenOrbiter to
`bd1d101f348da66e8c5b986633127c05e18d42c8` and NASSP v9 release 434 to
`e731c28c0ebc1a4fe3b63f99e2006a1a8a2b42a9`. Release tags are provenance labels;
the commit hashes select the inputs. Earlier Orbiter 2016/NASSP v8 experiments
remain dated historical evidence.

On 2026-10-12, the complete native macOS suite recorded **103 passed, 1 failed,
0 skipped, 104 total**. The strict AGC save/reload gate fails and remains enforced.
Live inspection verified cockpit instruments, pause/resume, manual ascent START
and release, separation, plume and initial climb. Full missions, lunar orbit,
manual landing, physical controllers and faithful AGC resume remain unverified.

Start with the [project documentation](docs/README.md). The principal paths are:

| Need | Entry point |
| --- | --- |
| Build and reproduce the native baseline | [Native reproduction](docs/native-reproduction.md) |
| Operate the bounded original mission demo | [Operator guide](orbiter/docs/native-v9-operator-demo.md) |
| Understand the implementation | [Architecture and portability boundaries](docs/architecture.md) |
| Understand how the baseline evolved | [Project history and decisions](docs/history-and-decisions.md) |
| Assess what the evidence proves | [Validation and open problems](docs/validation-and-open-problems.md) |
| Reproduce stock Windows comparisons | [Windows reproduction](docs/windows-reproduction.md) |
| Prepare licensing and future build sharing | [Publication and release gates](docs/publication-and-release.md) |
| Plan research from this baseline | [Research brief](docs/research-baseline.md) |
| Inspect scoped CI | [CI foundation](ci/README.md) |
| Inspect preview gates | [Packaging preparation](packaging/README.md) |

`orbiter/` contains the active engine and `orbiter/Addons/NASSP/` the adapted
v9 addon. The private POC archive retains the historical top-level `NASSP/`; this
clean source candidate omits that inactive duplicate. Matching original NASSP data and planetary archives are explicit
external inputs. Independent original work uses MIT; inherited grants remain authoritative.
See [component licenses](LICENSE.md) and the [current publication treatment](docs/publication-treatment.md).
Binary previews remain gated. The pinned Architext font is retained unchanged;
its raw-file publication permission is an explicit final review caveat.
