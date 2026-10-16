#!/usr/bin/env python3
"""Exercise the production source packager using disposable Git repositories."""
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest

PACKAGER = Path(__file__).resolve().parents[1] / "support/release/build_source_bundle.sh"


class SourceBundleTests(unittest.TestCase):
    def git(self, repository, *args):
        return subprocess.check_output(
            ["git", "-C", str(repository), *args], stderr=subprocess.DEVNULL,
            text=True,
        ).strip()

    def repository(self, directory):
        directory.mkdir()
        self.git(directory, "init", "-q")
        self.git(directory, "config", "user.name", "Source fixture")
        self.git(directory, "config", "user.email", "fixture@example.invalid")
        self.git(directory, "config", "commit.gpgsign", "false")
        return directory

    def commit(self, repository):
        self.git(repository, "add", ".")
        self.git(repository, "commit", "-qm", "Fixture source")
        return self.git(repository, "rev-parse", "HEAD")

    def test_committed_inputs_concurrency_and_failure(self):
        with tempfile.TemporaryDirectory(prefix="calculator source fixture ") as temporary:
            base = Path(temporary)
            scratch = base / "scratch"
            scratch.mkdir()
            sentinel = scratch / "calculator-source" / "keep"
            sentinel.parent.mkdir()
            sentinel.write_text("unrelated caller state", encoding="utf-8")

            leaf = self.repository(base / "leaf origin")
            (leaf / "leaf.txt").write_text("committed leaf", encoding="utf-8")
            self.commit(leaf)
            dependency = self.repository(base / "dependency origin")
            (dependency / "dependency.txt").write_text("committed dependency", encoding="utf-8")
            self.commit(dependency)
            self.git(dependency, "-c", "protocol.file.allow=always", "submodule",
                     "add", "-q", str(leaf), "nested leaf")
            self.commit(dependency)

            project = self.repository(base / "project checkout")
            (project / "VERSION").write_text("1.2.3\n", encoding="utf-8")
            (project / "source.txt").write_text("committed product", encoding="utf-8")
            self.commit(project)
            self.git(project, "-c", "protocol.file.allow=always", "submodule",
                     "add", "-q", str(dependency), "src/dependency")
            revision = self.commit(project)
            self.git(project, "-c", "protocol.file.allow=always", "submodule",
                     "update", "--init", "--recursive")
            child = project / "src/dependency"
            (project / "source.txt").write_text("uncommitted edit", encoding="utf-8")
            (project / "private-runner-file").write_text("must not ship", encoding="utf-8")
            (child / "dependency.txt").write_text("uncommitted dependency", encoding="utf-8")
            (child / "private-child-file").write_text("must not ship", encoding="utf-8")
            environment = dict(os.environ, GITHUB_WORKSPACE=str(project), RUNNER_TEMP=str(scratch))

            def package(output, version="1.2.3"):
                return subprocess.run(
                    ["bash", str(PACKAGER), version, revision, str(output)],
                    env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                    text=True,
                )

            first, second = base / "first.tar.gz", base / "second.tar.gz"
            with ThreadPoolExecutor(max_workers=2) as pool:
                results = list(pool.map(package, [first, second]))
            for result in results:
                self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            with tarfile.open(first) as archive:
                prefix = "Calculator-1.2.3/"
                self.assertEqual(archive.extractfile(prefix + "source.txt").read(), b"committed product")
                self.assertEqual(archive.extractfile(prefix + "src/dependency/dependency.txt").read(),
                                 b"committed dependency")
                self.assertEqual(archive.extractfile(prefix + "src/dependency/nested leaf/leaf.txt").read(),
                                 b"committed leaf")
                names = archive.getnames()
                self.assertFalse(any("private-" in name or "/.git/" in name
                                     or name.endswith("/.git") for name in names))

            original = first.read_bytes()
            self.assertNotEqual(package(first, "9.9.9").returncode, 0)
            self.assertEqual(first.read_bytes(), original)
            self.git(child, "checkout", "-q", "HEAD^")
            self.assertNotEqual(package(first).returncode, 0)
            self.assertEqual(first.read_bytes(), original)
            self.git(project, "submodule", "deinit", "-f", "src/dependency")
            self.assertNotEqual(package(first).returncode, 0)
            self.assertEqual(first.read_bytes(), original)
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "unrelated caller state")
            self.assertEqual(list(scratch.iterdir()), [sentinel.parent])
            self.assertFalse(list(base.glob("*.tmp.*")))


if __name__ == "__main__":
    unittest.main()
