#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify separately acquired NASSP v9 data and stage an original mission."""
import argparse
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nassp-data", required=True, type=Path)
    parser.add_argument("--textures", type=Path,
                        help="Optional separately acquired Orbiter planetary Textures tree")
    parser.add_argument("--output", type=Path,
                        help="Fresh runtime directory; defaults beside this script")
    args = parser.parse_args()
    package = Path(__file__).resolve().parent
    source = package / "source/repository/orbiter"
    output = args.output or package / "runtime-original-insertion"
    if output.exists() or output.is_symlink():
        parser.error("Use a fresh runtime output directory: " + str(output))
    data = args.nassp_data.resolve(strict=True)
    if not data.is_dir():
        parser.error("NASSP data must be a directory")
    verify = [sys.executable, str(source / "tools/nassp_demo.py"), "check-data",
              "--profile", "v9", "--nassp-data-root", str(data),
              "--source-addon", str(source / "Addons/NASSP"), "--verify-all-assets"]
    if args.textures:
        verify += ["--orbiter-textures", str(args.textures.resolve(strict=True))]
    subprocess.run(verify, check=True)
    stage = [sys.executable, str(source / "tools/stage_nassp_mission.py"), str(package),
             "--profile", "v9", "--addon", str(data), "--output", str(output)]
    if args.textures:
        stage += ["--textures", str(args.textures.resolve(strict=True))]
    subprocess.run(stage, check=True)
    print("Staged " + str(output))
    print("Run: " + str(package / "orbiter") + " --root " + str(output) +
          " --scenario 'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 02 - Insertion'"
          " --graphics WgpuClient --window --vc")


if __name__ == "__main__":
    main()
