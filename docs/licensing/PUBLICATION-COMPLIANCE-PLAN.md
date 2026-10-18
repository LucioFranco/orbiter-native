> Historical audit, 2026-10-12. User decisions and current candidate treatment
> are recorded in [publication treatment](../publication-treatment.md).

# Native NASSP v9 artifact compliance plan

Baseline assessed: `49c0046ab581a78296edfa92ae5cf45c71be2b14`, 2026-10-12.
This is a proposed plan. No public artifact, hosted job or licensing change was created by the audit.

## Common source-publication gate

1. Resolve the hosted destination and current visibility through authorized read-only access. Select exact branch/tag/ref; do not mirror unrelated refs. Preserve the old clean demo independently.
2. Resolve or exclude Architext, proprietary/prebuilt SDK/tool/runtime files and assets without a documented grant. If publishing selected history, verify excluded bytes are not reachable from any ref being published. Obtain approval before history replacement or changing repository visibility.
3. Add a mixed-license overview and inventory. Preserve MIT engine notices, GPL NASSP grants, the yaAGC/Project Apollo exceptions and file-specific GPLv3/LGPL/third-party notices. Do not substitute a blanket MIT root license.
4. Make the original-glue/standalone-helper/Rust-crate license choice explicit. Ensure changed covered files carry required modification/date notices and include the applicable full license texts. Retain asset-specific notices separately from software terms.
5. Document exact source/data pins, platform distinction, minimum prerequisites, build and original-mission staging commands. Keep private machine paths out of the default quick start. A download instruction must not claim authority to redistribute or acquire a restricted SDK.
6. Scan the final selected history and source archive for secrets/private data and binaries; inspect embedded document/archive metadata. Existing pattern scan is limited, not a clearance of future commits.
7. Review the main worker's prepared documentation/publication diff against the actual pins, rights findings and platform evidence before pushing.

## Common binary-download gate

Each artifact must contain or link at the same release location:

- Platform/architecture, minimum OS, source commit, exact engine/NASSP pins, SHA-256 manifest, build profile and qualified support claims.
- Exact corresponding-source archive with complete covered source, interfaces, required editable/resource inputs, all local dependency patches, Cargo lock/vendor source and build/install scripts. Rebuild from that archive in a clean environment.
- `LICENSES/` with full applicable texts; a human-readable third-party notice list; selected alternative-license choices; resolved compiled dependency SBOM; per-asset creator/source/grant ledger for included media.
- Accessible copyright, warranty, redistribution and license/source notices where the applicable GPL interface requirements call for them; review the inherited-interface exceptions instead of assuming every new interface is exempt or must be changed.
- Safe per-user installation/removal behavior, separated writable saves and a relocation test without source checkout/cache paths. Dependency verification must inspect actual shipped objects, not the source tree alone.
- No VM disks, Windows/macOS SDKs, DirectX installers, proprietary audio SDK archives or undocumented third-party DLLs. If runtime libraries are redistributed, retain their actual redistribution terms and notices.
- An honest test summary: the current 104-test run is 103 passed/1 strict failure, not full success. Manual START establishes initial climb, not full ascent/mission. Strict AGC resume, complete flight/landing, hardware-controller and full material/terrain parity remain unaccepted.

## Platform matrix

| Target | What is evidenced | Release implementation still needed |
| --- | --- | --- |
| macOS arm64 native SDL/wgpu/miniaudio | Current v9 build and 104-test registration; 103 pass/1 known strict failure. Coordinated original-mission graphics, matched sunlight images, bounded manual START separation/initial climb. | Adapt old packaging prototype to v9, inspect bundled resources/media rights, build from corresponding source, validate relocation/minimum OS, test fresh install/save paths, include notices. Choose ad-hoc preview versus authorized Developer ID/notarization; current ad-hoc signing is not notarization. |
| macOS x86_64 | Nix development-shell declaration and portable source intent. | Clean x86_64 build, architecture-specific dependency inspection and full applicable runtime/graphics/manual checks. No downloadable support claim based on an arm64 run. |
| Windows x64 stock OpenOrbiter/D3D9/NASSP reference | Selected parity/reference runs under ARM64 Windows with x64 emulation and software Microsoft Basic Render Driver. This is stock-Windows comparison evidence, not proof of native x64 hardware acceleration. | Separate clean build/reproduction guide; D3D9 GPL/LGPL notices/source; lawful VC/DirectX/irrKlang inputs; omit or obtain grants for irrKlang plugins, restricted font, third-party media and redistributables. Test on the architecture/driver class advertised before release. |
| Windows native SDL/wgpu/miniaudio port | Some WIN32 source/CMake paths exist. | Clean native Windows-port build, DLL import/export and deployment audit, backend/input/audio tests, original missions and saves. Do not label the stock D3D9 reference as this port's Windows artifact. Prefer the native audio path to avoid stock irrKlang packaging, subject to actual build results. |
| Linux x86_64/arm64 native | Nix shells declared; portable source and Vulkan-oriented dependencies. | Separate clean builds on each promised architecture, glibc/libstdc++ and X11/Wayland dependency choices, Vulkan backend and input/audio tests, original-mission staging, saves, relocation and install/uninstall. Use the native miniaudio path; the Windows stock XRSound target is not a Linux recipe. |

## Conservative first artifact

Start with a clearly labeled **macOS arm64 bounded preview**, whose license/source bundle uses a GPLv3-compatible combined-work policy with preserved component grants. Bundle only resources with documented rights. Keep large planetary archives and unresolved mission media as legally acquired external inputs. If required embedded panel resources cannot yet be cleared, publish source/patch instructions or an engine-only artifact rather than claiming a ready-to-run NASSP binary is cleared.

A lean source/patch release can preserve upstream adoption: MIT-compatible engine/platform changes and standalone helpers; original GPL-covered NASSP patches and exceptions retained; exact pins and reconstruction/removal instructions. Excluding assets must be reflected in the build/staging workflow and supported by legally obtainable inputs, not merely a README disclaimer.

## Existing packaging adaptation checklist

The separate `nassp-ci-packaging` prototype is prior work at `bafd021`, not an integrated v9 release workflow.

- Replace baseline-only source/data checks and metadata with the exact v9 profile and current original mission staging. Validate original fixture bytes and complete source-list reconstruction.
- Replace broad resource/config/source copying with reviewed allowlists and a per-asset ledger; preserve GPL-covered preferred inputs for every resource actually embedded.
- Keep exact fetched dependencies and Cargo vendor/checksums; generate license notices for the resolved target graph, including Apache-only and Unicode notices. Copying only six top-level license files is not a complete Rust notice audit.
- Verify all package files, detect unexpected external symlinks/absolute loader paths, and inspect non-system shared/static dependencies. The prototype relocates macOS dylibs only; implement Windows/Linux deployment separately.
- Do not equate a hosted-safe subset receipt with the complete local suite. Publish the strict failure, GPU/desktop coverage, manual scope and unresolved platform claims.
- Clean-build and test the actual packaged artifact from its accompanying source. Do not enable hosted CI, use paid runners, publish releases or change permissions merely by integrating this plan.

## Approval boundary

Preparation, read-only inspection and local documentation are complete audit activities. Public-history replacement, license adoption for original work, visibility changes, release publication, restricted-material permission requests and externally hosted CI require the user/main worker's actual authorized decisions. This audit does not authorize them and did not perform them.
