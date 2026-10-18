#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Inventory a proposed v9 preview; refuse readiness until explicit review gates pass."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--build", type=Path)
    parser.add_argument("--target", default="macos-arm64")
    parser.add_argument("--check-ready", action="store_true")
    args = parser.parse_args()
    contract = json.loads((ROOT / "packaging/preview-contract.json").read_text())
    if args.target != contract["target"]:
        print(json.dumps({"status": "unsupported", "target": args.target,
                          "reason": "No validated native runtime/deployment/installer exists for this target"}))
        return 2
    ledger = json.loads((ROOT / contract["resource_and_staging_inventory"]).read_text())
    mismatches = []
    for entry in ledger["files"]:
        path = args.source_root / entry["source"]
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            mismatches.append(entry["source"])
    missing = []
    if args.build:
        missing = [name for name in contract["binary_candidates"] if not (args.build / name).is_file()]
    report = {"target": args.target, "profile": contract["profile"], "status": "blocked",
              "creates_or_installs_files": False, "source_inputs_checked": len(ledger["files"]),
              "source_hash_mismatches": mismatches, "missing_build_outputs": missing,
              "exact_binary_candidates": contract["binary_candidates"],
              "external_assets": contract["external_only"], "blocked_on": contract["blocked_on"]}
    print(json.dumps(report, indent=2))
    # This preparation tool deliberately has no copy/archive/download/sign/install path.
    # Merely editing release_enabled cannot clear missing legal/technical review.
    return 2 if args.check_ready or mismatches or missing else 0


if __name__ == "__main__":
    sys.exit(main())
