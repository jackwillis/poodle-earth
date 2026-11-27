# Mac OS 9 CD image: installation and test evidence (#171)

The Linux release packager stages `PoodleEarth-<version>-<revision>-hfs.iso`
alongside the StuffIt archive and corresponding source. The version comes from
CMake and the revision from Git. The image is a non-bootable ISO-9660/classic HFS
hybrid with 2048-byte optical sectors and an Apple Partition Map. The Macintosh
view contains Poodle Earth (both forks, `APPL` / `PdlE`), ReadMe, two license
parts, DataCredit, `Source Notes` and `Source.tar.gz`. It contains no system software.

`Source.tar.gz` is the release's complete corresponding source: the same file,
byte for byte, as `PoodleEarth-<version>-<revision>-source.tar.gz` published
beside the image (#231). `tools/package-release.sh` makes that archive once,
before the disc, and hands the file to `tools/package_cd.py`; the disc never
carries a second archive. `Source Notes` names the revision and the archive's
SHA-256. The ISO 9660 view has the same bytes as `SOURCE.TGZ;1` (cdrkit's
8.3 name for it). An offline reader
can copy either file and verify it against the published SHA-256. Its Finder type
and creator are `Gzip` / `SITx`, so StuffIt Expander opens it. The StuffIt
download does not contain the source.

cdrkit's `genisoimage -hfs -part --macbin` decodes MacBinary into the HFS
catalog and shares payload extents with the ISO view. The ISO view is accessible
with standard ISO readers, while Finder needs the HFS view to copy the
application with its resource fork. The build suppresses cdrkit's empty Desktop
files to keep the image compact. [cdrkit options](https://manpages.debian.org/testing/genisoimage/genisoimage.1.en.html)

ClassicMac's Tools script uses a classic HFS partition in an Apple-partitioned
raw disk image. Its `.iso` name and integration with ClassicMac do not establish
compatibility with other virtual optical drives. A raw APM/HFS alternative would
be more clearly labeled `.img`. The hybrid keeps the `-hfs.iso` suffix and
gives ISO readers a conventional filesystem. No ClassicMac source was copied.
[ClassicMac Tools script](https://github.com/amcchord/ClassicMac/blob/main/scripts/build-guest-cd.sh)

## Attach and copy

In a PowerPC emulator with a working Mac OS 9 guest, attach the image to the
virtual **CD-ROM** drive. For QEMU, add `-cdrom /path/to/PoodleEarth-…-hfs.iso`
to the existing guest command if its CD drive is available. In a GUI, select the
image in the virtual CD drive's media picker. In Finder, open the Poodle Earth
volume, drag **Poodle Earth** to a folder on the writable Macintosh hard disk,
and open the copied application. Use the Finder copy so both forks travel
together. See the [user guide](user-guide.md) for the StuffIt alternative and controls.

## Earlier CD compatibility checks (November 19, 2026)

PR #172 test image:

- File: `PoodleEarth-PR172-2074034-local-hfs.iso`, 1,210,368 bytes.
- Source: `20740340c939bb5eda5185f5f288650db50850ee`.
- SHA-256: `1f4326ce52904f1f152f1d35ecca958974b7ed5cd5ce13bf1931913efb538021`.
- **Physical Power Mac G4, bare Mac OS 9.2.2:** CD-R burned with xorriso 1.5.8.pl02;
  the application, documents and Get Info worked. Memory settings were read-only
  on CD until the app was copied to the hard drive.
- **ClassicMac 4.7.0, emulated Power Mac G4, Mac OS 9.2.1:** The image mounted
  as a CD; the app launched from the CD and after copying to the guest desktop.

Detailed [physical Mac](https://github.com/jackwillis/poodle-earth/issues/171#issuecomment-7001544331)
and [emulator](https://github.com/jackwillis/poodle-earth/issues/171#issuecomment-7002974798)
test records are available on #171. Other emulators and guest OS versions were
not tested.

The [v0.1.1 release](https://github.com/jackwillis/poodle-earth/releases/tag/v0.1.1) publishes the hybrid ISO alongside StuffIt,
matching source, the release manifest and SHA-256 checksums. Use the published
checksums to verify the release image.

## v0.1.2 CD test (November 21, 2026)

The [v0.1.2 ISO](https://github.com/jackwillis/poodle-earth/releases/tag/v0.1.2) (SHA-256 `4e9b82304743902a63af2c02db8e6d006776e6d02eb081ce79d08f930acefb3a`) was burned to CD-R and checked by read-back. On a physical Power Mac G4 running Mac OS 9.2.2, the disc mounted and the Finder-copied application launched with About showing 0.1.2.

## Build and verify on Linux

From a clean checkout and with the release workflow's Retro68 and Rust images:

```sh
sudo apt-get install podman unar genisoimage faketime hfsutils
tools/build-os9.sh --floppy
GLOBE_RELEASE_STRICT=1 tools/package-release.sh build-os9-floppy
(cd build-os9-floppy/release && sha256sum -c SHA256SUMS)
python3 -m unittest discover -s tests -p test_package_cd.py -v
epoch=${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct)}
src=$(echo build-os9-floppy/release/PoodleEarth-*-source.tar.gz)
python3 tools/package_cd.py --source "$src" --notes build-os9-floppy/source-notes.txt \
  build-os9-floppy /tmp/poodle-earth-rebuild.iso "$epoch"
cmp build-os9-floppy/release/PoodleEarth-*-hfs.iso /tmp/poodle-earth-rebuild.iso
```

The packager checks the ISO descriptor, partition map, and classic HFS signature,
then uses hfsutils to read back the directory and MacBinary forks independently
of cdrkit. It also reads the source archive back from both views, the HFS view
with hfsutils and the ISO 9660 view with a small reader of its own (not cdrkit),
and refuses the image unless both equal the standalone archive. CI repeats that
comparison with `isoinfo` and `hcopy` (the `Extract the source archive...` step). To repeat the structural verification on an existing file, add
`--verify-only` to the Python command and pass that image's path (the source
archive and notes it was built with are required, so the check also proves the
image carries that archive). To inspect the
ISO view separately, run `isoinfo -d -i IMAGE` and `isoinfo -f -i IMAGE`.

The staging dates and cdrkit clock use `SOURCE_DATE_EPOCH` (default: commit
time) in UTC. The packager clears the unused driver descriptor bytes left
uninitialized by cdrkit. CI compares two builds from the same input binary, text and epoch.
Expect identical output within the same Linux toolchain. Cross-version cdrkit
reproducibility and fresh Retro68 binary reproducibility are separate checks.
`RELEASE.json` records revision, epoch, byte counts and SHA-256, and
`SHA256SUMS` includes the CD image.
