#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify v9 panel bitmaps before generating native runtime resources.

The BMP bytes come from a separately acquired pristine NASSP checkout. The
adapted C++ source and RC tables remain in the corresponding-source tree.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

from nassp_bitmap_resources import collect

REPO = Path(__file__).resolve().parents[2]
LEDGER = REPO / "packaging/v9-input-review.json"
EXPECTED_MAP = REPO / "packaging/v9-bitmap-map.json"
PREFIX = "orbiter/Addons/NASSP/"


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def verify(data_root, adapted_root):
    ledger = json.loads(LEDGER.read_text())
    bitmaps = [entry for entry in ledger["files"] if "/Bitmaps/" in entry["source"]]
    tables = [entry for entry in ledger["files"]
              if entry["role"] == "Resource table/header; required source input"]
    if len(bitmaps) != 397 or len(tables) != 6:
        raise ValueError("Unexpected v9 bitmap/RC inventory length")
    external_project = data_root / "Orbitersdk/samples/ProjectApollo"
    expected_names = set()
    for entry in bitmaps:
        relative = Path(entry["source"].split(PREFIX, 1)[1])
        bitmap = data_root / relative
        if not bitmap.is_file() or sha256(bitmap.read_bytes()) != entry["sha256"]:
            raise ValueError(f"Original NASSP bitmap identity mismatch: {relative}")
        expected_names.add(relative.relative_to("Orbitersdk/samples/ProjectApollo/Bitmaps"))
    actual_names = {path.relative_to(external_project / "Bitmaps")
                    for path in (external_project / "Bitmaps").rglob("*")
                    if path.is_file() and path.suffix.lower() == ".bmp"}
    if actual_names != expected_names:
        raise ValueError(f"NASSP bitmap set changed: missing={len(expected_names-actual_names)} "
                         f"extra={len(actual_names-expected_names)}")
    for entry in tables:
        relative = Path(entry["source"].split(PREFIX, 1)[1]).relative_to(
            "Orbitersdk/samples/ProjectApollo")
        path = adapted_root / relative
        if not path.is_file() or sha256(path.read_bytes().replace(b"\r\n", b"\n")) != entry["sha256"]:
            raise ValueError(f"Adapted resource table/header changed: {relative}")
    if collect(external_project, "Resources/NASSP") != json.loads(EXPECTED_MAP.read_text()):
        raise ValueError("Original bitmap ID/path map differs from compiled v9 source")
    return {"verified_bitmaps": len(bitmaps), "verified_tables": len(tables),
            "mapping": "exact v9 ID/path map"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-root", required=True, type=Path)
    parser.add_argument("--adapted-root", required=True, type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.data_root.resolve(strict=True),
                                args.adapted_root.resolve(strict=True))))
    except (OSError, ValueError) as error:
        parser.exit(2, f"NASSP bitmap preflight: {error}\n")


if __name__ == "__main__":
    main()
