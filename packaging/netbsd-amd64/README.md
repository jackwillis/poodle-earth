# NetBSD 11.1/amd64 package, built on Linux and tested in a NetBSD guest

Refs #212, #216, #278. This is the amd64 counterpart of `packaging/netbsd-macppc/`. It is **not** a pkgsrc
`make package` build (that is the recipe in #216, which builds inside the guest). The package is
**pkgsrc-format, assembled on Linux** by a `pkg_create` built from the pinned NetBSD 11.1 sources, from an
executable cross-built on Linux; `+BUILD_INFO` says so.

## How it reuses the macppc scripts

The five scripts in `packaging/netbsd-macppc/` (tools, sysroot, sysroot check, X11 build, package) take
their port-specific values from optional environment variables. The defaults are the macppc values, so the
macppc workflows are unchanged; `.github/workflows/netbsd-amd64-package.yml` sets the amd64 values.

| Variable | macppc (default) | amd64 |
| --- | --- | --- |
| `NETBSD_MACHINE` | `macppc` | `amd64` |
| `NETBSD_TRIPLET` | `powerpc--netbsd` | `x86_64--netbsd` |
| `NETBSD_SETS_PINS` | `netbsd-11.1-macppc-sets.sha512` (`.tgz`) | `packaging/netbsd-amd64/netbsd-11.1-amd64-sets.sha512` (`.tar.xz`) |
| `X11_TOOLCHAIN_FILE` | macppc toolchain file | `packaging/netbsd-amd64/netbsd-amd64-x11-toolchain.cmake` |
| ELF expectations | ELF32, big endian, PowerPC, EXEC | ELF64, little endian, X86-64, EXEC or DYN |
| package `MACHINE_ARCH` | `powerpc` | `x86_64` |

## Jobs

1. **build** (clean `ubuntu-24.04`, no cache): `build.sh -m amd64 tools` from the pinned source sets; the
   sysroot from the pinned `base`, `comp`, `xbase`, `xcomp` sets; cross-build and inspect
   `poodle-earth-x11` (NetBSD 11.1.0 note, `ld.elf_so`, NEEDED, RPATH, no runner paths); assemble the
   package twice and require identical bytes; record the SHA-256. Artifact `netbsd-11.1-amd64-package`
   (package, `SHA256SUMS`, `PLIST`, `buildinfo`, logs, `provenance.md`).
2. **guest** (`vmactions/netbsd-vm`, NetBSD 11.1 amd64): download that artifact, check the SHA-256 on the
   runner and again in the guest against the build job's output, `pkg_add`, library check, the repository's
   X11 pixel test (`tests/test_x11_pixels.py`, the same arguments as the `x11_pixels_*` CTest) under Xvfb at
   depth 24 and 16, `pkg_delete`, residue check and `pkg_admin check`. Evidence artifact
   `netbsd-11.1-amd64-guest-evidence`.

## Pinned and unpinned

Pinned by SHA-512, each checked against the digest file of two NetBSD hosts at run time: the four source sets
and the amd64 `base`, `comp`, `xbase`, `xcomp` sets (four hosts agreed on 2026-11-25: `cdn.netbsd.org`,
`ftp.netbsd.org`, `mirror.planetunix.net`, `ftp.nluug.nl`; NetBSD publishes no signature, see
`packaging/netbsd-macppc/PROVENANCE.md`). In the guest, `xbase`, `xcomp` and `xserver` are pinned by SHA-512
(`netbsd-11.1-amd64-guest-sets.sha512`, repeated in the workflow's prepare step).

**Actions** are pinned to full commit SHAs (`actions/checkout` v4.4.0, `upload-artifact` v4.6.2,
`download-artifact` v4.3.0, `vmactions/netbsd-vm` v1.5.3). `vmactions/netbsd-vm` has no image-ID or checksum
output; it downloads `anyvm.py` v0.7.5 and the `anyvm-org/netbsd-builder` v2.2.9 `netbsd-11.1.qcow2.zst` image
at run time. The workflow's "Record and verify the guest image provenance" step hashes what the runner kept
and compares it with the GitHub release-asset digests written in the workflow; the result is
`guest-image-provenance.txt` in the guest evidence (a file that is no longer on the runner is reported as
not retained, not as verified).

**Not pinned:** the `ubuntu-24.04` image and its apt packages (cmake, ninja, Python, libarchive, binutils,
GCC; versions are recorded in `provenance.md`); the libarchive and zlib the host `pkg_create` links; the
`python313` binary package from the NetBSD 11.1 repository; the image contents beyond the digests above are
whatever NetBSD 11.1 that image holds (`uname` is recorded and must say 11.1 amd64).

## Reproducibility limits

Two `pkg_create` runs in one job are byte-identical (clock and group database fixed by `pe-epoch.so`,
`SOURCE_DATE_EPOCH` from the source commit time). Byte identity across runs needs the same source commit and
is not claimed across different runner images or libarchive/zlib versions. The executable is built with
`-O2 -DNDEBUG`; its cross-run identity is not established here.

## Not shown

A real display, timing or performance, a pkgsrc `make package` amd64 package from this recipe path (see
#216), any other NetBSD release, release version alignment (CMake says 0.1.2, the package says 0.1.3, as
for macppc), and anything on the G4.
