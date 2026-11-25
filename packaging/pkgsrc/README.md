# NetBSD pkgsrc recipe: geography/poodle-earth

Draft for v0.1.3 (release plan #212, Refs #122). Packages the X11 Reader with embedded data, from the
GitHub release tarball of the tag `v${PKGVERSION}`. Needs the install rules from #209 and the
`poodle-earth` executable and doc directory name from #211.

## Status

| Item | State |
|---|---|
| Recipe (`Makefile`, `DESCR`, `PLIST`) written against current pkgsrc conventions | done |
| `distinfo` | **pending**: needs the published tag tarball; generate at release with `make makesum` |
| Version `0.1.3` | placeholder; set `DISTNAME` to the tag the owner cuts |
| `pkglint`, `make package`, `pkg_add`, `pkg_delete` on NetBSD 11.1/amd64 | passed on a local VM (#212); CI job `NetBSD pkgsrc` runs the same steps (see below) |
| NetBSD/macppc CI | **none**: no pkgsrc `make package` and no hosted macppc runner; see below |
| NetBSD/macppc (G4) build, install, launch | **pending**, owner-controlled hardware |
| Any other NetBSD architecture | not attempted |

## CI

`.github/workflows/netbsd-pkgsrc.yml` runs `tools/netbsd-pkgsrc-acceptance.sh` in a NetBSD 11.1/amd64
VM (`vmactions/netbsd-vm`, pinned by commit; pkgsrc-2026Q3 tarball with a pinned SHA-512): `makesum`,
`pkglint`, `make package`, PLIST check, `pkg_add`, headless readback oracle at depth 24 and 16,
`pkg_delete`, residue check, `pkg_admin check`. It proves package correctness only: no real display,
no timing, no hardware. `distinfo` there comes from a `git archive` of the PR, not a tag tarball.

### NetBSD/macppc: CI state (2026-11-22)

There is no macppc CI. The G4 is owner-controlled hardware (no runner, no compilation on it) and
QEMU/macppc is deferred. The macppc evidence is manual: a Linux cross-built binary (NetBSD 11.1
sysroot) wrapped by a Linux-built `pkg_create` into a pkgsrc-format package, accepted on the G4 with
owner-run root steps (#212). It is **not** a pkgsrc `make package` build and must not be described as
one. Reproducible hosted cross-build and assembly of that package is future work (#212, #250).

## Try it (once a tag and `distinfo` exist)

```sh
cp -R packaging/pkgsrc/geography/poodle-earth /usr/pkgsrc/geography/   # or into pkgsrc-wip
cd /usr/pkgsrc/geography/poodle-earth
make makesum            # writes distinfo from the tag tarball (release step)
pkglint -Wall           # from pkgtools/pkglint
make package            # builds on the target; do not build on the shared G4 without the owner
make install
poodle-earth            # needs an X display
```

Compiler: the sources are C++20. pkgsrc asks for GCC 12 or newer (`USE_CXX_FEATURES=c++20`); on
a NetBSD whose base compiler is older this pulls in `lang/gcc12`, which takes a long time to build on
a G4. Prefer a binary `gcc12` package or a cross build if the owner wants macppc evidence.

## Removal

```sh
pkg_delete poodle-earth      # or: make deinstall
```

Removes `bin/poodle-earth` and `share/doc/poodle-earth/`; nothing else is written (no settings or
caches, #49).

## Licences and data

`LICENSE=gnu-gpl-v3 AND cc0-1.0-universal AND cc-by-v4.0 AND public-domain`. The program is
GPL-3.0-or-later. The data embedded in the executable is separate (Natural Earth: public domain;
ETOPO 2022 and Wikidata: CC0; RESOLVE Ecoregions: CC BY 4.0, per `data/DATA-CREDITS.txt` and
`docs/licensing-policy.md`). The notices are installed in `share/doc/poodle-earth/`. Whether pkgsrc's
`LICENSE` should list the data licences is a packaging-policy question for the owner and pkgsrc reviewers.

## Owner acceptance checklist (not done)

- [ ] Choose the architecture(s) to claim (macppc preferred) and the NetBSD release.
- [ ] Cut the tag, publish the tarball, then run `make makesum`; commit `distinfo`.
- [ ] `pkglint -Wall` clean; `make package` ok on the target (record `uname -a`, compiler, pkgsrc branch).
- [ ] Install, launch on a real X display, drag the globe; `pkg_delete` leaves nothing behind.
- [ ] Decide where the recipe lives (pkgsrc-wip, or a pkgsrc PR) and who is MAINTAINER.
