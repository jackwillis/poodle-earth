#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Build and independently read back both views of the hybrid CD.

The disc carries the release's corresponding-source archive byte for byte: --source is the very file
tools/package-release.sh publishes beside the disc, and --notes is the short text that names its revision
and SHA-256. Both are read back from the Macintosh HFS view (hfsutils) and from the ISO 9660 view (a
reader written here, not cdrkit's) and compared with the standalone archive.
"""
import argparse
import hashlib
from datetime import datetime, timezone
import os
from pathlib import Path
import struct
import subprocess
import tempfile

from floppy_report import macbinary
from macbinary_rename import MAC_EPOCH_OFFSET, crc16_xmodem, rename


APP = "Poodle Earth"
DOCUMENTS = {
    "ReadMe": Path("os9/ReadMe.txt"),
    "License (1 of 2)": Path("license/License-1.txt"),
    "License (2 of 2)": Path("license/License-2.txt"),
    "DataCredit": Path("data/DATA-CREDITS.txt"),
}
SOURCE = "Source.tar.gz"      # HFS name; cdrkit gives the ISO 9660 view SOURCE.TGZ;1
SOURCE_ISO = "SOURCE.TGZ;1"
SOURCE_KIND = b"GzipSITx"    # file type and creator StuffIt Expander opens
NOTES = "Source Notes"
ENV = {**os.environ, "LC_ALL": "C", "TZ": "UTC"}


def run(*args, **kwargs):
    return subprocess.run(args, check=True, env=ENV, **kwargs)


def file_macbinary(name, data, epoch, kind):
    header = bytearray(128)
    header[65:73] = kind
    struct.pack_into(">I", header, 83, len(data))
    header[122:124] = bytes((129, 129))
    struct.pack_into(">H", header, 124, crc16_xmodem(header[:124]))
    return rename(bytes(header) + data + bytes(-len(data) % 128), name, epoch)


def text_macbinary(name, data, epoch):
    return file_macbinary(name, data, epoch, b"TEXTttxt")


def iso_root(raw):
    """Name -> (extent, size) for the files in the ISO 9660 root directory, read directly from the image."""
    if raw[32768:32775] != b"\x01CD001\x01":
        raise ValueError("missing ISO-9660 primary volume descriptor")
    record = raw[32768 + 156:32768 + 190]
    extent, size = struct.unpack_from("<I", record, 2)[0], struct.unpack_from("<I", record, 10)[0]
    directory = raw[extent * 2048:extent * 2048 + size]
    files, offset = {}, 0
    while offset < len(directory):
        length = directory[offset]
        if length == 0:                      # records never straddle a 2048-byte sector
            offset = (offset // 2048 + 1) * 2048
            continue
        entry = directory[offset:offset + length]
        flags, name_length = entry[25], entry[32]
        if not flags & 2:
            start, nbytes = struct.unpack_from("<I", entry, 2)[0], struct.unpack_from("<I", entry, 10)[0]
            files[entry[33:33 + name_length].decode("ascii")] = (start, nbytes)
        offset += length
    return files


def check_source(source, label, data):
    if data != source:
        raise ValueError(f"source archive in the {label} differs from the standalone archive")
    return hashlib.sha256(data).hexdigest()


def hybrid_partition(raw):
    if len(raw) % 2048 or raw[32768:32775] != b"\x01CD001\x01":
        raise ValueError("missing ISO-9660 primary volume descriptor")
    iso_blocks = struct.unpack_from("<I", raw, 32768 + 80)[0]
    if not 16 < iso_blocks <= len(raw) // 2048:
        raise ValueError("invalid ISO-9660 volume size")
    if raw[:2] != b"ER":
        raise ValueError("missing Apple driver descriptor")
    block = struct.unpack_from(">H", raw, 2)[0]
    if block not in (512, 2048) or raw[block:block + 2] != b"PM":
        raise ValueError("invalid Apple Partition Map block size or signature")
    count = struct.unpack_from(">I", raw, block + 4)[0]
    if not 1 <= count < 32768 // block:
        raise ValueError("invalid Apple Partition Map entry count")
    for index in range(1, count + 1):
        offset = block * index
        entry = raw[offset:offset + block]
        if entry[:2] != b"PM":
            raise ValueError("invalid Apple Partition Map entry")
        if entry[48:80].split(b"\0", 1)[0] == b"Apple_HFS":
            start, length = struct.unpack_from(">II", entry, 8)
            start, end = start * block, (start + length) * block
            if end > len(raw) or end - start < 1536 or raw[start + 1024:start + 1026] != b"BD":
                raise ValueError("invalid classic HFS partition")
            return raw[start:end]
    raise ValueError("missing Apple_HFS partition")


def check_application(actual, original):
    files = macbinary(actual)
    if (actual[65:73] != b"APPLPdlE" or original[65:73] != actual[65:73]
            or files != macbinary(original) or files["name"] != APP):
        raise ValueError("application name, type/creator or forks differ")


def verify(image, original, documents, source, work):
    # Read the HFS partition with hfsutils, independently of cdrkit's writer.
    partition = work / "verify.hfs"
    partition.write_bytes(hybrid_partition(image.read_bytes()))
    run("hmount", partition, stdout=subprocess.DEVNULL)
    try:
        listing = run("hls", "-1a", ":", capture_output=True, text=True).stdout.splitlines()
        expected = {APP, SOURCE, *documents}
        if set(listing) != expected:
            raise ValueError(f"unexpected HFS root entries: {listing}")
        extracted = work / "extracted.bin"
        run("hcopy", "-m", f":{APP}", extracted)
        check_application(extracted.read_bytes(), original)
        run("hcopy", "-m", f":{SOURCE}", extracted)
        raw = extracted.read_bytes()
        size, resource_size = struct.unpack_from(">II", raw, 83)
        if raw[65:73] != SOURCE_KIND or resource_size or raw[2:2 + raw[1]] != SOURCE.encode():
            raise ValueError("HFS source archive name, type/creator or forks differ")
        hfs_sha = check_source(source, "HFS view", raw[128:128 + size])
        for name, data in documents.items():
            run("hcopy", "-m", f":{name}", extracted)
            raw = extracted.read_bytes()
            size, resource_size = struct.unpack_from(">II", raw, 83)
            if raw[65:73] != b"TEXTttxt" or resource_size or raw[128:128 + size] != data:
                raise ValueError(f"HFS document content or metadata differ: {name}")
    finally:
        run("humount")
    # The ISO 9660 view, parsed without cdrkit or hfsutils.
    image_bytes = image.read_bytes()
    files = iso_root(image_bytes)
    if SOURCE_ISO not in files:
        raise ValueError(f"ISO 9660 view has no {SOURCE_ISO}: {sorted(files)}")
    start, size = files[SOURCE_ISO]
    iso_sha = check_source(source, "ISO 9660 view", image_bytes[start * 2048:start * 2048 + size])
    print(f"source archive sha256 {hfs_sha} (HFS {SOURCE}) == {iso_sha} (ISO {SOURCE_ISO}) == standalone")


def package(build, output, epoch, source_path, notes_path, verify_only):
    original = (build / "PoodleEarth-Finder.bin").read_bytes()
    check_application(original, original)
    source = source_path.read_bytes()
    notes = notes_path.read_bytes()
    if hashlib.sha256(source).hexdigest().encode() not in notes:
        raise ValueError("source notes do not name the SHA-256 of the source archive")
    documents = {
        name: (build / path if path.parts[0] == "license" else path).read_bytes().replace(b"\n", b"\r")
        for name, path in DOCUMENTS.items()
    }
    documents[NOTES] = notes.replace(b"\n", b"\r")
    with tempfile.TemporaryDirectory(dir=output.parent) as directory:
        work = Path(directory)
        if verify_only:
            verify(output, original, documents, source, work)
            return
        staging = work / "staging"
        staging.mkdir()
        (staging / "PoodleEarth.bin").write_bytes(rename(original, APP, epoch))
        for index, (name, data) in enumerate(documents.items()):
            (staging / f"document{index}.bin").write_bytes(text_macbinary(name, data, epoch))
        (staging / "source.bin").write_bytes(file_macbinary(SOURCE, source, epoch, SOURCE_KIND))
        for path in (*staging.iterdir(), staging):
            os.utime(path, (epoch, epoch))
        stamp = datetime.fromtimestamp(epoch, timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
        image = work / "candidate.iso"
        run("faketime", "-f", stamp, "genisoimage", "-hfs", "-part", "--macbin", "-mac-name",
            "-no-desktop",
            "-V", "POODLE_EARTH", "-hfs-volid", APP, "-o", image, staging)
        # cdrkit leaves the unused driver descriptor tail uninitialized.
        # This non-bootable image has no driver descriptors or MBR boot code.
        with image.open("r+b") as stream:
            descriptor = stream.read(12)
            if not descriptor.startswith(b"ER\x02\x00"):
                raise ValueError("unexpected cdrkit non-bootable driver descriptor")
            stream.write(bytes(512 - len(descriptor)))
        verify(image, original, documents, source, work)
        image.replace(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify-only", action="store_true")
    parser.add_argument("--source", type=Path, required=True, help="the release source archive, embedded as is")
    parser.add_argument("--notes", type=Path, required=True, help="text naming the source revision and SHA-256")
    parser.add_argument("build", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("epoch", type=int)
    args = parser.parse_args()
    if not 0 <= args.epoch <= 0xffffffff - MAC_EPOCH_OFFSET:
        parser.error("epoch outside the supported Classic HFS date range")
    try:
        package(args.build, args.output, args.epoch, args.source, args.notes, args.verify_only)
    except (OSError, ValueError, struct.error, subprocess.CalledProcessError) as error:
        parser.exit(1, f"CD packaging: {error}\n")
    print(f"HFS filenames, documents, source archive, application forks and type/creator verified: {args.output}")


if __name__ == "__main__":
    main()
