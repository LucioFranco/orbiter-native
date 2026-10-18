#!/usr/bin/env python3
"""Exercise upstream-only deletions in the native engine inventory."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


EXPORTER = Path(__file__).resolve().parents[1] / "tools" / "orbiter_upstream.py"


class UpstreamDeletionTests(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory(prefix="orbiter-inventory-")
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)
        self.reference = self.root / "reference"
        self.native = self.root / "native"
        self.engine = self.native / "orbiter"
        self.removed_path = "Doc/Scenery/fixture.pdf"
        self.removed_bytes = b"upstream binary\0\r\nfixture"
        for repo in (self.reference, self.native):
            repo.mkdir()
            self.git(repo, "init", "--quiet")
        self.write(self.reference / "Src/shared.cpp", b"shared\r\n")
        self.write(self.engine / "Src/shared.cpp", b"shared\n")
        self.write(self.reference / self.removed_path, self.removed_bytes)
        for excluded in ("Addons/fixture", "docs/fixture", "upstream/fixture"):
            self.write(self.reference / excluded, b"excluded")
        for repo in (self.reference, self.native):
            self.git(repo, "add", ".")
            self.git(repo, "-c", "user.name=Inventory Test",
                     "-c", "user.email=inventory@example.invalid",
                     "-c", "commit.gpgsign=false", "commit", "--quiet", "-m", "fixture")
        self.commit = self.git(self.reference, "rev-parse", "HEAD").strip()

    @staticmethod
    def write(path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    @staticmethod
    def git(repo, *args):
        return subprocess.check_output(["git", "-C", str(repo), *args], text=True)

    def assert_deleted(self):
        output = self.root / "inventory.json"
        subprocess.run([sys.executable, str(EXPORTER), str(self.reference),
                        "--engine", str(self.engine), "--commit", self.commit,
                        "--output", str(output)], check=True, capture_output=True, text=True)
        inventory = json.loads(output.read_text())
        self.assertEqual(inventory["unchanged_files"], 1)
        self.assertEqual([item["path"] for item in inventory["files"]], [self.removed_path])
        removal = inventory["files"][0]
        self.assertEqual(removal["status"], "deleted")
        self.assertEqual(removal["upstream_lf_sha256"], hashlib.sha256(self.removed_bytes).hexdigest())
        self.assertIsNone(removal["native_lf_sha256"])

    def test_upstream_only_tracked_file_is_reported_as_deleted(self):
        self.assert_deleted()

    def test_untracked_native_copy_does_not_hide_committed_deletion(self):
        self.write(self.engine / self.removed_path, b"untracked native copy")
        self.assert_deleted()


if __name__ == "__main__":
    unittest.main()
