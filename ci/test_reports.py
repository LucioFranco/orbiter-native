# SPDX-License-Identifier: MIT
"""Regression coverage for rejected leftovers and workflow upload authorization."""
import contextlib
import io
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest.mock import patch

import collect_reports as collector

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github/workflows/native-foundation.yml"


class ReportPublicationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.build.mkdir()
        (self.root / "ci").mkdir()
        (self.root / "ci/acceptance-context.json").write_text(
            (ROOT / "ci/acceptance-context.json").read_text())
        self.outputs = self.root / "outputs"
        self.env = patch.dict(os.environ, {"GITHUB_OUTPUT": str(self.outputs),
                                          "GITHUB_STEP_SUMMARY": str(self.root / "summary")})
        self.env.start()
        self.addCleanup(self.env.stop)
        self.root_patch = patch.object(collector, "ROOT", self.root)
        self.root_patch.start()
        self.addCleanup(self.root_patch.stop)
        self.argv = patch.object(sys, "argv", ["collect_reports", "--build", str(self.build), "--scope", "regression"])
        self.argv.start()
        self.addCleanup(self.argv.stop)
        self.junit()

    def junit(self, failures=0):
        (self.build / "ctest.xml").write_text(
            '<testsuite tests="1" failures="' + str(failures) + '" errors="0" skipped="0"/>')

    def output_values(self):
        return dict(line.split("=", 1) for line in self.outputs.read_text().splitlines())

    def upload_allowed(self, outcome):
        # Evaluate the actual workflow's two narrowly specified guard expressions
        # against collector outputs. This catches unconditional always() regression.
        gates = re.findall(r"- name: Upload reports only\n\s+if: (.+)", WORKFLOW.read_text())
        self.assertEqual(len(gates), 2)
        self.assertEqual(WORKFLOW.read_text().count(
            "path: " + "$" + "{{ steps.collect_reports.outputs.directory }}"), 2)
        values = self.output_values()
        results = []
        for gate in gates:
            expression = (gate.replace("always()", "True")
                          .replace("steps.collect_reports.outcome", repr(outcome))
                          .replace("steps.collect_reports.outputs.ready", repr(values.get("ready", "")))
                          .replace("&&", " and ").replace("||", " or "))
            results.append(bool(eval(expression, {"__builtins__": {}}, {})))
        self.assertEqual(results[0], results[1])
        return results[0]

    def collect(self):
        with contextlib.redirect_stdout(io.StringIO()):
            collector.main()

    def rejected(self, error=ValueError):
        with self.assertRaises(error):
            self.collect()
        self.assertEqual(self.output_values()["ready"], "false")
        self.assertFalse(self.upload_allowed("failure"))

    def test_existing_output_symlink_cannot_be_uploaded(self):
        foreign = self.root / "foreign"
        foreign.mkdir()
        (foreign / "not-a-report.bin").write_bytes(b"private leftover")
        (self.root / "ci-reports").symlink_to(foreign, target_is_directory=True)
        self.rejected()
        self.assertNotIn("directory", self.output_values())
        self.assertTrue((foreign / "not-a-report.bin").is_file())

    def test_existing_nested_output_symlink_cannot_be_uploaded(self):
        base = self.root / "ci-reports"
        base.mkdir()
        (base / "leftover").symlink_to(self.root / "ci", target_is_directory=True)
        self.rejected()
        self.assertNotIn("directory", self.output_values())

    def test_invalid_output_file_cannot_be_uploaded(self):
        (self.root / "ci-reports").write_text("not a directory")
        self.rejected()

    def test_stale_plain_output_is_outside_fresh_upload_directory(self):
        base = self.root / "ci-reports"
        base.mkdir()
        (base / "stale.bin").write_bytes(b"x" * (collector.LIMIT + 1))
        self.collect()
        output = Path(self.output_values()["directory"])
        self.assertEqual(output.parent, base.resolve())
        self.assertNotEqual(output, base)
        self.assertEqual({p.name for p in output.iterdir()}, {"ctest.xml", "scope.json"})
        self.assertLessEqual(sum(p.stat().st_size for p in output.iterdir()), collector.LIMIT)
        self.assertTrue(self.upload_allowed("success"))

    def test_failed_collection_revokes_prior_readiness_and_cannot_upload(self):
        self.collect()
        prior = self.output_values()["directory"]
        (self.build / "test-plan.json").write_bytes(b"x" * (collector.LIMIT + 1))
        self.rejected()
        # Even an old successful output path cannot authorize a failed collection.
        self.assertEqual(self.output_values()["directory"], prior)

    def test_malformed_report_collection_cannot_upload(self):
        (self.build / "ctest.xml").write_text("invalid XML")
        self.rejected(collector.ET.ParseError)
        self.assertNotIn("directory", self.output_values())

    def test_dangling_input_symlink_cannot_upload(self):
        (self.build / "test-result.json").symlink_to(self.root / "absent")
        self.rejected()

    def test_normal_test_failure_still_has_safe_report_upload(self):
        self.junit(failures=1)
        self.collect()
        output = Path(self.output_values()["directory"])
        summary = json.loads((output / "scope.json").read_text())
        self.assertEqual(summary["current_test_status"], "failed")
        self.assertFalse(summary["full_acceptance"])
        self.assertTrue(self.upload_allowed("success"))
        self.assertFalse(self.upload_allowed("failure"))


if __name__ == "__main__":
    unittest.main()
