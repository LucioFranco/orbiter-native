# v9 preview packaging preparation

No binary artifact is enabled by this overlay. The old `bafd021` prototype's
baseline-only launcher and broad source/resource copies are not carried forward.
This is preparation for a validated macOS arm64 bounded preview, not a cross-OS
installer or an acceptance waiver.

`preview-contract.json` lists **42 exact candidate binary paths** from the v9
runtime inventory, required notices and blocking conditions. The differential
test fixture `StepInputParity.dylib` is intentionally outside a normal original
mission package; its source remains required where applicable. No source/resource
file is removed or rewritten.

`v9-input-review.json` inventories **617 exact source/resource/staging inputs**
and SHA-256 hashes under the current rules: all bitmap inputs selected by
`NasspResources.cmake`, its six RC/header inputs, unchanged native fonts/notices,
and exact engine/celestial/gravity/ephemeris inputs copied by the existing stager.
This is a review inventory, **not an approved redistribution allowlist**. An
unresolved entry must block a package; silently deleting it would break
reproducibility or change runtime behavior. Broader runtime datasets should remain
external only after the stager supports that boundary and tests it explicitly.

```sh
python3 packaging/preview_plan.py --source-root /absolute/path/to/v9-source
python3 packaging/preview_plan.py --source-root /absolute/path/to/v9-source \
  --build /absolute/path/to/v9-build --check-ready
```

Planning reads/hashes inputs without copying anything. Readiness fails with exit
2 while gates remain unresolved; Windows/Linux/Intel Mac targets are explicitly
unsupported. The tool never downloads, archives, signs, installs or publishes.

## Matching source and notices

Complete corresponding source must match the actual binary and contain the
preferred source for all compiled resources. Do not use a blanket suffix filter
or the public repository snapshot as proof of completeness. Assemble from the
reviewed publication tree plus exact lawful dependencies, required generated
inputs and all local patches; verify against CMake source/include/dependency
inventories, RC/header/BMP inputs and the resolved Cargo target graph. Include all
build/staging/install scripts and prove a clean offline rebuild from that archive.

Preserve OpenOrbiter MIT, NASSP GPLv2-or-later/GPLv3-or-later and Project Apollo/
yaAGC exceptions, CDK per-file grants and required modification/date notices.
The combined native preview uses a GPLv3-compatible distribution policy.
Only independent original helpers/glue receive the approved MIT grant.

The 14 component notice obligations in the contract come from the completed audit.
Include full applicable permission/copyright/warranty texts, not just SPDX names:
SDL zlib, ImGui/Lua/Corrosion MIT, chosen stb MIT and miniaudio MIT-0 alternatives,
their embedded decoder notices, and the **resolved** Rust dependency licenses/
NOTICE files. Apache-only and Unicode-3.0 terms are required; copying six top-level
dependency licenses is insufficient. Preserve the existing Roboto Apache-2.0 and
JetBrains Mono OFL-1.1 bytes and notices; no fonts are replaced.

## Release blockers and external setup

The audit has not cleared embedded NASSP panel artwork/preferred-source scope,
all selected engine datasets, the complete public history or proprietary/prebuilt
files. Restricted Architext is outside the native font selection but must be
resolved in the publication tree/history by the main worker. A deletion at the
tip does not remove historical bytes. Do not infer grants from upstream hashes.

Original NASSP Config/ROM/scenarios/checklists/meshes/textures/sounds remain
separately lawfully acquired pinned inputs, using the existing v9 manifest and
full preflight. Planetary archives have no authoritative package checksum/grant
clearance here and are never acquired automatically. SDKs, OS/VM images, DirectX
installers and proprietary irrKlang files are not runtime-package inputs.

Future installation needs per-user fresh-directory refusal, manifest verification,
safe extraction, platform/minimum-OS checks, separate writable saves, original
scenario byte preservation and data/source preflight before staging. Inspect every
shipped Mach-O/ELF/PE dependency for outside paths/redistribution obligations and
test relocation on each target. Windows staging must account for symlink
privileges; Linux must define its ABI/graphics dependency floor. This overlay does
not claim these deployment checks have passed for v9.

The strict `native_pdi_pilot` failure remains visible. A future bounded preview
needs an explicitly reviewed scope/known-limit decision; it cannot claim full
acceptance or faithful AGC resume.
