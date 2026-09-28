"""Verify the DKMS patch series and the full-source submodule have identical results."""

import hashlib
import io
import pathlib
import re
import subprocess
import tarfile
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
DRIVERS = ROOT / "submodules" / "tuxedo-drivers"
PATCHES = ROOT / "kernel" / "patches"


class DriverReleaseTests(unittest.TestCase):
    def test_patch_series_matches_full_source(self):
        patches = sorted(PATCHES.glob("*.patch"))
        self.assertEqual(16, len(patches))
        archive = subprocess.check_output(
            ["git", "-C", str(DRIVERS), "archive", "v4.22.3"]
        )
        changed = subprocess.check_output(
            ["git", "-C", str(DRIVERS), "diff", "--name-only", "v4.22.3", "--", "src", "Documentation"],
            text=True,
        ).splitlines()
        with tempfile.TemporaryDirectory() as directory:
            baseline = pathlib.Path(directory)
            with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
                tar.extractall(baseline, filter="data")
            for patch in patches:
                subprocess.run(
                    ["patch", "-d", str(baseline), "-p1", "--batch", "-i", str(patch)],
                    check=True,
                    stdout=subprocess.DEVNULL,
                )
            for path in changed:
                self.assertEqual(
                    (baseline / path).read_bytes(),
                    (DRIVERS / path).read_bytes(),
                    path,
                )
        self.assertEqual(
            (ROOT / "kernel" / "uniwill_mechrevo_quirks.h").read_bytes(),
            (DRIVERS / "src" / "uniwill_mechrevo_quirks.h").read_bytes(),
        )

    def test_arch_package_hashes(self):
        spec = (ROOT / "kernel" / "packaging" / "arch" / "PKGBUILD").read_text()
        names = re.findall(r'"(\d{4}[^\"]+\.patch)"', spec)
        sums = re.findall(r"'([0-9a-f]{64})'", spec)
        self.assertEqual(len(list(PATCHES.glob("*.patch"))), len(names))
        self.assertEqual(len(names) + 3, len(sums))
        for name, checksum in zip(names, sums[3:]):
            self.assertEqual(checksum, hashlib.sha256((PATCHES / name).read_bytes()).hexdigest(), name)


if __name__ == "__main__":
    unittest.main()
