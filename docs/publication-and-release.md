# Publication and binary release gates

The agreed destination is a new public development repository named
`LucioFranco/orbiter-native`, created by the user. Keep `nassp-universal` private
as the POC archive. Verify actual destination, ownership, empty/public status
and reviewed content before any push. No existing remote history is rewritten.

The local candidate preserves the working native implementation and upstream
build structure. It carries a mixed-license overview, explicit original-work MIT
grant, preserved inherited terms, exact source/data pins and justified exclusions.
See [publication treatment](publication-treatment.md) for the actual boundary,
the user-selected unchanged Architext treatment and unresolved permission and packaging questions.

## Source and notices

The source snapshot includes the original eight behavioral NASSP patch groups
plus a separate `09-publication-notices.patch`. The latter adds only prominent
changed-file/date comments to 93 adapted paths. Removing that exact comment
recovers previous normalized source bodies byte-for-byte. It changes no
simulation equation, mission state or runtime policy and replaces no original
copyright, GPL grant or linking exception.

MIT applies to genuinely original independent host/renderer/glue/helpers; it does
not relicense NASSP derivatives or media. Main NASSP uses GPLv2-or-later grants
with exceptions; LDPP/LWP use GPLv3-or-later. D3D9 has per-file GPL/LGPL/MIT terms.
Preserve the [component inventory](licensing/COMPONENT-LICENSE-INVENTORY.csv),
font terms, dependency notices and source provenance. Complete GPLv2/GPLv3
texts are included under `LICENSES/`.

## Three-OS CI and previews

Three-OS CI and gated previews are approved for the new public repository.
The separately prepared workflow overlay must report actual platform scope:
headless portability/renderer compilation, Windows SDK ABI checks and qualified
macOS native-v9 CPU checks are not three proven runnable mission packages.
Keep strict AGC resume failing evidence and graphics/manual limits explicit.

Inherited files under `orbiter/.github/workflows` remain nested upstream
material; do not activate their scheduled releases or broad resource copying.
No hosted action runs in the private POC. Source/destination review precedes
the first public push and any automatically triggered workflow.

Binary preview uploads remain gated on selected media/embedded-resource rights,
full matching corresponding source and build/install inputs, target dependency
notices/SBOM, relocation/install checks, qualified platform tests and reviewed
signing/distribution policy. Do not solve an uncleared resource by silently
substituting a font, changing original scenario state or omitting required source.

## Release contents

Each eventual binary needs exact source revision and pins, checksums, build
profile, support limits and complete corresponding-source delivery at the release
location. Include dependency/resource inputs and local patches needed to rebuild;
patch files or assumed-complete GitHub auto archives alone are insufficient.
See the [GNU source-distribution guidance](https://www.gnu.org/licenses/gpl-faq.en.html#DistributingSourceIsInconvenient).

Do not bundle VM images, installers, proprietary SDKs, undocumented runtime files,
credentials or private raw evidence. Users acquire separately licensed original
data/prerequisites through lawful upstream routes. Architext is retained unchanged with its upstream notice at the user's direction;
raw-font publication permission is unverified and loose-file app packaging
remains gated as described in the treatment document.

Review current [Actions billing](https://docs.github.com/en/billing/concepts/product-billing/github-actions),
runner architecture and artifact retention before activation. An approved
three-OS plan does not authorize paid larger runners or imply all artifact storage
is free. Reports can precede distributable binary previews while gates are open.
