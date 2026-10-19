#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Assemble and audit a *local-only* relocatable macOS v9 preview directory.

This intentionally has no archive, upload or release operation. NASSP bitmap
bytes and original mission data are generated or staged from user inputs later.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
CONTRACT = json.loads((HERE / "preview-contract.json").read_text())
LEDGER = json.loads((HERE / "v9-input-review.json").read_text())
NOTICE_FILES = (
    "LICENSES/GPL-2.0.txt", "LICENSES/GPL-3.0.txt",
    "LICENSES/MIT-original.txt", "orbiter/LICENSE",
    "orbiter/Addons/NASSP/NASSP-LICENSE.txt",
    "orbiter/Extern/NativeFonts/LICENSE-APACHE.txt",
    "orbiter/Extern/NativeFonts/LICENSE-OFL.txt",
)


class PreviewError(Exception):
    pass


def command(*args):
    result = subprocess.run(args, text=True, capture_output=True)
    if result.returncode:
        raise PreviewError(f"{' '.join(map(str, args[:2]))}: {result.stderr.strip()}")
    return result.stdout


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def safe_name(name):
    relative = PurePosixPath(name)
    if relative.is_absolute() or not relative.parts or any(x in (".", "..") for x in relative.parts):
        raise PreviewError(f"Unsafe contract path: {name}")
    return Path(*relative.parts)


def source_inventory(source):
    revision = command("git", "-C", str(source), "rev-parse", "HEAD").strip()
    command("git", "-C", str(source), "merge-base", "--is-ancestor",
            "45ffa402d84364fb489c60ba8e2c6ec4d9576f58", revision)
    dirty = bool(command("git", "-C", str(source), "status", "--porcelain",
                         "--untracked-files=no").strip())
    failures = []
    for entry in LEDGER["files"]:
        path = source / safe_name(entry["source"])
        if not path.is_file():
            failures.append(entry["source"])
            continue
        data = path.read_bytes()
        identities = {hashlib.sha256(data).hexdigest()}
        # Git's checkout attributes may restore the six NASSP RC/header tables
        # as CRLF. Their ledger records the same LF-normalized source content.
        if entry["role"] == "Resource table/header; required source input":
            identities.add(hashlib.sha256(data.replace(b"\r\n", b"\n")).hexdigest())
        if entry["sha256"] not in identities:
            failures.append(entry["source"])
    if failures:
        raise PreviewError(f"Source inventory mismatch ({len(failures)}): {failures[:5]}")
    return {"revision": revision, "tracked_worktree_dirty_at_assembly": dirty,
            "reviewed_input_count": len(LEDGER["files"]),
            "reviewed_input_ledger_sha256": sha256(HERE / "v9-input-review.json"),
            "status": "inventory only; complete corresponding source remains unverified"}


def macho_rpaths(path):
    data = command("otool", "-l", str(path))
    return re.findall(r"cmd LC_RPATH\n.*?path (.*?) \(offset", data, re.S)


def macho_loads(path):
    data = command("otool", "-L", str(path)).splitlines()
    names = [line.strip().split(" (compatibility")[0] for line in data[1:]]
    # The first install name on a dylib is its identity, not a dependency.
    own = command("otool", "-D", str(path)).splitlines()
    identity = own[1].strip() if len(own) > 1 else None
    if identity in names:
        names.remove(identity)
    return names


def macho_minos(path):
    data = command("otool", "-l", str(path))
    versions = re.findall(r"cmd LC_BUILD_VERSION\n.*?minos (\d+\.\d+)", data, re.S)
    if not versions:
        raise PreviewError(f"Missing macOS build version: {path}")
    return max(versions, key=lambda value: tuple(map(int, value.split("."))))


def binaries(build):
    found = []
    for name in CONTRACT["binary_candidates"]:
        relative = safe_name(name)
        path = build / relative
        if not path.is_file() or path.is_symlink():
            raise PreviewError(f"Missing or linked candidate binary: {path}")
        architectures = command("lipo", "-archs", str(path)).split()
        if architectures != ["arm64"]:
            raise PreviewError(f"Expected arm64 only, got {architectures}: {path}")
        minimum = macho_minos(path)
        if tuple(map(int, minimum.split("."))) > tuple(map(int, CONTRACT["minimum_macos"].split("."))):
            raise PreviewError(f"{path} requires macOS {minimum}, above contract {CONTRACT['minimum_macos']}")
        found.append(relative)
    return found


def rewrite_rpaths(root, paths, old_build):
    old_build = old_build.resolve()
    for relative in paths:
        file = root / relative
        for old in macho_rpaths(file):
            old_path = Path(old)
            if not old_path.is_absolute() or not old_path.is_relative_to(old_build):
                raise PreviewError(f"Unexpected build runpath {old} in {relative}")
            target = root / old_path.relative_to(old_build)
            rel = os.path.relpath(target, file.parent)
            new = "@loader_path" if rel == "." else "@loader_path/" + rel
            command("install_name_tool", "-rpath", old, new, str(file))
        # Rpath mutation invalidates any former signature. Ad-hoc signing is
        # only for local execution; it is neither Developer ID nor notarization.
        command("codesign", "--force", "--sign", "-", str(file))


def resolve_loader(value, file, root):
    if value == "@loader_path":
        target = file.parent
    elif value.startswith("@loader_path/"):
        target = file.parent / value[len("@loader_path/"):]
    elif value == "@executable_path":
        target = root
    elif value.startswith("@executable_path/"):
        target = root / value[len("@executable_path/"):]
    else:
        raise PreviewError(f"Unsupported runpath {value} in {file}")
    target = target.resolve()
    if not target.is_relative_to(root):
        raise PreviewError(f"Runpath escapes preview: {value} in {file}")
    return target


def audit_closure(root, paths):
    root = root.resolve()
    results = {}
    for relative in paths:
        file = root / relative
        runpaths = macho_rpaths(file)
        for value in runpaths:
            resolve_loader(value, file, root)
        dependencies = []
        for dep in macho_loads(file):
            if dep.startswith(("/usr/lib/", "/System/Library/")):
                dependencies.append({"name": dep, "kind": "system"})
                continue
            if not dep.startswith("@rpath/"):
                raise PreviewError(f"Non-system dependency {dep} in {relative}")
            candidates = [resolve_loader(rpath, file, root) / dep[len("@rpath/"):]
                          for rpath in runpaths]
            matches = [candidate for candidate in candidates if candidate.is_file()]
            if len(matches) != 1:
                raise PreviewError(f"Unresolved/ambiguous {dep} in {relative}: {matches}")
            dependencies.append({"name": dep, "kind": "preview",
                                 "target": str(matches[0].relative_to(root))})
        results[str(relative)] = {"rpaths": runpaths, "dependencies": dependencies,
                                  "minimum_macos": macho_minos(file)}
    return results


def copy_regular(source, target):
    if not source.is_file() or source.is_symlink():
        raise PreviewError(f"Refusing missing/linked source file: {source}")
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def assemble(args):
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        raise PreviewError("Assembly needs an Apple Silicon Mac with Mach-O tools")
    build = args.build.resolve(strict=True)
    source = args.source_root.resolve(strict=True)
    requested = args.output.expanduser().absolute()
    output = requested.parent.resolve(strict=True) / requested.name
    if output.exists() or output.is_symlink():
        raise PreviewError("Use a fresh nonexistent output directory")
    if output.is_relative_to(build) or output.is_relative_to(source):
        raise PreviewError("Output must be outside the build and source trees")
    source_info = source_inventory(source)
    paths = binaries(build)
    cache = build / "CMakeCache.txt"
    if not cache.is_file():
        raise PreviewError("Expected a v9 CMake build cache")
    settings = dict(line.split("=", 1) for line in cache.read_text().splitlines()
                    if not line.startswith(("//", "#")) and "=" in line)
    if (settings.get("NASSP_DATA_PROFILE:STRING") != "v9" or
            settings.get("ORBITER_BUILD_NASSP:BOOL") not in ("ON", "TRUE", "1") or
            settings.get("ORBITER_BUILD_WGPU:BOOL") not in ("ON", "TRUE", "1") or
            Path(settings.get("CMAKE_HOME_DIRECTORY:INTERNAL", "/missing")).resolve() != source / "orbiter"):
        raise PreviewError("Expected a v9 CMake build cache")
    output.mkdir(parents=True)
    for relative in paths:
        copy_regular(build / relative, output / relative)
    for name in NOTICE_FILES:
        copy_regular(source / safe_name(name), output / "notices" / safe_name(name))
    copy_regular(HERE / "run_preview.py", output / "run-preview.py")
    copy_regular(HERE / "v9-input-review.json", output / "v9-input-review.json")
    source_plan = {
        "status": "preparation only; complete corresponding source not assembled or cleared",
        "application_source_revision": source_info["revision"],
        "reviewed_source_and_resource_entries": len(LEDGER["files"]),
        "pinned_upstream": {"openorbiter": CONTRACT["openorbiter"], "nassp": CONTRACT["nassp"]},
        "build_recipe": "orbiter/CMakePresets.json native-v9 and docs/native-reproduction.md",
        "lockfiles_sha256": {"nix/flake.lock": sha256(source / "orbiter/nix/flake.lock"),
                            "Cargo.lock": sha256(source / "orbiter/Src/WgpuClient/rust/Cargo.lock")},
        "unresolved": ["Exact six dependency source snapshots/local patches and Cargo vendor graph",
                       "Full per-component and per-crate notices and resolved target SBOM",
                       "Preferred source/redistribution review for included fonts and compiled code",
                       "Clean offline rebuild from the proposed source archive"],
    }
    (output / "source-plan.json").write_text(json.dumps(source_plan, indent=2) + "\n")
    (output / "DO-NOT-DISTRIBUTE.txt").write_text(
        "Local technical validation only. NASSP artwork and original mission assets are external. "
        "Complete matching source, target notices, and release rights remain under review. "
        "No archive or upload is authorized.\n")
    rewrite_rpaths(output, paths, build)
    closure = audit_closure(output, paths)
    all_files = sorted(p for p in output.rglob("*") if p.is_file())
    hashes = {str(p.relative_to(output)): sha256(p) for p in all_files}
    minima = sorted({item["minimum_macos"] for item in closure.values()},
                    key=lambda value: tuple(map(int, value.split("."))))
    if tuple(map(int, minima[-1].split("."))) > tuple(map(int, CONTRACT["minimum_macos"].split("."))):
        raise PreviewError(f"Built binaries require macOS {minima[-1]}; contract claims {CONTRACT['minimum_macos']}")
    manifest = {
        "schema": 1, "local_validation_only": True, "release_ready": False,
        "target": "macos-arm64", "contract_minimum_macos": CONTRACT["minimum_macos"],
        "actual_minimum_macos": minima[-1], "source": source_info,
        "source_contract_sha256": sha256(HERE / "preview-contract.json"),
        "binary_candidates": len(paths), "bundled_nassp_bitmaps": 0,
        "first_run_external_bitmaps": 397,
        "files_sha256": hashes, "macho_closure": closure,
        "distribution_blockers": CONTRACT["blocked_on"] + [
            "Build binary/source revision is not cryptographically attested by the CMake cache; require a clean matching-source rebuild",
        ],
    }
    if source_info["tracked_worktree_dirty_at_assembly"]:
        manifest["distribution_blockers"].append("Tracked source worktree was dirty at assembly")
    (output / "preview-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({"output": str(output), "binary_candidates": len(paths),
                      "bundled_nassp_bitmaps": 0,
                      "actual_minimum_macos": minima[-1], "release_ready": False}, indent=2))


def verify(args):
    root = args.output.resolve(strict=True)
    manifest = json.loads((root / "preview-manifest.json").read_text())
    if not manifest.get("local_validation_only") or manifest.get("release_ready"):
        raise PreviewError("Unexpected preview manifest release state")
    for name, expected in manifest["files_sha256"].items():
        file = root / safe_name(name)
        if not file.is_file() or file.is_symlink() or sha256(file) != expected:
            raise PreviewError(f"Preview file mismatch: {name}")
    allowed = set(manifest["files_sha256"]) | {"preview-manifest.json"}
    actual = {str(path.relative_to(root)) for path in root.rglob("*") if path.is_file() or path.is_symlink()}
    if actual != allowed:
        raise PreviewError(f"Unexpected preview files: {sorted(actual ^ allowed)[:5]}")
    paths = [safe_name(name) for name in CONTRACT["binary_candidates"]]
    closure = audit_closure(root, paths)
    if closure != manifest["macho_closure"]:
        raise PreviewError("Mach-O closure differs from assembly manifest")
    print(json.dumps({"verified_files": len(manifest["files_sha256"]),
                      "verified_binaries": len(paths), "relocated_root": str(root)}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    new = sub.add_parser("assemble", help="Copy for local validation; never archive")
    new.add_argument("--build", required=True, type=Path)
    new.add_argument("--source-root", required=True, type=Path)
    new.add_argument("--output", required=True, type=Path)
    check = sub.add_parser("verify", help="Hash and Mach-O closure audit after relocation")
    check.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        (assemble if args.action == "assemble" else verify)(args)
    except (OSError, PreviewError, subprocess.SubprocessError) as error:
        parser.exit(2, f"macOS preview: {error}\n")


if __name__ == "__main__":
    main()
