#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Stage an offline-buildable, executable-only Mac preview source bundle.

This is a local review artifact, never an upload/release command. Original
NASSP artwork, Orbiter planetary archives and Architext are intentionally
external; native panel bitmaps are read from a verified user-acquired checkout.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parent.parent
EXT = {'.c', '.cc', '.cpp', '.h', '.hpp', '.hxx', '.m', '.mm', '.rs', '.rc',
       '.cmake', '.in', '.py', '.json', '.toml', '.lock', '.nix', '.md',
       '.txt', '.vcxproj', '.filters', '.props', '.s', '.asm', '.def',
       '.wgsl', '.glsl', '.inl'}
TOP = {'CMakeLists.txt', 'CMakePresets.json', 'flake.nix', 'flake.lock',
       'LICENSE', 'README', 'Makefile', 'Cargo.lock', 'Cargo.toml'}
NATIVE_FONTS = {'Roboto-Regular.ttf', 'Roboto-Bold.ttf', 'Roboto-Italic.ttf',
                'Roboto-BoldItalic.ttf', 'JetBrainsMono-Regular.ttf',
                'JetBrainsMono-Bold.ttf', 'JetBrainsMono-Italic.ttf',
                'JetBrainsMono-BoldItalic.ttf'}
NAMES = {'SDL3': 'SDL', 'IMGUI': 'imgui', 'STB': 'stb',
         'MINIAUDIO': 'miniaudio', 'LUA': 'lua', 'CORROSION': 'corrosion'}


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def command(*args, cwd=None):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def selected(path):
    p = path.parts
    if path.as_posix() in {'LICENSES/GPL-2.0.txt', 'LICENSES/GPL-3.0.txt',
                           'LICENSES/MIT-original.txt', 'orbiter/LICENSE',
                           'orbiter/Addons/NASSP/NASSP-LICENSE.txt'}:
        return True
    if p[0] in {'packaging', 'ci'}:
        return path.suffix.lower() in EXT or path.name in TOP
    if p[0] != 'orbiter':
        return False
    if len(p) > 2 and p[1:3] == ('Extern', 'NativeFonts'):
        return path.name in NATIVE_FONTS or path.suffix.lower() in {'.txt', '.md'}
    if path.suffix == '.ttf':
        return False
    if p[1] in {'Src', 'Addons', 'Orbitersdk', 'Tests', 'Extern',
                'cmake', 'tools', 'nix', 'upstream', 'docs'}:
        return path.suffix.lower() in EXT or path.name in TOP
    return len(p) == 2 and (path.name in TOP or path.suffix.lower() in EXT)


def stage(args):
    dest = args.output.expanduser().absolute()
    if dest.exists():
        raise ValueError(f'Output already exists: {dest}')
    dest.mkdir(parents=True)
    names = command('git', 'ls-files', '-z', cwd=ROOT).split('\0')
    names = [Path(n) for n in names if n and selected(Path(n))]
    # Include these new build inputs even before a follow-up commit is made.
    for name in ('orbiter/tools/check_nassp_bitmap_inputs.py',
                 'packaging/v9-bitmap-map.json'):
        p = Path(name)
        if p not in names:
            names.append(p)
    for rel in sorted(names):
        src = ROOT / rel
        if not src.is_file() or src.is_symlink():
            raise ValueError(f'Invalid source input: {src}')
        target = dest / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, target)
    pinned = json.loads((ROOT / 'ci/inputs.json').read_text())['dependencies']
    for key, name in NAMES.items():
        source = args.dependencies_root / name
        if command('git', 'rev-parse', 'HEAD', cwd=source) != pinned[key]['commit']:
            raise ValueError(f'{name} pin mismatch')
        if command('git', 'status', '--porcelain', cwd=source):
            raise ValueError(f'{name} source is dirty')
        output = dest / 'vendor/cmake' / name
        output.mkdir(parents=True)
        # git archive excludes .git, build products, and unrelated ignored files.
        subprocess.run(['git', 'archive', '--format=tar', 'HEAD', '-o', str(dest / f'{name}.tar')],
                       cwd=source, check=True)
        with tarfile.open(dest / f'{name}.tar') as stream:
            for member in stream.getmembers():
                target = (output / member.name).resolve()
                if not target.is_relative_to(output.resolve()) or not (member.isfile() or member.isdir()):
                    raise ValueError(f'Unsafe archived source path: {member.name}')
                stream.extract(member, output)
        (dest / f'{name}.tar').unlink()
    if not args.cargo_vendor.is_dir():
        raise ValueError('Use a completed cargo vendor --locked --versioned-dirs directory')
    shutil.copytree(args.cargo_vendor, dest / 'vendor/cargo', symlinks=False)
    config = dest / 'orbiter/Src/WgpuClient/rust/.cargo/config.toml'
    config.parent.mkdir(parents=True, exist_ok=True)
    config.write_text('[source.crates-io]\nreplace-with = "vendored-sources"\n\n'
                      '[source.vendored-sources]\n'
                      'directory = "../../../../vendor/cargo"\n')
    inventory = {str(p.relative_to(dest)): sha(p) for p in sorted(dest.rglob('*')) if p.is_file()}
    result = {'schema': 1, 'scope': 'local-experimental-macos-arm64-executable-only',
              'repo_revision': command('git', 'rev-parse', 'HEAD', cwd=ROOT),
              'pinned_cmake_dependencies': {key: data['commit'] for key, data in pinned.items()},
              'files_sha256': inventory, 'external_original_nassp_bitmaps': 397,
              'excluded': ['original NASSP media', 'planetary textures', 'raw Architext font']}
    (dest / 'SOURCE-MANIFEST.json').write_text(json.dumps(result, indent=2) + '\n')
    if args.archive:
        archive = args.archive.expanduser().absolute()
        if archive.exists() or archive.is_relative_to(dest):
            raise ValueError('Archive output must be fresh and outside the source tree')
        with archive.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as gz:
            with tarfile.open(fileobj=gz, mode='w') as stream:
                for path in sorted(p for p in dest.rglob('*') if p.is_file()):
                    info = stream.gettarinfo(str(path),
                        arcname='nassp-preview-source/' + str(path.relative_to(dest)))
                    info.uid = info.gid = 0
                    info.uname = info.gname = ''
                    info.mtime = 0
                    with path.open('rb') as source:
                        stream.addfile(info, source)
    print(json.dumps({'output': str(dest), 'files': len(inventory),
                      'bytes': sum(p.stat().st_size for p in dest.rglob('*') if p.is_file()),
                      'archive': str(args.archive) if args.archive else None}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--cargo-vendor', required=True, type=Path)
    parser.add_argument('--dependencies-root', required=True, type=Path,
                        help='Six clean git checkouts pinned by ci/inputs.json')
    parser.add_argument('--archive', type=Path, help='Optional local .tar.gz, never uploaded')
    stage(parser.parse_args())
