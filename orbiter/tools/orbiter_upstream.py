#!/usr/bin/env python3
"""Inventory the tracked native engine overlay against an exact pristine upstream.

NASSP has its own removable patch stack. This ledger covers engine source,
SDK, external build policy, native assets and regression tools; it excludes
historical reports and documentation. CRLF normalization affects comparisons
only. File bytes are never changed. The union of both tracked trees includes
upstream files omitted from the native checkout.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

PIN = "bd1d101f348da66e8c5b986633127c05e18d42c8"
GROUPS = {
    "build": "Replace external native CMake policy with equivalent upstream targets; preserve source selection and matching-data preflight.",
    "host-platform": "Adopt upstream native host, VFS, input and audio services, then remove the matching platform implementation and dependent registrations.",
    "graphics": "Adopt an upstream native graphics client and cockpit services, then remove WgpuClient and its build targets/assets.",
    "sdk-core": "Adopt equivalent upstream portable SDK/core changes after original mission and state regressions pass.",
    "regression-tools": "Retain or upstream the corresponding validation; remove only with equivalent upstream coverage.",
    "native-assets": "Replace with equivalent upstream licensed native assets and remove their generation/build references.",
    "upstream-omissions": "Restore omitted upstream files if required by native packaging; remove their omission records once restored.",
}


def group(path):
    if path.startswith(("Tests/", "tools/")):
        return "regression-tools"
    if path.startswith("Extern/NativeFonts/"):
        return "native-assets"
    if path.endswith(".cmake") or Path(path).name.startswith("CMake"):
        return "build"
    if path.startswith("Src/WgpuClient/"):
        return "graphics"
    if "/Platform/" in path:
        return "host-platform"
    return "sdk-core"


def digest(data):
    return hashlib.sha256(data if b"\0" in data else data.replace(b"\r\n", b"\n")).hexdigest()


def tracked_paths(root):
    top = Path(subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "--show-toplevel"], text=True).strip())
    tracked = subprocess.check_output(
        ["git", "-C", str(top), "ls-files", "-z", "--", root.relative_to(top).as_posix()],
        text=True).split("\0")
    return {(top / path).relative_to(root).as_posix() for path in tracked if path}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("--engine", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--commit", default=PIN)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    def git(*argv):
        return subprocess.check_output(["git", "-C", str(args.reference), *argv], text=True).strip()
    if git("rev-parse", "HEAD") != args.commit or git("status", "--porcelain"):
        parser.error("reference must be pristine at the exact specified commit")
    engine = args.engine.resolve()
    reference_root = args.reference.resolve()
    native_tracked = tracked_paths(engine)
    upstream_tracked = tracked_paths(reference_root)
    files = []
    unchanged = 0
    for path in sorted(native_tracked | upstream_tracked):
        if path.startswith(("Addons/", "docs/", "upstream/")):
            continue
        reference = reference_root / path
        source = engine / path
        before = reference.read_bytes() if path in upstream_tracked and reference.is_file() else None
        after = source.read_bytes() if path in native_tracked and source.is_file() else None
        if before is not None and after is not None and digest(before) == digest(after):
            unchanged += 1
            continue
        files.append({"path": path, "group": "upstream-omissions" if after is None else group(path),
                      "status": "added" if before is None else "deleted" if after is None else "modified",
                      "upstream_lf_sha256": digest(before) if before is not None else None,
                      "native_lf_sha256": digest(after) if after is not None else None})
    record = {"schema": 1, "upstream": {"repository": "https://github.com/orbitersim/orbiter.git",
              "release_ref": "latest", "commit": args.commit, "tree": git("rev-parse", "HEAD^{tree}")},
              "scope": "Union of both tracked engine trees excluding Addons/, docs/ and upstream/",
              "normalisation": "CRLF to LF for text; binary bytes unchanged", "unchanged_files": unchanged,
              "groups": {key: {"removal_path": removal,
                  "removal_trigger": "Omitted files restored" if key == "upstream-omissions" else
                      "Equivalent upstream implementation and unchanged native/original-mission regressions pass"}
                  for key, removal in GROUPS.items()}, "files": files}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(record, indent=2) + "\n")
    print(f"Verified {args.commit}: {unchanged} unchanged, {len(files)} engine/build/test differences, "
          f"{sum(item['status'] == 'deleted' for item in files)} upstream omissions")


if __name__ == "__main__":
    main()
