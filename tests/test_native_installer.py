#!/usr/bin/env python3
"""Exercise production package version checks without installing any packages."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeInstallerVersionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="calculator installer policy ")
        work = Path(cls.temporary.name)
        source = work / "policy.cpp"
        cls.program = work / "policy"
        source.write_text(
            '#define main calculator_native_installer_entrypoint\n'
            f'#include {json.dumps(str(ROOT / "support/tools/native_installer.cpp"))}\n'
            '#undef main\n'
            r'''
int main(int argc, char **argv)
{
    if (argc == 2 && std::string(argv[1]) == "status") {
        auto version = package_version_from_status(std::cin, "calculator");
        std::cout << version.value_or("not-installed") << '\n';
        return 0;
    }
    if (argc != 3) return 2;
    std::optional<std::string> installed;
    if (std::string(argv[1]) != "not-installed") installed = argv[1];
    for (const auto &argument : package_install_arguments(
             "/tmp/calculator rebuilt package.deb", argv[2], installed))
        std::cout << argument << '\n';
    return 0;
}
''',
            encoding="utf-8",
        )
        subprocess.run(
            ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Wpedantic",
             "-Werror", str(source), "-o", str(cls.program)],
            check=True,
        )

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def plan(self, installed, target):
        return subprocess.run([str(self.program), installed, target],
                              text=True, capture_output=True)

    def test_same_version_local_rebuild_is_explicitly_reinstalled(self):
        result = self.plan("0.2.53", "0.2.53")
        self.assertEqual(result.returncode, 0, result.stderr)
        arguments = result.stdout.splitlines()
        self.assertIn("--reinstall", arguments)
        self.assertIn("--allow-downgrades", arguments)
        self.assertEqual(arguments[-1], "/tmp/calculator rebuilt package.deb")

    def test_fresh_install_and_upgrade_keep_normal_apt_policy(self):
        for installed in ("not-installed", "0.2.52", "0.2.53~rc1"):
            with self.subTest(installed=installed):
                result = self.plan(installed, "0.2.53")
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertNotIn("--allow-downgrades", result.stdout.splitlines())

    def test_actually_older_release_is_refused(self):
        for installed in ("0.2.54", "0.2.53+local1", "1:0.2.52"):
            with self.subTest(installed=installed):
                result = self.plan(installed, "0.2.53")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("refusing to replace installed Calculator", result.stderr)
                self.assertEqual(result.stdout, "")

    def test_status_parser_requires_the_installed_canonical_package(self):
        status = (
            "Package: unrelated\r\nStatus: install ok installed\r\nVersion: 99\r\n\r\n"
            "Package: calculator\r\nStatus: install ok installed\r\nVersion: 0.2.53\r\n"
        )
        result = subprocess.run([str(self.program), "status"], input=status,
                                text=True, capture_output=True, check=True)
        self.assertEqual(result.stdout.strip(), "0.2.53")
        result = subprocess.run(
            [str(self.program), "status"],
            input=status.replace("Status: install ok installed", "Status: deinstall ok config-files"),
            text=True, capture_output=True, check=True,
        )
        self.assertEqual(result.stdout.strip(), "not-installed")


if __name__ == "__main__":
    unittest.main()
