#!/usr/bin/env python3
"""Check and stage original Apollo demos with explicitly supplied external data."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PROFILES = ('baseline', 'candidate', 'v9')
SOURCE_STACKS = {'baseline': 'nassp', 'candidate': 'nassp-candidate', 'v9': 'nassp-v9'}
MISSIONS = {
    'insertion': 'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 02 - Insertion.scn',
    'liftoff': 'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 11 - Lunar Liftoff T-1m.scn',
}

class DataError(ValueError):
    pass

def sha256(data):
    return hashlib.sha256(data).hexdigest()

def profile_manifest(profile):
    path = ROOT / 'upstream' / ('nassp-data-' + profile + '.json')
    return json.loads(path.read_text()), sha256(path.read_bytes())

def detect_profile(data_root):
    path = data_root / 'Config/ProjectApollo/LEMSystems.cfg'
    if not path.is_file():
        raise DataError('Missing ' + str(path) + '. Supply --nassp-data-root with the original NASSP Config, Scenarios, Missions, Doc, Meshes, Textures and Sound directories.')
    # LEMSystems is shared by the candidate and v9 release. Verify an original
    # scenario too so automatic selection cannot silently choose old data.
    signatures = ('Config/ProjectApollo/LEMSystems.cfg',
                  'Scenarios/Project Apollo - NASSP/' + MISSIONS['insertion'])
    for profile in PROFILES:
        manifest, _ = profile_manifest(profile)
        if all((data_root / name).is_file() and
               sha256((data_root / name).read_bytes().replace(b'\r\n', b'\n')) ==
               manifest['critical'][name].get('lf_sha256', manifest['critical'][name]['sha256'])
               for name in signatures):
            return profile
    raise DataError('NASSP configuration/scenario signatures do not match a pinned revision. Select matching upstream data; source and original mission data must use the same revision.')

def verify_data(data_root, profile='auto', full_assets=False):
    data_root = Path(data_root).resolve()
    if not data_root.is_dir():
        raise DataError('NASSP data root is missing: ' + str(data_root) + '. No data is downloaded automatically; supply --nassp-data-root from the documented pinned upstream revision.')
    if profile == 'auto':
        profile = detect_profile(data_root)
    manifest, manifest_sha = profile_manifest(profile)
    missing = [name for name in manifest['critical'] if not (data_root / name).is_file()]
    missing += [name for name in manifest['assets'] if not (data_root / name).is_file()]
    if missing:
        raise DataError('Incomplete NASSP data: missing ' + missing[0] + ' (' + str(len(missing)) + ' required files missing). The source checkout omits scenarios/textures; provide the complete external data directories for ' + manifest['commit'] + '.')
    line_endings = []
    for name, entry in manifest['critical'].items():
        data = (data_root / name).read_bytes()
        if sha256(data) != entry['sha256']:
            if 'lf_sha256' not in entry or sha256(data.replace(b'\r\n', b'\n')) != entry['lf_sha256']:
                raise DataError('NASSP data hash mismatch: ' + name + '. Expected ' + profile + ' commit ' + manifest['commit'] + '; do not combine source and data from different revisions.')
            line_endings.append(name)
    if full_assets:
        for name, expected in manifest['assets'].items():
            data = (data_root / name).read_bytes()
            actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
            if actual != expected:
                raise DataError('Upstream asset blob mismatch: ' + name + '. Expected Git blob ' + expected + ' from ' + manifest['commit'] + '.')
    return {'profile': profile, 'upstream_commit': manifest['commit'], 'upstream_repository': manifest['repository'],
            'data_root': str(data_root), 'manifest_sha256': manifest_sha, 'critical_hashes_verified': len(manifest['critical']),
            'asset_files_present': len(manifest['assets']), 'all_asset_blobs_verified': full_assets,
            'text_line_endings_converted': line_endings}

def verify_sources(source, profile):
    source = Path(source).resolve()
    stack = SOURCE_STACKS[profile]
    manifest = json.loads((ROOT / 'upstream' / stack / 'manifest.json').read_text())
    for entry in manifest['files']:
        path = source / entry['path']
        if not path.is_file() or sha256(path.read_bytes().replace(b'\r\n', b'\n')) != entry['patched_sha256']:
            raise DataError('Adapted NASSP source mismatch: ' + str(path) + '. Use the ' + profile + ' patch stack with its matching data; source and data roots are separate.')
    exported = ROOT / 'upstream/nassp-manifests' / profile
    inventory = json.loads((exported / 'manifest.json').read_text())
    expected_commit = profile_manifest(profile)[0]['commit']
    if inventory['commit'] != expected_commit:
        raise DataError('Exported source lists do not match the selected upstream revision.')
    for name, entry in inventory['files'].items():
        path = exported / name
        if not path.is_file() or sha256(path.read_bytes().replace(b'\r\n', b'\n')) != entry['lf_sha256']:
            raise DataError('Exported upstream source-list hash mismatch: ' + str(path))
        if profile == 'v9':
            # Native targets read CMake directly from the adapted addon. Verify
            # those unpatched lists too; ignored VC2017 lists may use exports.
            actual = source / 'Orbitersdk/samples/ProjectApollo' / name
            if name.endswith('CMakeLists.txt') or actual.exists():
                if not actual.is_file() or sha256(actual.read_bytes().replace(b'\r\n', b'\n')) != entry['lf_sha256']:
                    raise DataError('Adapted NASSP source-list mismatch: ' + str(actual))
    return {'source_root': str(source), 'source_adaptations_verified': len(manifest['files']),
            'exported_source_lists_verified': len(inventory['files'])}

def verify_textures(path):
    if path is None:
        return {'orbiter_textures': None, 'planetary_package_supplied': False}
    path = Path(path).resolve()
    required = ('Earth/Archive/Surf.tree', 'Earth/Archive/Elev.tree', 'Moon/Archive/Surf.tree', 'Moon/Archive/Elev.tree')
    for name in required:
        file = path / name
        if not file.is_file() or file.stat().st_size == 0:
            raise DataError('Missing planetary texture/elevation archive: ' + str(file) + '. Pass --orbiter-textures pointing to the extracted Orbiter 2024 Textures directory, not its parent.')
    # Archive presence is checked here; no claim of release-package checksum
    # verification is made for arbitrary user-provided planetary packages.
    return {'orbiter_textures': str(path), 'planetary_package_supplied': True,
            'planetary_archive_sizes': {name: (path / name).stat().st_size for name in required},
            'planetary_archive_hashes_verified': False}

def cmake_cache(build):
    file = build / 'CMakeCache.txt'
    if not file.is_file():
        raise DataError('Build is not configured: ' + str(build) + '. Run cmake --preset native-poc -DNASSP_DATA_DIR=<data-root> and build it first.')
    result = {}
    for line in file.read_text().splitlines():
        if line and not line.startswith(('#', '//')) and '=' in line and ':' in line.split('=', 1)[0]:
            name, value = line.split('=', 1)
            result[name.split(':', 1)[0]] = value
    if Path(result.get('CMAKE_HOME_DIRECTORY', '')).resolve() != ROOT:
        raise DataError('This build belongs to another engine checkout. Configure a fresh build under the current checkout instead of reusing an old CMake cache.')
    return result

def verify_build(build):
    exe = build / ('orbiter.exe' if sys.platform == 'win32' else 'orbiter')
    if not exe.is_file():
        raise DataError('Native executable is missing: ' + str(exe) + '. Run cmake --build --preset native-poc -j4.')
    modules = build / 'runtime/Modules'
    names = ('Saturn5', 'LEM', 'Sat5LMDSC', 'MCC', 'SII', 'SIVB', 'Floodlight', 'Flag', 'EasepPSEP', 'EasepLR3', 'EasepSWC')
    for folder, name in ([('ProjectApollo', name) for name in names] + [('Plugin', 'ProjectApolloMFD'), ('Plugin', 'WgpuClient')] + [('Celbody', body) for body in ('Sun', 'Earth', 'Moon')]):
        if not any((modules / folder / (name + ext)).is_file() for ext in ('.dylib', '.so', '.dll')):
            raise DataError('Missing mission module ' + name + '. Build the complete native preset; standalone vessel targets are insufficient for original missions.')
    if not (build / 'runtime/Resources/NASSP/csm_main_panel.bmp').is_file():
        raise DataError('Native bitmap resources are missing. Build nassp_bitmap_resources before staging.')
    return exe

def display(path):
    try:
        return str(Path(path).relative_to(ROOT))
    except ValueError:
        return str(path)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('check-data', 'check', 'stage'))
    parser.add_argument('--nassp-data-root', type=Path, required=True)
    parser.add_argument('--profile', choices=('auto', *PROFILES), default='auto')
    parser.add_argument('--source-addon', type=Path)
    parser.add_argument('--orbiter-textures', type=Path)
    parser.add_argument('--verify-all-assets', action='store_true')
    parser.add_argument('--build', type=Path, default=Path('build/native-poc'))
    parser.add_argument('--mission', choices=MISSIONS, default='insertion')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--headless', action='store_true', help='Print a load-only headless command instead of the interactive command')
    args = parser.parse_args()
    try:
        report = verify_data(args.nassp_data_root, args.profile, args.verify_all_assets)
        report.update(verify_textures(args.orbiter_textures))
        if args.action == 'check-data':
            if args.source_addon:
                report.update(verify_sources(args.source_addon, report['profile']))
            print(json.dumps(report, indent=2)); return
        build = args.build.resolve()
        cache = cmake_cache(build)
        source = Path(cache.get('NASSP_ADDON_DIR', str(ROOT / 'Addons/NASSP'))).resolve()
        if args.source_addon and args.source_addon.resolve() != source:
            raise DataError('--source-addon differs from the configured NASSP_ADDON_DIR. Check the source actually compiled into this build.')
        report.update(verify_sources(source, report['profile']))
        configured_data = Path(cache.get('NASSP_DATA_DIR', cache.get('NASSP_ADDON_DIR', ''))).resolve()
        if configured_data != args.nassp_data_root.resolve():
            raise DataError('Build NASSP_DATA_DIR differs from the supplied root. Reconfigure it so compiled test gates and demo staging select the same revision.')
        configured_textures = cache.get('ORBITER_TEXTURE_DIR', '')
        if configured_textures and (args.orbiter_textures is None or Path(configured_textures).resolve() != args.orbiter_textures.resolve()):
            raise DataError('Pass the same --orbiter-textures root configured as ORBITER_TEXTURE_DIR.')
        exe = verify_build(build)
        out = args.output.resolve() if args.output else build / ('runtime-original-' + args.mission)
        for protected in (ROOT, args.nassp_data_root.resolve(), source.resolve(), build):
            if protected == out or protected.is_relative_to(out):
                raise DataError('Runtime output must be a separate directory, not a source/data/build root or its ancestor.')
        if out.is_relative_to(args.nassp_data_root.resolve()) or out.is_relative_to(source):
            raise DataError('Runtime output must not be inside a NASSP data or source directory.')
        if out.is_relative_to(ROOT) and not out.is_relative_to(build):
            raise DataError('Runtime output inside the engine checkout must be under the selected build directory.')
        scenario = MISSIONS[args.mission]
        command = [display(exe), '--root', display(out), '--scenario', scenario[:-4]]
        if args.headless:
            command += ['--headless', '--steps', '0']
        else:
            command += ['--graphics', 'WgpuClient', '--module', 'ProjectApolloMFD', '--window', '--vc', '--laptop-controls']
        command += ['--save', 'MyOriginalInsertion' if args.mission == 'insertion' else 'MyOriginalLiftoff']
        report.update(runtime=str(out), mission=args.mission, launch_argv=command, launch_command=shlex.join(command))
        if args.action == 'stage':
            stage = [sys.executable, str(ROOT / 'tools/stage_nassp_mission.py'), str(build), '--addon', str(args.nassp_data_root.resolve()), '--profile', report['profile'], '--scenario', scenario, '--output', str(out)]
            if args.orbiter_textures:
                stage += ['--textures', str(args.orbiter_textures.resolve())]
            subprocess.run(stage, check=True, stdout=subprocess.DEVNULL)
            actual = json.loads((out / 'mission-source.json').read_text())
            report['runtime_provenance'] = actual['data_provenance']['runtime_provenance']
            (out / 'demo-preflight.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
    except (DataError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(2, 'NASSP demo preflight: ' + str(error) + '\n')

if __name__ == '__main__':
    main()
