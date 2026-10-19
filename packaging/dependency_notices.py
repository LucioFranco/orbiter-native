#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Inventory Mac target dependencies and copy exact included notice texts.

Reads Cargo's aarch64-apple-darwin resolved graph, not every cross-platform
entry in Cargo.lock. Any package lacking a root notice is reported explicitly.
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


def main(args):
    stage = args.source.resolve(strict=True)
    dest = args.output.expanduser().absolute()
    if dest.exists():
        raise ValueError(f'Output exists: {dest}')
    dest.mkdir(parents=True)
    target = json.loads(args.metadata.read_text())
    graph = {node['id'] for node in target['resolve']['nodes']}
    crates = []
    gaps = []
    for pkg in sorted(target['packages'], key=lambda p: (p['name'], p['version'])):
        if pkg['id'] not in graph:
            continue
        name, version = pkg['name'], pkg['version']
        if not pkg['source']:
            crates.append({'name': name, 'version': version,
                           'license_expression': pkg['license'], 'kind': 'local source'})
            continue
        source = stage / 'vendor/cargo' / f'{name}-{version}'
        checksum = json.loads((source / '.cargo-checksum.json').read_text())['package']
        root_notices = sorted(p for p in source.iterdir() if p.is_file() and
                              p.name.lower().startswith(('license', 'notice', 'copying', 'copyright')))
        outputs = []
        for path in root_notices:
            out = dest / 'cargo' / f'{name}-{version}' / path.name
            copy_notice(path, out)
            outputs.append({'name': path.name, 'sha256': sha(out)})
        if not outputs:
            gaps.append(f'{name} {version} ({pkg["license"]})')
        crates.append({'name': name, 'version': version,
                       'license_expression': pkg['license'], 'kind': 'registry',
                       'registry_package_sha256': checksum, 'notice_files': outputs})
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
            gaps.append(f'{name} pinned C/C++ dependency has no root notice')
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
            'root_license_file_gaps': gaps,
            'scope': 'technical inventory; review source headers and nested notices before release'}
    (dest / 'SBOM.json').write_text(json.dumps(sbom, indent=2) + '\n')
    print(json.dumps({'target_packages': len(crates), 'native_dependencies': len(native),
                      'root_license_file_gaps': len(gaps), 'output': str(dest)}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--metadata', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    main(parser.parse_args())
