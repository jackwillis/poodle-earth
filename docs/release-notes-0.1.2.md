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

See the [v0.1.2 GitHub release](https://github.com/jackwillis/poodle-earth/releases/tag/v0.1.2)
for downloadable artifacts and matching source. Linux and NetBSD use the
separate source-build X11 frontend, not a published Unix binary package.

## Verification and limitations

Country labels merged in [PR #185](https://github.com/jackwillis/poodle-earth/pull/185),
with host tests, X11 interaction checks, Retro68 cross-build and packaging
checks. The owner first accepted the appearance of an earlier country-label
build (`b7aa82a`) on a Power Mac G4 running bare Mac OS 9.2.2.

The owner subsequently accepted **the actual v0.1.2 release artifacts** built
from commit `68e40b3`: the StuffIt archive passed the native installation,
launch, label-toggle/settled-frame and interaction smoke checks. A CD-R burned
from the release's hybrid ISO mounted on the G4, and the Finder-copied app
launched and reported version 0.1.2 ([native acceptance record](https://github.com/jackwillis/poodle-earth/issues/119#issuecomment-7066176502)).
The checksums of the published release downloads were also verified
([publication record](https://github.com/jackwillis/poodle-earth/issues/119#issuecomment-7066217800)).

No numeric G4 timings were recorded; the formal timing comparison and
font-removal tests were waived. Native Classic glyph and shadow ink bounds
remain an accepted, unverified limitation. These native results do not
certify every operating-system version or emulator.

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
The distributed DataCredit title still says “0.1”; the owner accepted this
cosmetic discrepancy for v0.1.2, and the attribution text remains accurate.
