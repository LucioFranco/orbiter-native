# Component licenses

This repository contains separately licensed components. It has no blanket MIT
grant covering all source, media or dependencies.

| Component | Applicable terms |
| --- | --- |
| OpenOrbiter engine and SDK | [Upstream MIT grant](orbiter/LICENSE), with nested/per-file exceptions preserved |
| Independently authored native host, renderer, platform glue and standalone helpers | [MIT for original work](LICENSES/MIT-original.txt); existing third-party notices still apply |
| NASSP-derived source and native NASSP adaptations | [NASSP GPL text](orbiter/Addons/NASSP/NASSP-LICENSE.txt), file-specific GPLv2-or-later/GPLv3-or-later grants and preserved linking exceptions |
| D3D9Client | Per-file GPL/LGPL/MIT notices; it is separate from the native wgpu renderer |
| Fonts, libraries, data and media | Their individual notices and grants; see [component inventory](docs/licensing/COMPONENT-LICENSE-INVENTORY.csv) |

The original-work MIT choice does not relicense NASSP derivatives, imported
third-party code, fonts, images, meshes, audio, ROMs or datasets. The original
native Rust renderer crate declares MIT explicitly. Existing copyright, warranty,
attribution and linking-exception text remains in place.

NASSP's LDPP/LWP files grant GPLv3-or-later; describing the whole project as
GPLv2-only is incorrect. A combined preview must satisfy a GPLv3-compatible
distribution route while preserving each component's actual grants. Binary
distribution requires complete matching corresponding source and build/install
inputs, notices and reviewed resources. No binary preview is cleared here.

See [publication treatment and unresolved gates](docs/publication-treatment.md)
and [the dated audit](docs/licensing/PUBLICATION-LICENSE-AUDIT.md). Architext is
retained in the private POC; its standalone public-source treatment remains
proposed and its application-packaging grant unresolved. It has not been replaced.
