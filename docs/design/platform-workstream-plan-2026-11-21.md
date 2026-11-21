# Platform workstream plan — 2026-11-21

**Status:** Proposed cross-release operating plan; draft for owner review. This is neither an Accepted ADR nor a change to release gates, target support, issue readiness or hardware authorization. **Planning tracker:** [#247](https://github.com/jackwillis/poodle-earth/issues/247). **Release authority:** [#212](https://github.com/jackwillis/poodle-earth/issues/212); repository authority: [AGENTS.md](../../AGENTS.md).

## Purpose and boundaries

Continuously improve Poodle Earth's ability to run across historical and contemporary computers **without making experimental portability work a prerequisite for shipping user-visible improvements**. Invest in the existing portable C++20 geographic Model/Msg/update/Effects core, verified data formats and native frontend adapters; prefer thin reusable infrastructure over an unneeded universal GUI layer. Keep native experiences and shared geographic semantics. Offline behavior and constraints of old hardware are product features, not afterthoughts.

Direction is documented separately in **unmerged** [product-decision draft #243](https://github.com/jackwillis/poodle-earth/pull/243). Its target preference is one public `poodle-earth` application per qualified Unix OS/ISA/runtime: FLTK where proven viable, Xlib otherwise; retain Xlib as reference/fallback, not as a routine second customer edition. The current approved v0.1.3 release still uses Xlib. The unmerged [FLTK research #236](https://github.com/jackwillis/poodle-earth/pull/236) and [portfolio model #239](https://github.com/jackwillis/poodle-earth/pull/239) / stacked [coverage model #241](https://github.com/jackwillis/poodle-earth/pull/241) inform options; none certify products. Portfolio costs and reach are currently hypothetical.

## Operating method: evidence-driven platform evolution

This is a **project-specific lightweight synthesis**, not a formal external methodology.

1. **Product-first:** each release has one owner-approved customer outcome. An experiment cannot become a release gate merely by running during that cycle.
2. **Fitness continuously:** preserve known-good core tests, deterministic geographic behavior, bounded format readers, compiler checks and existing target regressions. Select a check because it protects a concrete boundary, not for a matrix trophy.
3. **Small falsifiable experiments:** state decision, baseline/control, hypothesis, fixed effort appetite, dependencies, exact artifact and failure/stop condition. Negative evidence is a valid result.
4. **Options before commitments:** compare alternatives (FLTK/Xlib, Java AWT, native Cocoa, vector-only 68k) without funding or promising every route.
5. **Evidence before promotion:** record target-scoped provenance and qualification; running a compiler is not running an application, and running an app is not accepting a redistributable package.
6. **Owner attention is the bottleneck:** soft WIP limit of **one** platform task needing substantial owner/hardware acceptance. Parallel independent agent research is welcome only when ownership, review and integration do not collide.
7. **No fixed platform quota:** take thin slices when they remove a real blocker or create reusable option value; ship when acceptance is ready.

### Slices and promotion

- **Horizontal slice:** one reusable guarantee (e.g., GCC/Clang portability job, little-endian reader fixture, frontend semantic trace, archive ABI report, reproducible package test), with at least one real consumer and independent verification.
- **Vertical slice:** one complete observable behavior on one precisely named target (e.g., Linux FLTK presents the existing framebuffer and accepts navigation; NetBSD package installs, launches offline and removes; historical JVM executes the same hashed JAR). Prefer thin end-to-end proof to broad unfinished abstractions.

Evidence ladder for each *named OS / version / ISA / ABI / application revision / distribution*:
`hypothesis → source/build-check → linkable binary → native execution → native interaction acceptance → package installation/offline acceptance → owner-approved publication/support`.
Separate recorded observations from qualification decisions. Authentic system evidence, failures, hashes, tool versions, test commands, provenance and limitations accompany promotions. Test claims always name target, environment and revision. The [observability-over-recollection proposal #227](https://github.com/jackwillis/poodle-earth/pull/227) is a related unmerged methods record; capture evidence at the point of observation rather than retrospective estimates.

## Five enduring workstreams

| Workstream | Scope and thin shared capabilities | Existing home |
| --- | --- | --- |
| **A. Portable core and builds** | GCC/Clang and compiler-floor CI; C++20 discipline; endian/atlas fixtures; deterministic test/release builds; narrowly justified CMake improvements | [#3](https://github.com/jackwillis/poodle-earth/issues/3), [#102](https://github.com/jackwillis/poodle-earth/issues/102), [#126](https://github.com/jackwillis/poodle-earth/issues/126), #164 |
| **B. Frontend contracts** | Shared semantic commands, bounded framebuffer/presentation contracts, trace comparison; native window/text/input policies; no obligatory widget parity | [#114](https://github.com/jackwillis/poodle-earth/issues/114), [#183](https://github.com/jackwillis/poodle-earth/issues/183), [#242](https://github.com/jackwillis/poodle-earth/issues/242) |
| **C. Distribution and acceptance** | Archive/package provenance, ABI dependencies, install/offline/launch/remove, exact source licensing and checksums | [#211](https://github.com/jackwillis/poodle-earth/pull/211), [#215](https://github.com/jackwillis/poodle-earth/pull/215), [#216](https://github.com/jackwillis/poodle-earth/pull/216), [#122](https://github.com/jackwillis/poodle-earth/issues/122), [#231](https://github.com/jackwillis/poodle-earth/issues/231) |
| **D. Historical/constrained targets** | Java 1.4 AWT feasibility, vector-only 68k, Tiger/Cocoa, historical Windows and unusual Unix; version-specific negative controls | [#237](https://github.com/jackwillis/poodle-earth/issues/237), [#176](https://github.com/jackwillis/poodle-earth/issues/176), [#178](https://github.com/jackwillis/poodle-earth/issues/178) |
| **E. Evidence and investment** | Minimal experiment record, owner/validation cost observations, target-scoped evidence states, Pareto/option and dependency sensitivity; no fabricated market reach | [#238](https://github.com/jackwillis/poodle-earth/issues/238), [#240](https://github.com/jackwillis/poodle-earth/issues/240), drafts [#239](https://github.com/jackwillis/poodle-earth/pull/239)/[#241](https://github.com/jackwillis/poodle-earth/pull/241), [#227](https://github.com/jackwillis/poodle-earth/pull/227) |

These are **views of existing issues**, not five new epics or separate standing ceremonies. Issue ownership remains with each existing record.

## Actual milestone and release mapping (snapshot 2026-11-21)

Based on the milestone objects attached to the repository's issue inventory, including closed issues; **the milestone collection endpoint was unavailable**, so an empty, issue-free milestone cannot be ruled out. The owner-approved [#212](https://github.com/jackwillis/poodle-earth/issues/212) roadmap governs release intentions even when milestone metadata lags.

| GitHub milestone | Recorded status | Platform relationship |
| --- | --- | --- |
| v0.1 (#1), v0.1.1 (#6) | closing; shipped | Historical Classic foundation and acceptance. No new platform tasks. |
| **v0.1.2 (#7)** | **current** (metadata), although public v0.1.2 release shipped November 21 | **Drift to release steering**, not platform-plan authority to change status or close residual issues. |
| **v0.1.3 (#8)** | planned | **Release commitment:** Linux x86_64 Xlib archive + one native-tested NetBSD/pkgsrc package for an exact architecture, with ABI, offline and matching-source evidence; #211/#215/#216 plus supporting #230/#231 per #212. FLTK **non-blocking**. |
| **v0.1.4** | Owner-approved roadmap theme; no milestone confirmed from issue inventory | **PoodleScripts** #221/#232; optional platform experiment may run alongside, but cannot redefine this release. |
| v0.2 (#2) | planned | Better atlas / core and frontend semantic improvements where independently useful; no new port promise. The older milestone description is not a replacement for #212's current direction. |
| v0.2.1 (#5) | planned | Faster and firmer: compiler floor, performance/heap/hardware evidence; a natural home for approved fitness work, not an automatic gate for unapproved ports. |
| v0.3 (#4) | planned | Memory and packs; resource accounting and portable data handling only where the scoped work is accepted. |
| v0.4 (#3) | planned | Expandable atlas and later Unix/regional pack packaging research (#122); no automatic external-pack commitment for v0.1.3. |

**Scheduling labels are conceptual, not new GitHub labels:** (1) **release commitment** is owner-approved and release-blocking; (2) **opportunistic slice** has value but cannot block the release; (3) **independent experiment** informs a future decision, ideally unmilestoned when milestone counts would imply a promise. A release record can link optional work under “platform improvements” without assigning it to the milestone. Only the owner can promote any optional work into required release scope.

## Bounded queue and dependencies

1. **First, finish the already-committed v0.1.3 distribution acceptance path (C).** Reconcile #211 → #215/#216 drafts, pin Linux ABI/runtime baseline, select NetBSD release/ISA, execute native pkgsrc install/launch/remove, then matching-source CD #231 and hygiene #230. Packaging branches are not proof of native acceptance; this work belongs to release steering rather than an additional platform experiment.
2. **Next optional experiment: FLTK-01, Linux-only (B).** Resume existing **blocked** #183 / draft #184 after a pinned FLTK 1.4 development environment and working local X11/Xvfb are available. Build an opt-in framebuffer frontend without portable core change; compare matched navigation, selection, resize, startup, RSS, framebuffer copies and presentation with Xlib. Reproducible artifact or documented block is the deliverable. **Stop:** missing dependencies or invalid output beyond bounded effort; do not expand into NetBSD, packaging, universal UI or a public frontend switch. Draft #236 informs graph assumptions.
3. **Then PE-JAVA-00 (D).** [#237](https://github.com/jackwillis/poodle-earth/issues/237): original positive/negative Java 1.4 class/API tests, pinned FOSS toolchain and authentic historical JVM execution if separately available/authorized. **Stop:** if credible rights/toolchain/API evidence or actual runtime cannot be obtained, report inconclusive/no-go; no Reader implementation.
4. **Later bounded 68k vector proof (D).** Propose a target-specific emulator/CPU/RAM budget and minimal synthetic vector-coastline rotation using QuickDraw. No full engine, shaded raster, modern globe feature parity or 68000 support promise; new authorization required before implementation.

**Shared dependencies:** A core/format confidence supports B and C; B can reuse semantic traces from #114; C's ABI, provenance and native proof practice informs every target. D experiments may reuse fixture/test *contracts* without requiring an identical renderer. E captures actual owner validation and integration costs after each task; do not wait for calibration to complete v0.1.3.

## Experiment record (in the owning issue)

Record **question and decision enabled; exact baseline, target and revision; evidence already held; dependencies; bounded artifact; agent effort plus owner/hardware attention appetite; falsifiable success and stop conditions; explicit exclusions; full commands, hashes, logs and provenance; and follow-up owner decision**. If not measured, mark unknown. A negative experiment may complete without a new implementation issue.

## Ownership, review and maintenance

[#247](https://github.com/jackwillis/poodle-earth/issues/247) owns this lightweight cross-release index; individual issues and PRs own experiments, code, measurements and acceptance. Revisit when a release theme changes (#212), a target passes or fails a native gate, a dependency/toolchain version materially changes, a platform is proposed for public support, or actual owner validation/maintenance costs overturn the current preferred order. Check the queue during existing release planning; **no standing meeting, fixed quota, shadow status matrix or duplicate epic**. Owner controls priorities, hardware, Accepted ADRs, merges and promotion to release gates per AGENTS.md.

**Next single platform engineering assignment (proposed, not granted here):** after v0.1.3 release-critical work has a clear acceptance path, authorize one Linux-only FLTK-01 qualification slice under existing #183. Start by pinning FLTK 1.4 and validating the optional target from draft #184 against Xlib; stop on reproducible dependency/runtime blocker. Agent may prepare host-only evidence once #183 is explicitly assigned/ready; no G4 access, milestone change or support claim.
