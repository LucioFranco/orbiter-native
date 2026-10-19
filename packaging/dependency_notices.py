#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Inventory Mac target dependencies and copy verified notice texts.

Reads Cargo's aarch64-apple-darwin resolved graph, not every cross-platform
entry in Cargo.lock. Missing packaged notices use exact official upstream Git
revision files, or canonical text plus clearly identified Cargo author data.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
NAMES = {'SDL3': 'SDL', 'IMGUI': 'imgui', 'STB': 'stb',
         'MINIAUDIO': 'miniaudio', 'LUA': 'lua', 'CORROSION': 'corrosion'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def copy_notice(src, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, target)
    return {'path': str(target), 'sha256': sha(target)}


def git_blob_sha1(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def upstream_evidence(stage, dest, pkg, checksum, evidence):
    name, version = pkg['name'], pkg['version']
    entry = evidence[(name, version)]
    if (entry['package_sha256'] != checksum or
            entry['license_expression'] != pkg['license'] or
            entry['authors_from_package'] != pkg['authors']):
        raise ValueError(f'Locked upstream notice metadata mismatch: {name} {version}')
    vcs = stage / 'vendor/cargo' / f'{name}-{version}' / '.cargo_vcs_info.json'
    if vcs.is_file() and json.loads(vcs.read_text())['git']['sha1'] != entry['upstream_revision']:
        raise ValueError(f'Upstream revision mismatch: {name} {version}')
    for item in entry.get('release_tag_source_blobs', []):
        source = stage / 'vendor/cargo' / f'{name}-{version}' / item['path']
        if git_blob_sha1(source.read_bytes()) != item['git_blob_sha1']:
            raise ValueError(f'Release-tag source differs from locked crate: {name} {item["path"]}')
    files = entry['upstream_license_files']
    if entry['upstream_metadata_file']:
        files = files + [entry['upstream_metadata_file']]
    outputs = []
    for item in files:
        flat = f'{name}-{version}__' + item['path'].replace('/', '__')
        source = stage / 'packaging/license-evidence' / flat
        data = source.read_bytes()
        if sha(source) != item['sha256'] or git_blob_sha1(data) != item['git_blob_sha1']:
            raise ValueError(f'Official upstream license evidence changed: {flat}')
        if item == entry['upstream_metadata_file'] and (
                f'license = "{pkg["license"]}"'.encode() not in data or
                f'version = "{version}"'.encode() not in data or
                any(author.encode() not in data for author in pkg['authors'])):
            raise ValueError(f'Upstream package license/version declaration differs: {flat}')
        out = dest / 'cargo' / f'{name}-{version}' / 'upstream' / item['path']
        copy_notice(source, out)
        outputs.append({'path': str(out.relative_to(dest)), 'sha256': sha(out),
                        'official_url': entry['official_repo'].replace(
                            'https://github.com/', 'https://github.com/') +
                        '/blob/' + entry['upstream_revision'] + '/' + item['path'],
                        'git_blob_sha1': item['git_blob_sha1']})
    fallback = entry.get('canonical_fallback')
    if fallback:
        source = stage / 'packaging/license-evidence' / fallback['path']
        if sha(source) != fallback['sha256']:
            raise ValueError(f'Canonical license evidence changed: {name} {version}')
        out = dest / 'cargo' / f'{name}-{version}' / 'canonical' / fallback['path']
        copy_notice(source, out)
        outputs.append({'path': str(out.relative_to(dest)), 'sha256': sha(out),
                        'official_url': fallback['official_url']})
        if pkg['license'] == 'MIT':
            attribution = dest / 'cargo' / f'{name}-{version}' / 'ATTRIBUTION.txt'
            attribution.write_text(
                f'{name} {version}\n'
                f'License declared by the published package: MIT\n'
                'Author listed in package metadata: ' + ', '.join(pkg['authors']) + '\n'
                f'Official source: {entry["official_repo"]}\n'
                'The published package and matching upstream revision provide no '
                'separate copyright statement or license file. The accompanying '
                'MIT permission terms retain the standard permission, condition '
                'and disclaimer without inventing a copyright holder or year.\n')
            outputs.append({'path': str(attribution.relative_to(dest)),
                            'sha256': sha(attribution)})
    if not entry['upstream_license_files'] and not fallback:
        raise ValueError(f'No actual license text for {name} {version}')
    provenance = {
        'package': f'{name} {version}', 'registry_package_sha256': checksum,
        'license_expression_from_package': pkg['license'],
        'authors_from_package': pkg['authors'],
        'official_upstream': entry['official_repo'],
        'upstream_revision': entry['upstream_revision'],
        'revision_evidence': entry['revision_evidence'],
        'notice_basis': ('standard license terms and labeled package authors; upstream revision '
                         'has no standalone license file or copyright-year statement'
                         if fallback else 'exact license files at official upstream revision'),
    }
    note = dest / 'cargo' / f'{name}-{version}' / 'PROVENANCE.json'
    note.write_text(json.dumps(provenance, indent=2) + '\n')
    outputs.append({'path': str(note.relative_to(dest)), 'sha256': sha(note)})
    return outputs, ('canonical-text-plus-Cargo-authors' if fallback else 'exact-upstream-revision')


def main(args):
    stage = args.source.resolve(strict=True)
    dest = args.output.expanduser().absolute()
    if dest.exists():
        raise ValueError(f'Output exists: {dest}')
    dest.mkdir(parents=True)
    target = json.loads(args.metadata.read_text())
    graph = {node['id'] for node in target['resolve']['nodes']}
    evidence_data = json.loads((stage / 'packaging/rust-license-evidence.json').read_text())
    evidence = {(item['name'], item['version']): item for item in evidence_data['packages']}
    crates = []
    absent_roots = []
    canonical_fallbacks = []
    mit_without_published_copyright_statement = []
    for pkg in sorted(target['packages'], key=lambda p: (p['name'], p['version'])):
        if pkg['id'] not in graph:
            continue
        name, version = pkg['name'], pkg['version']
        if not pkg['source']:
            local_notices = []
            for rel in ('orbiter/LICENSE', 'packaging/LICENSE'):
                src = stage / rel
                out = dest / 'cargo' / f'{name}-{version}' / rel.replace('/', '-')
                copy_notice(src, out)
                local_notices.append({'path': str(out.relative_to(dest)), 'sha256': sha(out)})
            crates.append({'name': name, 'version': version,
                           'license_expression': pkg['license'], 'kind': 'local source',
                           'notice_status': 'local-repository-texts', 'notice_files': local_notices})
            continue
        source = stage / 'vendor/cargo' / f'{name}-{version}'
        checksum = json.loads((source / '.cargo-checksum.json').read_text())['package']
        root_notices = sorted(p for p in source.iterdir() if p.is_file() and
                              p.name.lower().startswith(('license', 'notice', 'copying', 'copyright')))
        outputs = []
        for path in root_notices:
            out = dest / 'cargo' / f'{name}-{version}' / path.name
            copy_notice(path, out)
            outputs.append({'path': str(out.relative_to(dest)), 'sha256': sha(out)})
        status = 'packaged-root'
        if not outputs:
            absent_roots.append(f'{name} {version} ({pkg["license"]})')
            outputs, status = upstream_evidence(stage, dest, pkg, checksum, evidence)
            if status == 'canonical-text-plus-Cargo-authors':
                canonical_fallbacks.append(f'{name} {version}')
                if pkg['license'] == 'MIT':
                    # Preserve the published MIT declaration and labeled authors,
                    # without presenting either as an invented copyright notice.
                    mit_without_published_copyright_statement.append(f'{name} {version}')
        crates.append({'name': name, 'version': version,
                       'license_expression': pkg['license'], 'kind': 'registry',
                       'registry_package_sha256': checksum, 'notice_status': status,
                       'authors_from_package': pkg['authors'], 'notice_files': outputs})
    pinned = json.loads((stage / 'ci/inputs.json').read_text())['dependencies']
    native = []
    for key, name in NAMES.items():
        source = stage / 'vendor/cmake' / name
        root_notices = sorted(p for p in source.iterdir() if p.is_file() and
                              p.name.lower().startswith(('license', 'notice', 'copying', 'copyright')))
        outputs = []
        for path in root_notices:
            out = dest / 'cmake' / name / path.name
            copy_notice(path, out)
            outputs.append({'name': path.name, 'sha256': sha(out)})
        if name == 'lua':
            header = (source / 'lua.h').read_text()
            start = header.rfind('/******************************************************************************')
            if start < 0 or 'Copyright (C) 1994-2006 Lua.org, PUC-Rio.' not in header[start:]:
                raise ValueError('Lua permission and warranty notice missing')
            out = dest / 'cmake/lua/lua-header-license.txt'
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_text(header[start:header.index('*/', start) + 2] + '\n')
            outputs.append({'name': out.name, 'sha256': sha(out)})
        if not outputs:
            raise ValueError(f'{name} pinned C/C++ dependency has no notice')
        native.append({'name': key, 'commit': pinned[key]['commit'], 'notice_files': outputs})
    fonts = []
    for name in ('LICENSE-APACHE.txt', 'LICENSE-OFL.txt'):
        src = stage / 'orbiter/Extern/NativeFonts' / name
        out = dest / 'fonts' / name
        copy_notice(src, out)
        fonts.append({'name': name, 'sha256': sha(out)})
    sbom = {'schema': 1, 'target': 'aarch64-apple-darwin',
            'cargo_resolved_packages': len(crates), 'cargo_packages': crates,
            'native_dependencies': native, 'native_font_licenses': fonts,
            'packaged_root_license_absent': absent_roots,
            'canonical_text_author_attribution_caveats': canonical_fallbacks,
            'mit_without_published_copyright_statement': mit_without_published_copyright_statement,
            'missing_notice_texts': [],
            'scope': 'technical inventory; source modifications and rights still need release review'}
    (dest / 'SBOM.json').write_text(json.dumps(sbom, indent=2) + '\n')
    print(json.dumps({'target_packages': len(crates), 'native_dependencies': len(native),
                      'recovered_missing_packaged_notices': len(absent_roots),
                      'canonical_fallbacks': len(canonical_fallbacks),
                      'mit_without_published_copyright_statement': mit_without_published_copyright_statement,
                      'missing_notice_texts': 0, 'output': str(dest)}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--metadata', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    main(parser.parse_args())
