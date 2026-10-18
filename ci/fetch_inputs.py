#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Retrieve exact public source; optional v9 data is verified and never uploaded."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(["git", *map(str, args)], text=True).strip()


def checkout(repository, commit, destination):
    if destination.is_symlink():
        raise ValueError("Input cache must be an independent checkout: " + str(destination))
    if destination.exists():
        if git("-C", destination, "rev-parse", "HEAD") != commit:
            raise ValueError("Cached revision differs: " + str(destination))
        if git("-C", destination, "status", "--porcelain", "--untracked-files=all"):
            raise ValueError("Cached source was modified: " + str(destination))
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["git", "init", "--quiet", str(destination)], check=True)
    subprocess.run(["git", "-C", str(destination), "-c", "core.autocrlf=false",
                    "fetch", "--quiet", "--depth=1", repository, commit], check=True)
    subprocess.run(["git", "-C", str(destination), "-c", "core.autocrlf=false",
                    "checkout", "--quiet", "--detach", "FETCH_HEAD"], check=True)
    if git("-C", destination, "rev-parse", "HEAD") != commit:
        raise ValueError("Fetched revision differs from the pin")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nassp-data", action="store_true",
                        help="Explicitly acquire original release434 data for ephemeral CI tests; not a redistribution grant")
    args = parser.parse_args()
    cache = ROOT / ".ci-cache"
    for name, item in json.loads((ROOT / "ci/inputs.json").read_text())["dependencies"].items():
        checkout(item["repository"], item["commit"], cache / "deps" / name.lower())
    if args.nassp_data:
        sys.path.insert(0, str(ROOT / "orbiter/tools"))
        from nassp_demo import profile_manifest, verify_data, verify_sources
        manifest, _ = profile_manifest("v9")
        if manifest["commit"] != json.loads((ROOT / "ci/inputs.json").read_text())["nassp"]:
            raise ValueError("CI and v9 data pins differ")
        data = cache / "nassp-v9"
        checkout(manifest["repository"], manifest["commit"], data)
        report = verify_data(data, "v9", full_assets=True)
        report.update(verify_sources(ROOT / "orbiter/Addons/NASSP", "v9"))
        print(json.dumps({k: report[k] for k in ("profile", "upstream_commit", "critical_hashes_verified",
                                               "asset_files_present", "all_asset_blobs_verified",
                                               "source_adaptations_verified", "exported_source_lists_verified")}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        sys.exit("Pinned input retrieval failed: " + str(error))
