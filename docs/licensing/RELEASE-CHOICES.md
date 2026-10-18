> Historical audit, 2026-10-12. User decisions and current candidate treatment
> are recorded in [publication treatment](../publication-treatment.md).

# Licensing choices before publication

Fresh local audit of `49c0046ab581a78296edfa92ae5cf45c71be2b14`, 2026-10-12. These are proposed choices; no project license, package, push or visibility change has happened. The assessment is not a legal guarantee.

## What is already licensed

| Actual component | Verified terms | What that means |
| --- | --- | --- |
| Pinned OpenOrbiter engine | MIT, `orbiter/LICENSE` | Preserve MIT copyright/permission/warranty. This does not override nested third-party licenses. |
| Main NASSP, PanelSDK and yaAGC | Mostly GPLv2-or-later; Project Apollo and yaAGC retain Orbiter linking exceptions | NASSP derivative changes retain GPL obligations. The exceptions do not give rights to assets or proprietary dependencies. |
| NASSP LDPP/LWP processors | GPLv3-or-later, actual source headers and MCC/RTCC source lists | A blanket GPLv2-only description is wrong. A GPLv3-compatible combined download is the practical choice. |
| D3D9Client | Per-file GPLv3/LGPLv3, LGPLv2 and MIT | Stock Windows D3D9 needs its own source/notice compliance; it is not the native wgpu renderer. |
| Native SDL3/ImGui/stb/miniaudio/Lua/wgpu | Permissive licenses; some Apache-only Rust dependencies | Keep their notices/source as required. Native audio uses miniaudio, not irrKlang. |
| Actual embedded fonts | Roboto Apache-2.0, JetBrains Mono OFL-1.1 verified from TTF metadata | Keep font-specific notices and conditions. These are separate from the restricted stock Windows font. |

The legacy `NASSP/COPYING.txt` warning describes old non-Free Orbiter. It is **absent from the exact v9 release434 tree**. Its historical premise does not describe the pinned MIT source engine, but neither that change nor the linking exceptions clears third-party media or all combined-distribution duties.

## What blocks public release of the full current history

- **Architext font:** tracked standalone TTF; nested license requires written permission to publish. Stock Windows CMake installs it. Exclude/replace it in the chosen public tree/history, or obtain a documented grant. A deletion at the tip does not remove ancestor bytes. [Pinned license](https://raw.githubusercontent.com/orbitersim/orbiter/bd1d101f348da66e8c5b986633127c05e18d42c8/Extern/imgui/architext.regular%20-%20FFC%20License.txt).
- **Prebuilt third-party files:** Microsoft runtime/import libraries, SDK tools and other DLL/EXE/LIB outputs are tracked. Their individual redistribution authority has not been established. Avoid publishing or bundling them as uniformly open-source material.
- **Assets:** meshes, panel bitmaps, audio, third-party music, manuals, AGC variants and planetary archives need a creator/source/grant ledger for the actual shipped set. Upstream-matching hashes do not supply permissions. Some required mission textures/scenarios/archives are external, but much media is already tracked and panel resources are compiled into binaries.
- **Stock Windows audio:** CMake fetches an archived irrKlang SDK and installs its runtime plus two plugin DLLs. Vendor terms distinguish permitted unmodified non-commercial runtime files from the rest of the archive; plugin grants and lawful build acquisition remain unresolved. Downloads now say existing licensees only. [License](https://www.ambiera.com/irrklang/license.html), [download rule](https://www.ambiera.com/irrklang/downloads.html).

## Practical options

| Option | Benefit | Conditions still needed |
| --- | --- | --- |
| **Audited public source/patch release** | Preserves upstream adoption and avoids bundling uncertain runtime data | Choose an allowlisted public tree/history; exclude restricted font/prebuilt/unresolved assets; preserve all upstream notices; add required change/date notices; provide legally obtainable matching inputs. |
| **macOS arm64 bounded preview download** | Builds on actual current native evidence | First option's source/notice work plus exact corresponding-source archive, reviewed embedded resources, dependency SBOM, clean source rebuild and relocated-package tests. Keep unresolved large/mission assets external or clear their rights. |
| **Full ready-to-run or cross-platform downloads later** | Easier installation across systems | Complete asset rights, platform packaging and architecture/driver tests. Stock Windows reference runs do not validate the native Windows port; Linux and Intel macOS artifact support are not yet proven. |

For original code that is independent of NASSP, **MIT for engine/platform glue and standalone helpers** best matches upstream reuse. Preserve GPL grants for NASSP derivatives and a GPLv3-compatible combined application. Alternatively choose GPLv3-compatible terms for all original contributions. Neither option relicenses upstream code/media. Record the owner and the local `orbiter-wgpu` crate's explicit license; do not replace existing nested licenses.

Every GPL-covered binary needs corresponding source/build/install/resource inputs, applicable GPL text and notices at the release location. Include component/font/Unicode/Apache notices and required source modification dates. A separately acquired asset/download script is useful only when acquisition is lawful; it does not enlarge rights or clear bytes still included in the release.

## Evidence and limits

Actual local inspection covered 8,262 tracked files and all 23 commits reachable from the baseline. No blob exceeds GitHub's 100 MiB limit. No high-signal credential candidates were found in 4,501 text blobs; binary, large-blob and embedded document metadata scans remain incomplete. Local paths/task identifiers need a privacy decision.

The current complete suite records **103 passed, 1 strict failure, 104 total**. Actual macOS manual START evidence supports separation and initial climb. Full ascent/mission, strict resume and full terrain/material parity remain unaccepted. The stock Windows reference used software D3D9 on ARM64 Windows emulating x64; it is a qualified comparison reference.

Decide the public-source boundary/history, original-work license/owner, included-versus-external assets, first supported artifact, hosted destination/visibility and commercial intent before changes. Full evidence is in `PUBLICATION-LICENSE-AUDIT.md`, `COMPONENT-LICENSE-INVENTORY.csv` and `PUBLICATION-COMPLIANCE-PLAN.md`.
