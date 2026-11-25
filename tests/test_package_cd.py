#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Exercise the hybrid writer and verifier with synthetic Macintosh forks."""
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from macbinary_rename import crc16_xmodem
import hashlib
from package_cd import APP, SOURCE_ISO, hybrid_partition, iso_root, package, text_macbinary


@unittest.skipUnless(
    all(shutil.which(tool) for tool in ("faketime", "genisoimage", "hmount", "hls", "hcopy", "humount")),
    "requires the Linux CD toolchain",
)
class PackageCDTest(unittest.TestCase):
    def test_roundtrip_and_rejection(self):
        epoch = 1794632136
        data, resource = b"unique application data", b"unique resource fork"
        raw = bytearray(text_macbinary(APP, data, epoch))
        raw[65:73] = b"APPLPdlE"
        struct.pack_into(">I", raw, 87, len(resource))
        struct.pack_into(">H", raw, 124, crc16_xmodem(raw[:124]))
        raw += resource + bytes(-len(resource) % 128)
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "PoodleEarth-Finder.bin").write_bytes(raw)
            (build / "license").mkdir()
            for part in (1, 2):
                (build / "license" / f"License-{part}.txt").write_bytes(f"Part {part}\n".encode())
            source = build / "source.tar.gz"
            source.write_bytes(b"\x1f\x8b unique source archive " + bytes(range(256)) * 300)
            notes = build / "notes.txt"
            notes.write_text(f"Revision x\nSHA-256:  {hashlib.sha256(source.read_bytes()).hexdigest()}\n")
            image, second = build / "first.iso", build / "second.iso"
            package(build, image, epoch, source, notes, False)
            time.sleep(1.1)  # Expose any accidental use of whole-second wall time.
            package(build, second, epoch, source, notes, False)
            original = image.read_bytes()
            rebuilt = second.read_bytes()
            self.assertEqual(len(original), len(rebuilt))
            if original != rebuilt:
                offsets = [i for i, (a, b) in enumerate(zip(original, rebuilt)) if a != b]
                self.fail(f"{len(offsets)} different bytes; first offsets: {offsets[:60]}")
            partition = hybrid_partition(original)
            start, size = iso_root(original)[SOURCE_ISO]
            self.assertEqual(original[start * 2048:start * 2048 + size], source.read_bytes())
            self.assertIn(source.read_bytes(), partition)
            for label, before, after in (
                ("missing document", b"DataCredit", b"LostCredit"),
                ("resource fork", resource, bytes(len(resource))),
                ("Finder creator", b"APPLPdlE", b"APPLBAD!"),
            ):
                with self.subTest(label=label):
                    self.assertIn(before, partition)
                    offset = original.index(partition)
                    second.write_bytes(original[:offset] + partition.replace(before, after)
                                       + original[offset + len(partition):])
                    with self.assertRaises((ValueError, subprocess.CalledProcessError)):
                        package(build, second, epoch, source, notes, True)
            second.write_bytes(original[:32768] + b"\0" + original[32769:])
            with self.assertRaisesRegex(ValueError, "primary volume descriptor"):
                package(build, second, epoch, source, notes, True)
            # The standalone archive must be the embedded one: a different archive is refused in verify-only,
            # and notes that name another SHA-256 are refused before anything is built.
            other = build / "other.tar.gz"
            other.write_bytes(source.read_bytes() + b"x")
            wrong_notes = build / "wrong.txt"
            wrong_notes.write_text(f"SHA-256: {hashlib.sha256(other.read_bytes()).hexdigest()}\n")
            with self.assertRaisesRegex(ValueError, "notes do not name"):
                package(build, second, epoch, source, wrong_notes, False)
            other_notes = build / "other-notes.txt"
            other_notes.write_text(wrong_notes.read_text())
            with self.assertRaisesRegex(ValueError, "differs from the standalone"):
                package(build, image, epoch, other, other_notes, True)


if __name__ == "__main__":
    unittest.main()
