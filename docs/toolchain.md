# Toolchains and validation — 2026-11-02

## Canonical build-host and release workflow

**Owner policy (2026-11-21): [AGENTS.md](../AGENTS.md#5-safety-and-invariants-kept-from-claudemd-and-docshandoffmd) is authoritative.**
All compilation, including compiler/tool builds and Linux builds, happens on **Linux amd64**.
Other operating systems and architectures are **cross-targets**. NetBSD/amd64 VMs,
NetBSD/macppc on the G4, Classic Mac OS and other target environments are for
native package/runtime/GUI acceptance, **not native compilation**, even when a
package manager normally builds from source. For Linux amd64 itself, local host
builds and tests are allowed; they are not cross-compilation.

**Reproducible GitHub Actions on Linux amd64 is the canonical release-build path.**
Pin source, toolchains, target sysroots, flags, dependencies and output hashes;
verify a clean cold-cache build, and use caches only to save time. Publish only
the exact approved CI artifact after native acceptance. Local experimental
cross-builds, even ones accepted on hardware, are evidence pending CI integration.
The existing CI does not yet produce every planned Unix release artifact;
do not claim that it does. See [the release checklist](release-checklist.md)
and the proposed [PE-CI-01 strategy](https://github.com/jackwillis/poodle-earth/pull/250).

| Target | Compiler / method | Current evidence |
| --- | --- | --- |
| Host GCC | Fedora GCC 16.2.1, C++20, ASan/UBSan, no exceptions/RTTI | 35/35 tests, including both Xvfb depths and quiet idle |
| Host clang | clang 22.1.8, C++20, no exceptions/RTTI | 35/35 tests |
| NetBSD/macppc G4 | Local cross GCC 12.5.0, -O2 -static for headless tests, scratch sysroot | App/terrain tests pass; replay hash and deliberate terrain pixels agree with host; measured base/refine costs |
| Mac OS 9 PowerPC | Retro68 GCC 16.1.0, -Os, classic non-Carbon Toolbox | Links and HFS packaging/fork checks pass; launch/render and scheduling smoke checks under Tiger Classic; native OS 9 acceptance open |
| 68k | Retro68 GCC 16.1.0 | Portable core compile probe only; no application/runtime feasibility claim |

The current suite is enumerated by `ctest --test-dir build -N`; avoid treating historical counts as
current. The initial three-test bootstrap, original hardware runs and feature-probe history are
[archived](history/toolchain-bootstrap-20261101.md). `tools/probe-retro68.sh` guards the C++20 subset
on both classic toolchains after portable changes. No exceptions/RTTI, OS headers or clock reads
belong in the portable core; Retro68 formatting lacks %ll/%z. One reviewed text-trace formatter is
the portable stdio exception. Platform-specific sources use their actual cross-build for checking.

Host checks use CMake/Ninja, clang-format 22.1.8 and clang-tidy 22.1.8. Host lint excludes OS 9-only
sources; the actual Retro68 application build checks those against installed Multiversal interfaces.
The CI workflow is configured for GCC, clang, portability and edition size reports. Historical
run evidence is recorded in [docs/ci-issue-7.md](ci-issue-7.md); check GitHub Actions for the
current result rather than treating a dated report as a current status. Publishing is the owner's action.

Never compile on the G4. Cross-build locally, push under a new name, restore its executable bit and
run headless. Owner binaries tip3–tip7 and desktop display permissions are untouched. Earlier X11
builds were owner-tried; this pass claims headless correctness/performance and private host displays,
not real G4 submission/scanout, current GUI acceptance or OS 9 execution. See [terrain](terrain.md),
[scaling](perf/scaling.md), [size reports](floppy-size.md) and [OS 9 acceptance](os9-proof.md).

The [2026-11-04–05 scheduling audit](os9-scheduling.md) records current GCC host (40/40), focused sanitizer, Retro68 compile and Tiger Classic evidence; counts above describe earlier runs.
