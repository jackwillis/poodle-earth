# Platform product direction and decision register — 2026-11-21

**Status:** Owner-stated product preferences and provisional investment strategy. **Not** an Accepted ADR, certified target matrix, new release gate, authorization to implement unassigned ports, or a replacement for the release roadmap. This document records decisions at the *direction* level, not assertions that a platform works.

**Source of truth for release scope:** [#212](https://github.com/jackwillis/poodle-earth/issues/212). **Source of truth for compatibility evidence:** target-scoped tests and issue/PR records. **Research:** [71-scenario platform RICE snapshot](../research/platform-rice-2026-11.md), [FLTK dependency/refinement draft #236](https://github.com/jackwillis/poodle-earth/pull/236), [portfolio model draft #239](https://github.com/jackwillis/poodle-earth/pull/239), [evidence coverage draft #241](https://github.com/jackwillis/poodle-earth/pull/241), [historical JVM feasibility](../research/pe-java-experimental-protocol.md). Drafts may change or remain unmerged. Never treat illustrative scores as measured users or effort.

## Governing distinction

1. **Directional product decision:** may be made from the existing research, customer experience and transparent constraints without a completed experiment.
2. **Engineering investment:** requires a bounded assignment, falsifiable acceptance criteria and the appropriate owner authorization under [AGENTS.md](../../AGENTS.md).
3. **Supported binary or release claim:** requires exact OS/ISA/ABI/toolchain/packaging and native execution evidence, not a promising toolkit or successful cross-compile.

Our platform-portfolio optimizer informs and challenges product judgment; it does not automatically grant implementation, acceptance or release authority. Preserve a record of the evidence and revisit conditions rather than waiting for a calibrated forecast before making directional choices.

## Decision register

| ID | Direction | Basis | What would cause reconsideration | Scope |
| --- | --- | --- | --- | --- |
| PD-01 | **Keep the portable C++20 geographic engine**, Elm-style Model/Msg/update/Effects, shared semantics and versioned geography; retain native presentation per platform rather than a mandatory universal GUI abstraction. | Existing Classic implementation, Unix frontend boundary and architecture constraints. | Measured, concrete duplication or incompatible target toolchain makes an alternative materially better; separate design review. | Product architecture; no accepted ADR changed |
| PD-02 | **FLTK Workbench is the intended standard Unix frontend** where it is technically viable, usable and qualified. | Customer-first native UI, reusable investment across modern Unix and reduced bespoke Xlib chrome. #183 and draft #236 contain research, *not* a passing FLTK runtime. | Unacceptable dependencies, resident memory, availability, input behavior, native packaging or long-term maintenance on a given target. | Future product direction, not v0.1.3 gate |
| PD-03 | **Ordinarily distribute one customer-facing frontend per Unix OS/architecture target** under the public identity `poodle-earth`. **Retain Xlib as a compatibility/legacy reference** when FLTK cannot meet target constraints; maintenance-only or paused Xlib development is an option after real FLTK replacement evidence. | Avoid duplicate customer packages and two permanent UX/test/maintenance obligations; owner explicitly dislikes dual shipping. | A specific target demonstrates a compelling user need for two supported interfaces and the associated maintenance cost is justified. | No immediate deletion, freeze or support withdrawal |
| PD-04 | **Treat the Java 1.4 AWT Reader as an independent historical portability experiment**, not a rewrite or automatic binding to C++20. | One possible Reader architecture across Tiger PPC, historical Windows and Solaris JVMs; Linux-hosted FOSS-oriented build route might be easier than native Tiger Cocoa under strict inputs. | Authentic JVM/API/AWT evidence fails; footprint, duplicate-engine cost or licensing restrictions overwhelm the incremental reach. | Research-only [#237](https://github.com/jackwillis/poodle-earth/issues/237); no JAR/port commitment |
| PD-05 | **Investigate vector-only Classic 68k as a constrained capability tier**, initially coastline/graticule projection and rotation, not a cut-down promise of full PPC feature parity. | QuickDraw vectors may suit small-memory 68k hardware better than the full renderer; independent correctness and measured latency/memory needed. | Projection cost, RAM, supported CPU/toolchain or UX tests make it unusable. | Research idea; no 68k product support claim |
| PD-06 | **Treat native historical Mac OS X (especially Tiger PPC Cocoa) as a separate high-risk route** under FOSS-first build-input preferences; do not infer compatibility from modern macOS or Java bytecode. | SDK/framework licensing and old ABI/runtime qualification differ from modern hosted macOS CI. | A legally and technically reproducible toolchain plus authentic runtime tests changes feasibility. | Research only |
| PD-07 | **Prioritize shipping the already-scoped releases over finishing the optimizer.** Use small discriminating experiments in parallel, not as unapproved shipping blockers. | Research is sufficient to guide directional choices while effort, overlap and value-of-information estimates remain hypothetical. | An explicit owner-approved release scope revision. | Scheduling principle; defer to #212 |

### One frontend does not mean identical interfaces

Share geographic semantics and data contracts, not necessarily window layouts, visual chrome, widget look, or full feature parity. Where a fallback Xlib target is supported, it may remain a deliberately constrained Reader. Do not ship both Xlib and FLTK on one Unix target merely because both compile. Retain existing Xlib tests and code while the transition is hypothetical.

### Customer-facing Unix contract

Install one `poodle-earth` command and one relevant native package or archive per *qualified* target. The embedded world should run offline without optional data packs. Document ABI and library requirements, tested versions, revision and acceptance evidence. User preferences belong in appropriate user-writable locations (XDG where applicable); no installer should write to read-only media or discard user-owned data. Native pkgsrc packages on NetBSD are different from Linux tarballs. Broad regional pack design remains separate from initial Unix packaging.

## Release Steering reconciliation (explicit, no change)

As of the November 21 [owner-approved #212 roadmap](https://github.com/jackwillis/poodle-earth/issues/212):

- **v0.1.3 — Poodle Earth comes to Unix:** ship the existing Xlib application in **one tested Linux x86_64 portable archive** and a **genuine tested NetBSD pkgsrc package for one named, demonstrated architecture**; include modest repository hygiene (#230) and an exact matching-source hybrid CD (#231). Drafts [#211](https://github.com/jackwillis/poodle-earth/pull/211), [#215](https://github.com/jackwillis/poodle-earth/pull/215), [#216](https://github.com/jackwillis/poodle-earth/pull/216) are preparation, not accepted releases.
- **FLTK is a non-blocker for v0.1.3**, despite being the intended future Unix default. Shipping a transitional Xlib release does not establish a permanent two-product strategy.
- **v0.1.4 — PoodleScripts** and **v0.2.0 — a better atlas** retain their recorded directions. Java, 68k, Cocoa, new regional packs and extra Unix formats are not implied gates.
- The Linux runtime/ABI baseline, selected NetBSD architecture, packaging acceptance and milestone metadata remain separate release-steering responsibilities. No package or operating system is supported by this document.

## Next bounded actions

1. **Release Steering:** review live #211/#215/#216 dependency order, Linux baseline evidence and native pkgsrc acceptance needed for v0.1.3, without conflating toolkit direction with release acceptance.
2. **Platform engineering/research:** obtain a pinned, actually running Linux FLTK build and matched Xlib comparison for [#183](https://github.com/jackwillis/poodle-earth/issues/183)/[#184](https://github.com/jackwillis/poodle-earth/pull/184). No G4 access without new explicit authorization.
3. **Historical research:** execute PE-JAVA-00's API/bytecode negative controls and authentic JVM launch only within an independently authorized experiment; separately prototype vector-only 68k after scoping and approval.
4. **Model maintenance:** preserve [#239](https://github.com/jackwillis/poodle-earth/pull/239) and [#241](https://github.com/jackwillis/poodle-earth/pull/241) as experimental decision support, collect observed owner/validation costs opportunistically, and do not require a complete 71-target optimizer to ship releases.

**Review triggers:** new native FLTK measurements, failed pkgsrc/ABI acceptance, demonstrated JVM portability, 68k performance results, significant maintenance burden, or a change to the owner's customer-experience preference. Revisit the relevant decision ID with dated evidence, rather than silently replacing it.

**Authority:** This is a proposed, reviewable product-direction document. It does not modify an Accepted ADR or authorize a merge, hardware operation, new license/data source, milestone edit, tag, release or deployment.
