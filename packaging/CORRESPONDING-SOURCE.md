# Mac preview corresponding source (local experimental bundle)

The source-bundle script stages the native Mac v9 build sources, build scripts,
eight embedded native font inputs, six pinned C/C++ dependency code snapshots,
and all locked Rust crates. It excludes 397 NASSP panel bitmap bytes, other
original NASSP/Orbiter media, unused native-dependency test images/fonts/audio,
and the raw Architext font. The original media
remains separately user acquired and is checked by the native data and bitmap
preflights. Nothing here uploads or publishes a release.

For the same checkout used to build the preview, prepare clean local dependency
checkouts at the commits in `ci/inputs.json`, then vendor the locked crates:

```sh
cd orbiter/Src/WgpuClient/rust
CARGO_NET_OFFLINE=true cargo vendor --locked --versioned-dirs /absolute/cargo-vendor
cd ../../../..
python3 packaging/corresponding_source.py \
  --dependencies-root /absolute/dependency-checkouts \
  --cargo-vendor /absolute/cargo-vendor \
  --output /absolute/fresh/source-stage \
  --archive /absolute/fresh/nassp-preview-source.tar.gz
```

The dependency directory names are `SDL`, `imgui`, `stb`, `miniaudio`, `lua`
and `corrosion`. Every Git HEAD and cleanliness state is checked. Cargo's
vendored `.cargo-checksum.json` files keep registry-package checksums. The
archive contains `SOURCE-MANIFEST.json` with per-file SHA-256 hashes and pins.
The archive is a review artifact until the source/media rights review finishes.

The target notice inventory includes 11 Rust crates whose published package
lacks a root license file. Eight carry license texts retrieved from their
official upstream Git revision, with verified Git blob and SHA-256 hashes.
For `block`, `malloc_buf` and `hexf-parse`, the matching upstream revision
has no license file. Their package and upstream manifests declare MIT or CC0;
the inventory uses official canonical license text and records the Cargo author
metadata separately. No copyright year is inferred for those three crates.

With Xcode Command Line Tools and the pinned `orbiter/nix/flake.lock` toolchain
already installed, extract the source archive and build with network disabled.
Point `NASSP_DATA_DIR` at a separately acquired pristine NASSP v9 checkout and
`ORBITER_TEXTURE_DIR` at separately acquired Orbiter 2024 textures. Use paths
outside the source tree for the output and Cargo home:

```sh
export S=/absolute/extracted/nassp-preview-source
export B=/absolute/fresh/build
export D=/absolute/pristine-nassp-v9
export T=/absolute/orbiter-2024/Textures
export C=/absolute/fresh/cargo-home
export CARGO_NET_OFFLINE=true CARGO_HOME="$C"
nix develop "path:$S/orbiter/nix" --command cmake -S "$S/orbiter" -B "$B" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DORBITER_PORTABILITY_CHECKS=ON -DORBITER_BUILD_NASSP=ON \
  -DORBITER_BUILD_WGPU=ON -DNASSP_DATA_PROFILE=v9 \
  -DNASSP_DATA_DIR="$D" -DORBITER_TEXTURE_DIR="$T" -DORBITER_CARGO_HOME="$C" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
  -DFETCHCONTENT_SOURCE_DIR_SDL3="$S/vendor/cmake/SDL" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$S/vendor/cmake/imgui" \
  -DFETCHCONTENT_SOURCE_DIR_STB="$S/vendor/cmake/stb" \
  -DFETCHCONTENT_SOURCE_DIR_MINIAUDIO="$S/vendor/cmake/miniaudio" \
  -DFETCHCONTENT_SOURCE_DIR_LUA="$S/vendor/cmake/lua" \
  -DFETCHCONTENT_SOURCE_DIR_CORROSION="$S/vendor/cmake/corrosion"
nix develop "path:$S/orbiter/nix" --command cmake --build "$B" --parallel 8
```

Generate the target notice inventory from the extracted source using
`cargo metadata --format-version 1 --locked --offline --filter-platform
aarch64-apple-darwin` in `orbiter/Src/WgpuClient/rust`, then call
`packaging/dependency_notices.py --source "$S" --metadata /absolute/metadata.json
--output /absolute/fresh/notices`. The inventory names any package whose
vendored root has no standalone license file; inspect its source headers and
upstream license before treating a public binary as cleared.

This recipe reproduces the 42 candidate binary targets. The source-only archive
does not include Orbiter `Config`, Celbody ephemeris data, or test fixtures;
tests that stage a runtime need those inputs from a compatible original checkout.
The observed external test inputs include `orbiter/Config/**` (including
`Config/CSphere/Data/star.bin`), `orbiter/Src/Celbody/**/Config/**`,
`Src/Celbody/Vsop87/Data/*.dat`, `Galsat/ephem_e15.dat`,
`Satsat/tass17.dat`, and `orbiter/Tests/Fixtures/**`. Original NASSP data and
Orbiter planetary textures remain under the separate preflight. The full hosted
suite is therefore run against the complete, rights-reviewed checkout, while
source-only rebuild validation uses tests independent of those runtime files.
The native data preflight checks both in-checkout and external build paths while
requiring its displayed executable and runtime to resolve to the selected build,
independent of source and NASSP data. A graphical Metal run, second-machine
install, licensing review, and distribution signing remain separate from an
offline compile proof.
