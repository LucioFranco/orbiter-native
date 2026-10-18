# Public development candidate and retained POC

Decisions confirmed 2026-10-18: keep `nassp-universal` private as the POC archive;
prepare a fresh development repository named `orbiter-native` under LucioFranco;
MIT-license genuinely original independent code; preserve inherited licenses and
attribution; follow upstream asset distribution and use separately acquired exact
inputs where needed. Retain dependencies/build structure and the validated
implementation. Three-OS CI and gated previews are approved in principle, with
platform capability and packaging gates explicit.

The user creates the hosted repository. Its actual URL, empty/public status and
selected content require verification and review before any push. This local
candidate has fresh history and no inherited private Git objects/remotes. The
private archive/history is not rewritten or made public.

## Selection, rather than a new implementation

The [export ledger](../publication/export-inventory.json) records each omitted
path, original byte count/hash and concrete reason. Most omissions are the
inactive duplicate root `NASSP/`; the active addon remains
`orbiter/Addons/NASSP`. No simulation equation, ROM value, scenario state,
acceptance tolerance, input protocol or rendering behavior is changed.

Existing source and project-granted assets are retained with upstream notices.
This is not an assumption that every image/audio file is unlicensed. The dated
component and asset inventories distinguish project grants from specific scoped
exceptions and unresolved artifact rights. Asset provenance hashes establish
identity; they are not a new copyright grant.

Specific selected exclusions are prebuilt SDK/runtime/tool libraries/executables,
binary office documents with separate acquisition paths, NASSP program ROM binaries,
Solar Serenity track/artwork with an upstream-specific third-party grant, and
Skylab media whose notice refers to an absent scope-defining manual. The 28 editable
PSD project files, three source archives (including lunar ephemeris code/terms),
and four OpenOrbiter star/constellation catalog `.bin` files remain selected. The catalogs are runtime inputs under the upstream project
source tree, with dataset rights still marked for package review. An initial
extension-only `.bin` exclusion made `star_catalog` fail; restoring those exact
four upstream-pinned files made its focused test pass. Associated
license/attribution text stays in source. Exact NASSP data remains a separately
selected pristine release434 checkout, as in the POC. Original mission staging
gets its ROMs/checklists/full media from that root; users retain its terms.

The source candidate retains necessary panel resource source bitmaps under the
upstream project grant and records their origin. Packaging is held pending the
selected resource/source/notice review; separating external planetary data does
not mean embedded panels disappear from a binary or need no review.

## Architext: preserve preference, resolve publication

The private POC retains the unmodified upstream Architext TTF, SHA256
`a6b8e33ce190ee9c871590b982cff37e137b303bb73dcab55ef3ff7154ba143a`.
The upstream [FFC notice](../orbiter/Extern/imgui/architext.regular%20-%20FFC%20License.txt)
is preserved. Its distribution clause requires written permission to publish;
its separate embedding clause does not conclusively resolve installing a loose
TTF in a downloadable application's `Fonts` directory.

**Proposed source-publication treatment, awaiting review:** keep the TTF out of
the public Git history and retain its license, pin, hash and setup instructions.
A user who lawfully obtains the unchanged upstream copy can supply it locally at
`orbiter/Extern/imgui/architext.regular.ttf`. That path is ignored in this candidate
so it cannot be accidentally committed. The stock CMake font-install rule is
unchanged and will install that copy into `Fonts`, following upstream. Without
the separate copy, the stock font installation cannot complete; the native build
does not use that install rule. No automatic download or author contact occurs.

No font has been replaced or permanently removed from the private baseline.
The working native renderer already used embedded Roboto/JetBrains Mono before
this publication work; this candidate does not change that mapping or assert
that the native app was previously rendering with Architext. Selecting Architext
as a new native font would be a separate implementation decision.

Publicly bundling the raw TTF, or an app installing it as a loose file, remains
blocked until the grant is clarified. Do not infer that matching upstream bytes
or keeping it inside an app automatically clears that distribution. The user
preference to retain Architext is preserved while that question is resolved.

## Provenance and honest validation

The private baseline remains `49c0046`; its complete registered run tested
`0bd60e8` and records 103 pass/1 strict failure/0 skip/104. The strict AGC cycle/
CDU FIFO resume failure and separate exact-repeatability failure remain visible.
The initial manual climb does not establish a full ascent/orbit/mission.

Public copies of historical documentation/validation metadata normalize local
workspace/task locators. The export ledger records original and public-copy
hashes where normalized. Source patches, original scenario/config hashes,
physics values and failing assertions are preserved. Raw private evidence,
screenshots, VM images, installers and backup archives are not imported.

The candidate completed a fresh 819-step macOS native build. Portable helper
and link checks passed; the corrected CPU subset outcome is recorded in the
local validation checkpoint. The integrated CI overlay still requires independent
review of its report upload path before publication. CI reports must state each
platform's actual coverage. Binary uploads remain held by packaging,
corresponding-source, resource-rights, dependency-notice, relocation and platform
validation gates. Do not run hosted jobs in the private archive.
