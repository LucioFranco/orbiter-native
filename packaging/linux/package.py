#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build a reviewed Linux artifact with matching source, notices and no original data."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = ROOT / "packaging/linux/clearance.json"
NAME = "orbiter-native-linux-x64"


def run(*command, **kwargs):
    return subprocess.check_output(command, text=True, **kwargs).strip()


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_file(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError("Missing or unsafe required file: " + str(path))
    return path


def check_clearance():
    contract = json.loads(CONTRACT.read_text())
    if contract["schema"] != 1 or contract["release_enabled"] is not True:
        raise ValueError("Linux artifact blocked by packaging/linux/clearance.json: release_enabled is false")
    incomplete = [name for name, value in contract["checks"].items() if value is not True]
    if incomplete:
        raise ValueError("Linux artifact review gates remain open: " + ", ".join(incomplete))


def verify_build(build):
    if sys.platform != "linux" or platform.machine() not in ("x86_64", "AMD64"):
        raise ValueError("Linux x64 artifacts must be staged on Linux x64")
    cache = require_file(build / "CMakeCache.txt").read_text()
    for value in ("ORBITER_PORTABILITY_CHECKS:BOOL=ON", "ORBITER_BUILD_NASSP:BOOL=ON",
                  "ORBITER_BUILD_WGPU:BOOL=ON"):
        if value not in cache:
            raise ValueError("Combined runtime build required: " + value)
    result = json.loads(require_file(build / "test-result.json").read_text())
    plan = json.loads(require_file(build / "test-plan.json").read_text())
    if result.get("status") != "passed" or result.get("suite") != "hosted":
        raise ValueError("Current headless Linux test subset must pass before staging")
    if not {"native_data_preflight", "nassp_assets"}.issubset(plan.get("selected", [])):
        raise ValueError("Linux combined test plan is missing required data checks")
    if "native_wgpu" not in plan.get("excluded", {}):
        raise ValueError("Headless plan must record the excluded GPU probe")
    required = [build / "orbiter", build / "liborbiter.so", build / "Extern/Lua/liblua.so"]
    modules = build / "runtime/Modules"
    resources = build / "runtime/Resources/NASSP"
    for name in ("Plugin/WgpuClient.so", "Plugin/ProjectApolloMFD.so",
                 "ProjectApollo/Saturn5.so", "ProjectApollo/LEM.so",
                 "ProjectApollo/MCC.so", "Celbody/Earth.so"):
        required.append(modules / name)
    for path in required:
        require_file(path)
    if modules.is_symlink() or resources.is_symlink() or not resources.is_dir():
        raise ValueError("Module and NASSP resource trees must be regular directories")
    for tree in (modules, resources):
        if any(item.is_symlink() for item in tree.rglob("*")):
            raise ValueError("Cannot package linked runtime content: " + str(tree))
    return modules, resources


def verify_elf(binary, build):
    dynamic = run("readelf", "-d", str(binary))
    if str(build) in dynamic or "/nix/store/" in dynamic:
        raise ValueError("ELF retains a build or Nix store path: " + str(binary))
    linked = run("ldd", str(binary))
    if "not found" in linked or "/nix/store/" in linked:
        raise ValueError("ELF has unresolved or Nix store dependencies: " + str(binary))


def copy_runtime(build, package, modules, resources):
    for relative in ("orbiter", "liborbiter.so", "Extern/Lua/liblua.so"):
        target = package / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(build / relative, target)
    for source in sorted(modules.rglob("*.so")):
        target = package / "runtime/Modules" / source.relative_to(modules)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    shutil.copytree(resources, package / "runtime/Resources/NASSP")
    for path in [package / "orbiter", package / "liborbiter.so",
                 package / "Extern/Lua/liblua.so", *(package / "runtime/Modules").rglob("*.so")]:
        verify_elf(path, build)


def copy_source(package):
    commit = run("git", "-C", str(ROOT), "rev-parse", "HEAD")
    source = package / "source"
    repository = source / "repository"
    repository.mkdir(parents=True)
    archive = source / "repository.tar"
    with archive.open("wb") as stream:
        subprocess.run(["git", "-C", str(ROOT), "archive", "--format=tar", "HEAD"],
                       stdout=stream, check=True)
    with tarfile.open(archive) as stream:
        stream.extractall(repository, filter="data")
    archive.unlink()
    inputs = json.loads((ROOT / "ci/inputs.json").read_text())
    dependencies = {}
    for name, info in inputs["dependencies"].items():
        checkout = ROOT / ".ci-cache/deps" / name.lower()
        if checkout.is_symlink() or run("git", "-C", str(checkout), "rev-parse", "HEAD") != info["commit"]:
            raise ValueError("Pinned dependency missing or changed: " + name)
        if run("git", "-C", str(checkout), "status", "--porcelain", "--untracked-files=all"):
            raise ValueError("Pinned dependency has local changes: " + name)
        shutil.copytree(checkout, source / "deps" / name, symlinks=True,
                        ignore=shutil.ignore_patterns(".git"))
        dependencies[name] = info
    vendor = source / "cargo-vendor"
    cargo_env = dict(os.environ, CARGO_NET_OFFLINE="true")
    config = run("cargo", "vendor", "--locked", "--manifest-path",
                 str(ROOT / "orbiter/Src/WgpuClient/rust/Cargo.toml"), str(vendor), env=cargo_env)
    (source / "cargo-vendor-config.toml").write_text(config + "\n")
    return commit, dependencies


def copy_notices(package):
    source = package / "source/repository"
    notices = package / "notices"
    selected = ("LICENSE.md", "LICENSES/MIT-original.txt", "LICENSES/GPL-2.0.txt",
                "LICENSES/GPL-3.0.txt", "orbiter/Addons/NASSP/NASSP-LICENSE.txt",
                "orbiter/Extern/NativeFonts/LICENSE-APACHE.txt",
                "orbiter/Extern/NativeFonts/LICENSE-OFL.txt",
                "orbiter/Extern/NativeFonts/README.md")
    for relative in selected:
        target = notices / "repository" / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(require_file(source / relative), target)
    # Preserve original notice files verbatim, along with their complete source.
    # The review gate records whether these texts cover every resolved target.
    for folder in (package / "source/deps", package / "source/cargo-vendor"):
        for path in folder.rglob("*"):
            if not path.is_file() or path.is_symlink():
                continue
            if path.name.upper().startswith(("LICENSE", "LICENCE", "COPYING", "NOTICE")):
                target = notices / path.relative_to(package / "source")
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
    return {str(p.relative_to(package)): sha256(p) for p in sorted(notices.rglob("*")) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    check_clearance()
    build = args.build.resolve(strict=True)
    modules, resources = verify_build(build)
    if run("git", "-C", str(ROOT), "status", "--porcelain", "--untracked-files=all"):
        raise ValueError("Matching source tree must be clean and committed")
    output = args.output.resolve()
    if output.exists() or output.is_symlink():
        raise ValueError("Use a fresh artifact output directory")
    output.mkdir(parents=True)
    with tempfile.TemporaryDirectory(dir=output) as temporary:
        package = Path(temporary) / NAME
        package.mkdir()
        copy_runtime(build, package, modules, resources)
        commit, dependencies = copy_source(package)
        notices = copy_notices(package)
        shutil.copy2(ROOT / "packaging/linux/stage_external_data.py", package / "stage_external_data.py")
        shutil.copy2(ROOT / "packaging/linux/README.md", package / "README.md")
        if any(path.is_symlink() for path in package.rglob("*")):
            raise ValueError("Staged source or runtime contains a symlink")
        provenance = {"schema": 1, "source_repository": "https://github.com/LucioFranco/orbiter-native",
                      "source_commit": commit, "inputs": dependencies,
                      "openorbiter": json.loads((ROOT / "ci/inputs.json").read_text())["openorbiter"],
                      "nassp": json.loads((ROOT / "ci/inputs.json").read_text())["nassp"],
                      "cargo_lock_sha256": sha256(ROOT / "orbiter/Src/WgpuClient/rust/Cargo.lock"),
                      "linux": platform.platform(), "notices": notices,
                      "external_data_bundled": False,
                      "validation": "hosted headless subset only; graphical runtime not validated by CI"}
        (package / "SOURCE-PROVENANCE.json").write_text(json.dumps(provenance, indent=2) + "\n")
        files = {str(path.relative_to(package)): sha256(path) for path in sorted(package.rglob("*")) if path.is_file()}
        (package / "SHA256SUMS.json").write_text(json.dumps(files, indent=2) + "\n")
        archive = output / (NAME + ".tar.gz")
        with tarfile.open(archive, "w:gz") as stream:
            stream.add(package, arcname=NAME)
    print(archive)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        sys.exit("Linux packaging blocked: " + str(error))
