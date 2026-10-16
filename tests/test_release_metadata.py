#!/usr/bin/env python3
"""Exercise production release-intent decisions against disposable Git history."""
import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest

MODULE_PATH = Path(__file__).resolve().parents[1] / "support/release/release_metadata.py"
spec = importlib.util.spec_from_file_location("release_metadata", MODULE_PATH)
metadata = importlib.util.module_from_spec(spec)
spec.loader.exec_module(metadata)


class ReleaseIdentityTests(unittest.TestCase):
    def test_pending_corrections_and_immutable_tags(self):
        with tempfile.TemporaryDirectory(prefix="calculator release history ") as directory:
            root = Path(directory)
            previous_root = metadata.ROOT
            metadata.ROOT = root
            try:
                def git(*args):
                    return subprocess.check_output(["git", "-C", directory, *args],
                                                   stderr=subprocess.DEVNULL, text=True).strip()

                def commit(version, extra=""):
                    (root / "VERSION").write_text(version + "\n", encoding="utf-8")
                    (root / "source").write_text(extra, encoding="utf-8")
                    git("add", ".")
                    git("commit", "-qm", "Fixture")
                    return git("rev-parse", "HEAD")

                git("init", "-q")
                git("config", "user.name", "Release fixture")
                git("config", "user.email", "fixture@example.invalid")
                git("config", "commit.gpgsign", "false")
                old = commit("1.0.0")
                git("tag", "v1.0.0")
                introduced = commit("1.0.1", "first candidate")
                self.assertTrue(metadata.should_publish("push", old, False, "1.0.1"))
                corrected = commit("1.0.1", "corrected before publication")
                self.assertTrue(metadata.should_publish("push", introduced, False, "1.0.1"))
                self.assertTrue(metadata.should_publish("workflow_dispatch", "", True, "1.0.1"))
                self.assertFalse(metadata.should_publish("pull_request", old, False, "1.0.1"))
                git("tag", "v1.0.1")
                self.assertTrue(metadata.should_publish("push", introduced, False, "1.0.1"))
                self.assertTrue(metadata.should_publish("workflow_dispatch", "", True, "1.0.1"))

                commit("1.0.1", "maintenance after publication")
                self.assertFalse(metadata.should_publish("push", corrected, False, "1.0.1"))
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    metadata.should_publish("workflow_dispatch", "", True, "1.0.1")
                before = git("rev-parse", "HEAD")
                commit("0.9.0", "rollback")
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    metadata.should_publish("push", before, False, "0.9.0")
                before = git("rev-parse", "HEAD")
                commit("1.0.1", "attempted old identity reuse")
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    metadata.should_publish("push", before, False, "1.0.1")
            finally:
                metadata.ROOT = previous_root


if __name__ == "__main__":
    unittest.main()
