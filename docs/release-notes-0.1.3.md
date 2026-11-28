# Poodle Earth v0.1.3 — Release notes

Published November 27, 2026 from commit `6194516ee2e00e9666dc992f87bf6e7d56e3188e`. Download the files and `SHA256SUMS` from the [GitHub release](https://github.com/jackwillis/poodle-earth/releases/tag/v0.1.3).

## What's new

- City names appear once the globe settles. Zoom in to see more names. Country labels move to avoid city dots and are omitted when no clear position fits.
- Linux x86-64 and NetBSD 11.1/amd64 and macppc have downloadable X11 builds.
- The Classic hybrid CD includes the same corresponding source archive in its HFS and ISO 9660 views.

## Downloads and verification

| Platform | Download and installation | Verification |
| --- | --- | --- |
| PowerPC Classic Mac OS | Expand the `.sit` with StuffIt Expander, or mount the hybrid CD and copy the application and accompanying files with Finder. | Owner-confirmed on a Power Mac G4 running Mac OS 9. |
| Linux x86-64 | Extract the portable archive and follow `INSTALL.txt`. Requires X11 and glibc 2.34 or newer. | Built on Debian 12 and smoke-tested under Xvfb. |
| NetBSD 11.1 amd64 | Install the amd64 `.tgz` with `pkg_add` and launch `poodle-earth` under X11. | The NetBSD guest installed, tested, and removed the published package. |
| NetBSD 11.1 macppc | Install the macppc `.tgz` with `pkg_add` and launch `poodle-earth` under X11. | Owner-confirmed install, launch, removal, and package database checks on the G4. |

The NetBSD downloads are pkgsrc-format binary packages assembled on Linux. The Unix packages are separate downloads. The hybrid CD contains the Classic application and its source.

## Source and checksums

The Classic and Linux source archives have distinct filenames and packaging layouts. On the Classic CD, look for `Source.tar.gz` in Finder or `SOURCE.TGZ` in the ISO view. Both match the standalone Classic source archive. The source revision and artifact hashes are in the [candidate record](https://github.com/jackwillis/poodle-earth/issues/212#issuecomment-7227513096). See the [native acceptance record](https://github.com/jackwillis/poodle-earth/issues/212#issuecomment-7230962827) and the [published asset verification](https://github.com/jackwillis/poodle-earth/issues/212#issuecomment-7231304862).
