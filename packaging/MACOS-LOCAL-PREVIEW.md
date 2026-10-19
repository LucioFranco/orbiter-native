# Local Apple Silicon preview preparation

The executable-only assembly tool creates a **private local validation directory**.
It never archives, uploads, signs with Developer ID, notarizes, or enables a CI
binary artifact. The existing `preview_plan.py --check-ready` remains a release
gate and exits 2 while rights/source/notice/runtime reviews are open.

The 42 selected Mach-O files contain NASSP bitmap **IDs and paths**, not the
bitmap bytes: `NasspResources.cmake` generates a C++ lookup table while placing
397 normalized BMP files in `runtime/Resources/NASSP` outside the binaries.
The assembly copies none of those 397 files, no original NASSP scenario/ROM,
mesh, texture or sound, no planetary archive, and no raw Architext TTF. Native
Roboto and JetBrains Mono font bytes are compiled into the renderer and retain
their Apache-2.0/OFL-1.1 notices. The corresponding-source/notice audit is not
complete merely because the executable directory is small.

On Apple Silicon, with a complete v9 build and the same source revision:

```sh
python3 packaging/macos_preview.py assemble \
  --source-root /absolute/path/to/source-checkout \
  --build /absolute/path/to/source-checkout/orbiter/build/native-v9 \
  --dependency-notices /absolute/fresh/notices \
  --source-archive /absolute/fresh/nassp-preview-source.tar.gz \
  --output /absolute/fresh/local-preview
mv /absolute/fresh/local-preview /absolute/fresh/relocated-preview
python3 packaging/macos_preview.py verify --output /absolute/fresh/relocated-preview
```

The assembler copies only the exact contract binary paths, seven existing
license/notice texts, an inventory and the first-run helper. The separate
[corresponding-source recipe](CORRESPONDING-SOURCE.md) now stages the pinned
dependency source and an offline-buildable local archive. The target SBOM
recovers notice text for eleven Rust crates without a packaged root license:
eight from exact official upstream revisions and three from canonical MIT/CC0
texts plus package author metadata. The assembler rewrites each
absolute build `LC_RPATH` to `@loader_path` relative to the copied binary,
ad-hoc signs the locally modified Mach-O files, checks every dependency against
the package or a system framework/library, records architecture/minimum macOS,
and hashes every copied file. Ad-hoc signatures are not distribution signing.
`verify` checks hashes, unexpected files, runpaths and library closure after
the move. It refuses any absolute non-system dependency or escaped runpath.

The first run requires Python 3, Git, the same complete public source checkout,
a pristine pinned NASSP v9 data checkout, and a separately acquired Orbiter 2024
`Textures` directory with Earth/Moon archive files. The planetary check proves
presence and nonzero size only; it does not authenticate a release package.
Choose a writable directory that does not yet exist:

```sh
python3 /absolute/fresh/relocated-preview/run-preview.py prepare \
  --source-root /absolute/path/to/source-checkout \
  --nassp-data-root /absolute/path/to/pristine-nassp-v9 \
  --orbiter-textures /absolute/path/to/orbiter-2024/Textures \
  --runtime /absolute/fresh/liftoff-runtime --mission liftoff
python3 /absolute/fresh/relocated-preview/run-preview.py launch \
  --runtime /absolute/fresh/liftoff-runtime --headless
python3 /absolute/fresh/relocated-preview/run-preview.py launch \
  --runtime /absolute/fresh/liftoff-runtime
```

`prepare` verifies the exact source/data commits, the manifest's full original
asset set, all 397 panel bitmap hashes and the original-versus-adapted bitmap
ID/path map. It generates the BMPs into the user's runtime, stages unchanged
original scenario bytes, links only external user-owned assets and the relocated
package's modules, then writes a paused 1/60-second startup config. `launch`
checks package/input integrity again. The headless command loads the original
mission without GPU or audio and requests zero additional steps. The graphical
command is for a coordinated manual test on a desktop/Metal-capable machine;
the existing bounded [operator guide](../orbiter/docs/native-v9-operator-demo.md)
describes what has actually been demonstrated. Saves go to the writable runtime.

This packaging has no standalone binary preview download yet. Source checkout
is still needed at first run to supply engine Config/datasets and the vetted
staging helpers. The current full native suite remains 103/104, with strict AGC
save/reload failure. Full ascent/landing and faithful save resume are unverified.

## Remaining release gates

- Review the local corresponding-source archive and offline rebuild result for
  completeness, including local patches and the external bitmap input boundary.
- Review attribution for the three canonical-text fallback crates, nested
  native notices and source modifications before binary distribution. Preserve
  GPL exceptions and the existing native font notices.
- Review rights for any source/media included with a public binary; retaining
  the upstream Architext font in the source tree does not establish permission
  to ship its raw file in an executable package. External planetary packages
  need a verified lawful acquisition and authenticity story.
- Fresh Mac 15/arm64 relocation and bounded graphical/input/audio testing on
  the **same built binaries**; notarization/signing policy is separate. The
  assembler records the actual Mach-O OS floor and blocks unsupported claims.

The generated `source-plan.json` and `preview-manifest.json` are review aids, not
a source offer, a clearance decision or a release-readiness receipt.
