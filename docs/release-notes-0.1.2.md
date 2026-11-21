# Poodle Earth v0.1.2 — release notes

**November 21, 2026**

## Changes since v0.1.1

- Country names appear after the globe settles. Toggle them using
  **View → Country Names** on Classic Mac OS or the F2 tools window on X11.
  Labels do not appear while dragging or zooming.
- The overlay uses installed fonts; Classic selects Helvetica first, with
  Palatino as fallback. Country label positions and priorities come from
  the pinned Natural Earth data. No fonts or network data are bundled.

## Platforms and installation

The public binary is the PowerPC Classic Mac OS application. Its release
includes a StuffIt archive (`.sit`) and a hybrid HFS/ISO CD image. Expand
the StuffIt archive on the Mac, or mount the CD image in a compatible
PowerPC environment and copy the application to a writable volume using
Finder, preserving its resource fork. The source archive, release manifest
and SHA-256 checksums are provided with the distribution.

See the [GitHub releases](https://github.com/jackwillis/poodle-earth/releases)
for downloadable artifacts and matching source. Linux and NetBSD use the
separate source-build X11 frontend, not a published Unix binary package.

## Verification and limitations

Country labels merged in [PR #185](https://github.com/jackwillis/poodle-earth/pull/185),
with host tests, X11 interaction checks, Retro68 cross-build and packaging
checks. The owner accepted the appearance of an earlier country-label
build (`b7aa82a`) on a Power Mac G4 running bare Mac OS 9. No numeric
G4 timings were recorded; the formal timing comparison and font-removal
tests were waived. Native Classic glyph and shadow ink bounds remain an
accepted, unverified limitation. This historical test is not a claim
that the final 0.1.2 distribution was run on the G4.

The compact atlas retains 1:110-million-scale boundaries, one-degree
terrain and 304 city points. Country labels may extend beyond borders
or be omitted in crowded views; each country has one fixed anchor and
may lose its name near the limb. City labels, new geographic data packs
and new operating-system packages are not included in v0.1.2.

The release's `RELEASE.json` records the exact corresponding source
revision and artifact hashes; `SHA256SUMS` verifies the downloaded files.
Cross-build and packaging checks do not certify native execution.

License: GPL-3.0-or-later. Geographic data attributions are in the
[DataCredit](../data/DATA-CREDITS.txt) file and the About window.
