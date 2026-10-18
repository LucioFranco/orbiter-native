#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run a declared CPU subset, or the unfiltered strict full acceptance suite."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def plan(tests, capabilities, requirements, full=False):
    properties = {test["name"]: {p["name"]: p["value"] for p in test.get("properties", [])}
                  for test in tests}
    if full:
        if "native_pdi_pilot" not in properties:
            raise ValueError("Full v9 acceptance requires the registered strict native_pdi_pilot gate")
        return sorted(properties), {}
    excluded = {}
    for name, props in properties.items():
        needed = set(props.get("LABELS", [])) & {"gpu", "desktop"}
        needed.update(requirements.get(name, {}).get("requires", []))
        missing = needed - set(capabilities)
        if missing:
            excluded[name] = "unavailable capability: " + ", ".join(sorted(missing))
    setups = {}
    for name, props in properties.items():
        for fixture in props.get("FIXTURES_SETUP", []):
            setups.setdefault(fixture, []).append(name)
    while True:
        before = len(excluded)
        for name, props in properties.items():
            if name in excluded:
                continue
            for fixture in props.get("FIXTURES_REQUIRED", []):
                if any(item in excluded for item in setups.get(fixture, [])):
                    excluded[name] = "fixture " + fixture + " requires an unavailable test"
            if any(item in excluded for item in props.get("DEPENDS", [])):
                excluded[name] = "depends on an unavailable test"
        if len(excluded) == before:
            break
    return sorted(set(properties) - set(excluded)), excluded


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--suite", choices=("hosted", "full"), default="hosted")
    args = parser.parse_args()
    build = args.build.resolve()
    cache = (build / "CMakeCache.txt").read_text()
    capabilities = {"nassp-data"} if "ORBITER_BUILD_NASSP:BOOL=ON" in cache else set()
    requirements = json.loads((ROOT / "ci/test-capabilities.json").read_text())["tests"]
    tests = json.loads(subprocess.check_output(
        ["ctest", "--test-dir", str(build), "--show-only=json-v1"], text=True))["tests"]
    selected, excluded = plan(tests, capabilities, requirements, full=args.suite == "full")
    if not selected:
        raise ValueError("Refusing an empty test suite")
    report = {"suite": args.suite, "full_acceptance": args.suite == "full",
              "selected": selected, "excluded": excluded,
              "known_strict_gate": "native_pdi_pilot; historical FAIL; see acceptance-context.json"}
    (build / "test-plan.json").write_text(json.dumps(report, indent=2) + "\n")
    receipt = build / "test-result.json"
    receipt.unlink(missing_ok=True)
    (build / "ctest.xml").unlink(missing_ok=True)
    # Full mode has no name/label/fixture exclusions and preserves the failure.
    command = ["ctest", "--test-dir", str(build), "--output-on-failure", "--no-tests=error",
               "--parallel", "1", "--timeout", "180", "--output-junit", str(build / "ctest.xml")]
    if args.suite == "hosted":
        command += ["--fixture-exclude-any", ".*",
                    "-R", "^(" + "|".join(re.escape(name) for name in selected) + ")$"]
    result = subprocess.run(command)
    counts = {"scheduled": len(selected), "excluded": len(excluded)}
    if (build / "ctest.xml").is_file():
        xml = ET.parse(build / "ctest.xml").getroot()
        counts.update({key: int(xml.attrib.get(key, 0)) for key in ("tests", "failures", "errors", "skipped")})
    receipt.write_text(json.dumps({"suite": args.suite, "status": "passed" if result.returncode == 0 else "failed",
                                   "ctest_returncode": result.returncode, "counts": counts}, indent=2) + "\n")
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
