# Rendering backends, execution and display adaptation — exploratory design

**Status:** Nonbinding research hypotheses, 2026-11-12, condensed 2026-11-13; neither a backend selection nor an approved refactor. The current single-threaded, cooperative RGB565 CPU renderer is the **reference implementation and fallback**. Experiments are defined once, in the registry proposed in [PR #113](https://github.com/jackwillis/poodle-earth/pull/113); this note only records concepts and parked options.

See [architecture](../architecture.md), [ADR 0002](../adr/0002-rendering-shared-code-and-usability.md), [refinement order](../refinement-order.md), [text layout](../design/text-layout.md), [G4 measurements](../perf/renderer-integration-20261110.md), and issues [#2](https://github.com/jackwillis/poodle-earth/issues/2), [#18](https://github.com/jackwillis/poodle-earth/issues/18), [#34](https://github.com/jackwillis/poodle-earth/issues/34), [#55](https://github.com/jackwillis/poodle-earth/issues/55), [#59](https://github.com/jackwillis/poodle-earth/issues/59), [#64](https://github.com/jackwillis/poodle-earth/issues/64), [#97](https://github.com/jackwillis/poodle-earth/issues/97).

## Three independently variable concerns

1. **Render implementation:** CPU rasterization today; possibly OpenGL *geometry* rendering (not merely uploading a CPU bitmap) or other APIs if evidence warrants.
2. **Execution strategy:** bounded cooperative continuations today; possibly a background or parallel CPU executor. Changing the executor need not rewrite the rasterizer. Neither all jobs nor the OS 9 Toolbox can be assumed thread-safe.
3. **Presentation:** QuickDraw GWorld/CopyBits, XImage, or an optional GPU upload path. Presentation throughput is not geometric rendering throughput.

Also keep separate: **look** (Map/Relief), **detail quality** (geometry, terrain, label density, #34), **display format** (output depth) and **backend selection**. A monochrome screen need not imply low geographic detail; a slow true-colour CPU may still need reduced detail.

## Existing implementation (not greenfield)

- `Scene` caches geometry and caps; `Framebuffer` holds RGB565 pixels plus a coverage bitplane.
- `RefinementJob` has bounded, cancellable phases; the limb pass preserves a particular final overlap order. `RefineRunner` owns the shared lifecycle, `ZoomPreview` caches terrain, `ShownFrame` composes overlays during presentation.
- OS 9 converts RGB565 to a QuickDraw RGB555 GWorld and presents in bounded, adaptive CopyBits slices. Shared `pixel_format` converts RGB565 to several X11 16/24/32-bit layouts.
- Line-first refinement already uses a private line layer with sparse-pixel composition; conceptual layers do not each need a screen-sized buffer.

## Logical layers and text

Background/terrain → geographic vectors → place symbols → cartographic labels → highlights/measuring → transient annotations. A layer description would specify semantics, ordering, style, visibility, clipping and resource lifetimes; a backend may consume different prepared representations (CPU screen-space spans, or geometry transformed by a GPU). Stable identity and hit-testing stay consistent. Labels never go into the terrain cache or a scaled zoom-preview image. Glyph-mask compositing is an alternative under [#36](https://github.com/jackwillis/poodle-earth/issues/36), not adopted; native drawing remains the baseline ([text layout](../design/text-layout.md)).

## Execution and ownership

The main `update` owns authoritative state. Any worker receives immutable inputs or an exclusively owned workspace with a byte budget and publishes results through a main-loop message only when generation and resource tokens match. The cooperative executor must remain fully functional. Parallel direct writes to a shared framebuffer are unsafe without deterministic ownership; final-image rules, stable cell/limb draw order and cancellation must hold. A worker cannot call arbitrary Toolbox services; OS 9 Multiprocessing Services are a research target only after runtime/API audits. Prefer bounded pending work and a deterministic reference path over a thread-pool framework, and count worker stack and scratch memory.

## Evidence update: terrain execution and approved presentation (2026-11-23)

**Status:** Research synthesis only. The experiments below are draft, test-only evidence; they are **not merged**, not an accepted ADR, not a new renderer architecture, and not a release or platform-support commitment. See their reports for the exact fixtures, compiler provenance, stop conditions, and limitations.

- **X0 — terrain partition invariance ([draft PR #257](https://github.com/jackwillis/poodle-earth/pull/257)).** On the reported host matrix, 1,140 views produced zero mismatches across 7,980 reordered-band schedules and 3,420 independently owned band-buffer runs. Terrain-only pass-boundary comparisons also matched (3,247 checks across 191 views). This supports a **narrow terrain execution seam**: disjoint screen-row work can be rescheduled while prepared inputs remain stable and relevant cell/limb ordering is preserved. It does **not** establish threading, arbitrary cell-order independence, speedup, deep immutability, or whole-lifecycle equivalence. `const TerrainScene&` does not make its owned arrays deeply immutable; a reused workspace must remain alive and unchanged until all readers have stopped.
- **X1a — cooperative lifecycle characterization ([draft PR #258](https://github.com/jackwillis/poodle-earth/pull/258), erratum `0fdb160`).** The real runner/job emits 19 logical publications for the tested fresh lines-first jobs and 20 for preserved jobs. Coastlines/borders can be published **before terrain preparation**. Publication frames were budget-invariant in the tested runs, but work-unit positions may differ between sparse-list and scan composition. Classic's deferred capture can read live pixels between logical publications. The erratum corrected bare-job focus to the production view centre and fixed reported preparation counts; use the corrected report.
- **X1b — pass-transition equivalence ([draft PR #259](https://github.com/jackwillis/poodle-earth/pull/259)).** A small test-only model reproduced 96 production publication transitions across five cases under the cooperative order and four band-reordered schedules (480 transition comparisons), including tested snapshots. Pass barriers and canonical overlap ordering matter. **No reordered production `RefinementJob` ran**; cancellation, progress, native presentation and resource lifetime remain governed by the unchanged cooperative lifecycle.
- **Observed behavior is not automatically intended behavior.** X1a/X1b found residual line and marker pixels in the nominally terrain-only zoom snapshot (and a different scaled preview), plus a preserve-mode restore publication before the final line stamp. The final published frame in the tested cases was correct. Whether to require terrain-only preview content, reuse canceled mid-pass frames, or suppress the restore-only publication remains **an owner/product decision**, not a new golden-image contract.

**Presentation ownership hypothesis (not adopted).** Issue [#256](https://github.com/jackwillis/poodle-earth/issues/256) proposes three narrow shared contracts: terrain inventory, approved-frame ownership, and transactional resize. The approved-frame question is distinct from terrain scheduling: (1) logical `band_ready`, (2) successful conversion/capture into native storage, (3) approval for repaint, and (4) actual display need not happen simultaneously or contain identical pixels. **X-AF1 confirmed the X11 repaint defect** in [draft PR #261](https://github.com/jackwillis/poodle-earth/pull/261): with a known full-frame approved image and rendering held, a real Xvfb Expose took the production `Repaint` → `upload()` path and reconverted unfinished live pixels, replacing the approved `XImage` and changing 1,384 / 4,485 / 5,421 displayed pixels at early / mid / finished-held states. The stored-image `resend()` arm restored the approved frame exactly (0 differences), and a deliberately live-upload negative control distinguished the paths. These results reproduced at Xvfb depths 24 and 16 under GCC and ASan/UBSan; release recovered in the tested cases, disproving the earlier predicted stale-release hazard. X-AF1 itself tested no production patch or Classic hardware. Subsequently, **[PR #262](https://github.com/jackwillis/poodle-earth/pull/262) merged** as `23245755d38f4a0a1cf514af1d9e009d762cd377`, giving X11 held-drag Expose a `resend()` path backed by Xvfb regression tests, independent review, and no new framebuffer. Non-held Expose still uses `upload()`. **Classic foreground `updateEvt` reconversion remains source-derived and hardware-unverified**, not an X11 qualification. The existing native XImage/GWorld is the first candidate for approved storage; no permanent extra framebuffer is implied by this proposed contract. Other held upload paths, labels, partial-row publication and resize remain untested.

**Current boundary:** retain `RefinementJob` and ADR 0002's lines-first/cooperative semantics; keep the Elm model independent of bands and workers. Any future alternate executor needs explicit prepared-workspace lifetime/quiescence, independently owned mutable output, generation-safe cancellation and bounded continuations. A GPU backend may share geographic semantics without consuming CPU band plans. Do not extract a general renderer interface or amend an accepted ADR solely on this evidence.

## Backend capabilities and fallback

A future capability probe might report depths, graphics API and *actual* hardware acceleration, texture limits, pixel formats, memory ceiling, workers and font services. A driver advertising OpenGL does not prove hardware acceleration. Always support the CPU renderer; benchmark any accelerated path per target; fall back safely on init or allocation failure. Avoid automatic display-depth changes, unconsented preference writes and live backend switching until resource-lifetime tests exist. Hide options a platform doesn't have.

The CPU reference keeps exact cross-run hashes and the terrain/limb invariants. An independent GPU path might differ at edges (clipping, precision, interpolation); compare geometric/semantic invariants and visual tolerances, not pixel equality across unlike backends.

## Parked and conditional options

Records of options and what would reopen them; none is a plan. Experiments, predictions and stop conditions live in the registry ([PR #113](https://github.com/jackwillis/poodle-earth/pull/113)).

| Option | Status | What would reopen it |
| --- | --- | --- |
| SIMD (AltiVec) kernels | **Conditional research option under #97/#64**, not ruled out | Existing evidence points elsewhere first: #64 notes that converting two pixels per 32-bit word needs no AltiVec, and #97's counts and PPC32 codegen notes put settle cost in per-scanline setup, `ceil` and 64-bit conversion/divide library calls. These are host and codegen observations, not G4 measurements. Revisit if a measured stage is vectorizable pixel work (conversion, composition, glyph coverage) |
| OpenGL presentation or geometry | Parked | A read-only capability probe (independent of any decision to build GPU rendering) shows genuine hardware GL on a real target, and stage timings show presentation or fill dominates |
| Parallel / background execution | Parked | Measured benefit on a multi-core target without harming the single-core cooperative path, with bounded scratch memory and cancellation |
| Display depth vs. detail quality | Under #34 | Evidence that depth conversion and geographic fidelity affect responsiveness separately |
| Glyph-mask compositing | Alternative under #36 | Native-drawing baseline measured, memory and licensing reviewed |

Do not adopt a generic scene-graph or render-graph framework, multiple GPU APIs, Vulkan, Direct3D or a general UI library to future-proof the project; a second *real* implementation should reveal the necessary API.

**Decision gate:** an accepted design change requires correct behaviour, consistent provenance, actual platform measurements, documented memory impact and a reviewable implementation issue. v0.1 obligations are unchanged.
