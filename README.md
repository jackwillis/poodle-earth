# Poodle Earth

**An offline, software-rendered globe for PowerPC Classic Macs.** Turn the Earth, explore countries and cities, and see the world from another angle. Poodle Earth is free software; a Unix/X11 frontend can also be built from source.

**[Download Poodle Earth v0.1.2](https://github.com/jackwillis/poodle-earth/releases/tag/v0.1.2)** · [Website](https://jackwillis.github.io/poodle-earth/) · [User guide](docs/user-guide.md) · [Changelog](CHANGELOG.md)

![Poodle Earth on a Classic Mac desktop, showing the Map look and information sidebar](site/images/os9-hero-map.png)

*Captured on a Power Mac G4 running bare Mac OS 9.2.2 on November 17, 2026 (development build `110a197`). This screenshot predates the country-label feature in v0.1.2. [Capture provenance](docs/images/os9/README.md).*

## Features

- **Explore the globe:** drag to rotate, zoom and recenter, or navigate with the keyboard.
- **Choose a map look:** switch between biome-colored **Map** and elevation-shaded **Relief**.
- **Discover places:** select countries and cities to view information cards; country names appear when the globe settles and can be toggled off.
- **Explore overlays:** show country borders, the latitude/longitude grid and the day/night boundary.
- **Measure distances:** calculate great-circle distances and bearings, in metric or imperial units.
- **Find your bearings:** use the compass rose, geographic scale, native menus and in-app controls help.
- **Work offline:** the compact atlas is built in, and the application needs no network access.

## Download and platforms

The published application is for **PowerPC Classic Mac OS**. The [v0.1.2 release](https://github.com/jackwillis/poodle-earth/releases/tag/v0.1.2) includes a StuffIt archive (`.sit`), a hybrid HFS/ISO CD image (`.iso`), matching source and checksums.

| Platform | Availability |
| --- | --- |
| **Classic Mac OS (PowerPC)** | Published binary. The exact v0.1.2 StuffIt archive and CD image were tested on a Power Mac G4 running bare Mac OS 9.2.2. |
| **Earlier PowerPC Mac OS** | Earlier development builds were smoke-tested in emulation back to System 7.1.2; this is not blanket certification of v0.1.2 on those systems. |
| **Linux and NetBSD (X11)** | Separate frontend available from source; no published Unix binary packages yet. |

On a Classic Mac, expand the `.sit` with StuffIt Expander. Alternatively, mount the ISO as a CD in a compatible PowerPC environment. **Copy the application and accompanying files using Finder** so its resource fork and Finder information survive.

The Classic application has a **24 MB preferred / 12 MB minimum** application-memory setting. It is PowerPC-only, not a 68k or native Mac OS X application. See the [user guide](docs/user-guide.md) for installation steps, controls and troubleshooting, and the [v0.1.2 release notes](docs/release-notes-0.1.2.md) for tested behavior and limitations.

## Build from source

On Linux or NetBSD, install a C++20 compiler, CMake, Ninja, Python 3 and X11 development libraries (Xvfb is used for display tests), then run:

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/globe-x11
```

The last command requires an X display. Classic Mac builds use [Retro68 and separate tooling](docs/toolchain.md). See [CONTRIBUTING.md](CONTRIBUTING.md) for setup, tests and pull-request guidance; frontend controls differ across operating systems.

## Architecture

Poodle Earth has a portable **C++20 core** organized around an Elm-inspired `Model / Msg / update / Effects` loop. A CPU software renderer draws the globe progressively. Native frontends present it through the Classic Macintosh Toolbox/QuickDraw or X11.

Modern-host tools prepare geographic data as compact, validated resources; the Classic Mac application does not need Python, a GPU or an online service. More detail is in the [architecture overview](docs/architecture.md), [atlas production guide](atlas/README.md) and [documentation index](docs/README.md).

## Roadmap

- **v0.1.3 — First Unix distribution:** work toward tested NetBSD pkgsrc and Linux x86-64 packages for the existing X11 frontend.
- **v0.2 — The Atlas on the Map:** clearer labels and discovery, richer information about places, and more coherent geographic navigation.
- **Later:** richer offline geography and regional editions, resource management, and additional platforms where practical and tested.

Plans may change. [Milestones](https://github.com/jackwillis/poodle-earth/milestones) and [issues](https://github.com/jackwillis/poodle-earth/issues) are the current roadmap, not a promise that in-progress features are available.

## Inspiration

Poodle Earth draws inspiration from classic Macintosh atlases and educational software, the portability of [NetBSD](https://www.netbsd.org/), geographic explorers such as [KDE Marble](https://marble.kde.org/) and [NASA WorldWind](https://worldwind.arc.nasa.gov/), and offline knowledge projects such as [Kiwix](https://kiwix.org/). It is an independent project, learning from the work of many others.

## Geographic data and license

The built-in atlas uses **[Natural Earth](https://www.naturalearthdata.com/)** (coastlines and boundaries), **[NOAA NCEI ETOPO 2022](https://www.ncei.noaa.gov/products/etopo-global-relief-model)** (elevation and bathymetry), **[Wikidata](https://www.wikidata.org/)** (places) and **[RESOLVE Ecoregions](https://ecoregions.world/)** (adapted biome classes).

Data have been simplified for older computers: the current atlas contains 304 city points, coarse terrain and generalized borders; small islands, some countries and many cities are missing. There are no street maps. Boundaries are cartographic references, not statements about political status. See the [complete data credits and license notices](data/DATA-CREDITS.txt), [source provenance](data/SOURCES.md) and [data licensing policy](docs/licensing-policy.md).

Poodle Earth is free software under the **GNU GPL, version 3 or later**. Geographic datasets retain their own licenses. Bug reports, geographic corrections and code contributions are welcome; start with [CONTRIBUTING.md](CONTRIBUTING.md).
