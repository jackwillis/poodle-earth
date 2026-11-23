# X0: partition invariance and independent output ownership — experiment report

**Status:** Experimental research, 2026-11-23. Owner-authorized, host-only, test-only. **Not approved for merge**; it does not change production rendering, an accepted ADR, a golden image or a milestone. Companion to the [rendering backends note](rendering-backends-and-execution.md) and the [experiment registry](rendering-experiment-program.md).

**Question:** can the settled CPU terrain image be split into non-overlapping 16-row bands, run in any order, and assembled into the canonical image (X0a); and can each band render into storage it exclusively owns (X0b)? No threads, atomics or worker pools are involved, so nothing here is evidence of thread safety, speed or OpenGL suitability.

## Baseline and method

- Baseline `origin/main` at `cd0718e` (2026-11-23). Branch `research/x0-partition-invariance-20261123`.
- Host: Fedora 44, Linux 7.2.10, x86-64, 12 cores. GCC 16.2.1 and Clang 22.1.8 (Release); Clang 22.1.8 with `-DGLOBE_SANITIZE=ON` (ASan and UBSan).
- Change: `tests/test_partition_invariance.cpp` and one word in the `foreach` of `CMakeLists.txt`. No file under `src/` or `include/` changed.
- Run: `./build/test_partition_invariance . full` (quick mode, the default, is what `ctest` runs: about 9 s).
- The harness uses only public `TerrainScene`, `Framebuffer`, `fill_disc` and `RefinementJob` calls. Every drawing function takes `const TerrainScene&`, so the compiler enforces that pixel generation does not mutate the prepared view plan.

**Oracles.** (A) a copy of `reference_terrain` from `tests/test_terrain.cpp` (the definition of the final image: every unclipped cell in index order, then every clipped cell), extended with the preserve-bitmap coverage and restore steps. The original lives in an anonymous namespace, so this is a copy, not the same code. (B) a completed production `RefinementJob`, with `Layers` off and `terrain_only`.

**Schedules.** All prepare the view exactly as the job does (project, bound, `assign_pass` with the job's ocean-skip condition, pass lists), then draw `(band, pass)` work:

| Schedule | What varies |
|---|---|
| band-major normal / reverse / 2 × shuffled | band order; each band runs rings 0..15 then the limb pass |
| band-major index-order | each band: unclipped cells in index order, then limb cells (the definition, restricted to a band) |
| pass-major | bands in a fresh random order inside every pass (a barrier between passes) |
| random interleaving | any interleaving that keeps each band's own order |

Each view is also prepared with a random focus, while the reference uses the view centre, so ring assignment differs from the oracle.

## Results

Matrix (`full`): 1,140 views over 7 grids, each cycling all 16 looks (Map/Relief × smooth/flat × skip-ocean × preserve-bitmap), with random cameras, polar (±89.9°) and south-biased views, sizes from 40×17 to 640×480.
Grids: contrasting 24×12, 36×18, 60×30, 120×60, 400×300 (no pass lists), the real 1° pack with land classes, the real 0.5° pack (no pass lists).
Fixed seeds throughout.

| Check | Runs | Differences |
|---|---|---|
| X0a, six schedule families vs the reference and vs the production job (pixels) | 7,980 schedule runs | 0 |
| X0a draw-slice count equals the job's `terrain_slices` (visible-cell spans for the index-order schedule) | 7,980 | 0 |
| X0b, three schedules, own full-size buffer per band, vs the reference and the job | 3,420 | 0 |
| X0b guard rows untouched (pixel canary) | 3,420 | 0 |
| X0b coverage untouched outside the band | 3,420 | 0 |
| X0b assembly in two different orders is byte-identical | 3,420 | 0 |
| Production job equals the reference (oracle consistency) | 1,140 | 0 |
| Prepared plan identical before and after all drawing | 1,140 | 0 |
| Pass-boundary frame equals the cooperative job's frame, bands reordered inside each pass | 3,247 boundaries in 191 views | 0 |

**X0a: PASS** and **X0b: PASS** for the stated configurations. Same report from GCC, Clang and the sanitizer build.

**The guards can fail.** A one-row overreach of the band clip, injected on a scratch copy, was caught by 214 of 228 pixel guard checks and 107 of 228 coverage guard checks (quick mode). The change was reverted.

### Dependency controls (findings, not assertions)

| Control order | Views that differ from the reference |
|---|---|
| limb pass before the rings | 95 of 1,140 (smallest: one pixel, contrast 24×12, 97×65) |
| limb cells in reverse index order | 94 of 1,140 (smallest: two pixels; includes 1 of 340 on the **real 1° pack**) |
| rings in reverse order | 0 of 1,140 |
| unclipped cells in reverse index order | 0 of 1,140 |
| restore before the limb pass (preserve only) | 0 of 568 |

Limb ordering is the one dependency that is observable. Differences appear only on contrast grids and one real-1° view. None appeared on the 0.5° or 400×300 grids. 754 views contained a visible flat Antarctic cell and showed no ordering sensitivity, so the hazard flagged in the P audit (flat polygons using a different tie rule) did not show up in the finite matrix. That is not a proof of its absence.

### Coverage of the hazards
Every view had visible limb cells; 754 had visible flat Antarctic cells; 71 had cells removed by ocean-skip (that look is 1 of 16); 568 used the preserve-bitmap path with a seeded previous frame; none was empty.

## Ownership findings

| State | Finding |
|---|---|
| `TerrainScene` arrays after `BuildPassLists` (`projected_`, `bands_`, `passes_`, `order_`, `start_`) | Read-only for all pixel generation (`draw_cell`, `cell_*`, `pass_*`, `is_limb` are `const`; no `mutable` or static state). Mutable only inside the job's prepare phases. |
| Framebuffer pixels | Written only through the band clip. Rows are disjoint, and the coverage plane is stored per row, so no byte is shared between bands. |
| Framebuffer clip rows and coverage mode | **Members of the `Framebuffer` object.** Two workers cannot share one `Framebuffer`; this is the real coupling. Private buffers (X0b) remove it. |
| Coverage plane | Consumed inside the band: the restore pass is row-local, so coverage never has to be assembled. (There is no public accessor; it was verified through the restore result and a probe.) |
| Prepared-workspace reuse | `draw_cell` carries no token or generation check. The `RefinementJob::step` token check is the only protection. A stale executor that kept drawing after a new job's `claim()` and prepare drew the new view's plan into the old view's frame and matched neither view. |

Lifetimes that matter under concurrency: the plan must outlive every reader; a new job's `claim()` plus `ProjectCorners` must not start until old readers have stopped; the job's private line layer and the snapshot target are separate single-writer objects.

## Memory

Measured with a `MemoryLedger` (`JobWorkspace`): one full-size buffer per band.

| View | Bands | Per band (pixels + coverage) | All bands live | Row-window estimate (all bands) |
|---|---|---|---|---|
| 160×120 | 8 | 40,800 B | 326,400 B | 43,520 B |
| 640×480 | 30 | 652,800 B | 19,584,000 B | 652,800 B (one frame) |

A row window (pixel pointer, stride, clip and coverage rows only) costs about 21,760 B per band at 640×480 (20,480 B pixels + 1,280 B coverage), so W workers cost W × 21,760 B instead of W × 652,800 B. Full-size buffers are acceptable for a test and not for a historical target. The estimate is arithmetic, not a prototype.

## Scheduling, presentation and cancellation

**Mandatory ordering within a band** (observed or semantic): clear before any draw; the limb pass after all unclipped drawing, in index order; restore after the limb pass. **Not mandatory** in the finite matrix: order of rings, order of unclipped cells, order of bands.

**Barriers.** Pixel generation needs none across bands. These do (not exercised by X0): the whole-frame snapshot copy into the zoom cache at the last stamp, `keep_partial` on cancel, the line-layer composite after each pass, and city-marker drawing after each `band_ready`.

**Publication.** With a barrier between passes, the frame after pass *k* equals the cooperative job's frame at that pass boundary however bands are ordered within the pass (3,247 comparisons). So execution can be reordered while publication stays ordered at pass granularity. The job emits `band_ready` 17 times (rings and limb) plus once more on reaching `Done`.

**Not preserved by independent scheduling:** mid-pass partial frames, the 64-unit and four-slice step bound (input latency), `progress()` (derived from `pass_` and `list_pos_`), and what `keep_partial` captures on cancel. A cancelled partial frame is deterministic only if cancellation waits for a barrier; with unordered completion it depends on timing.

**Workspace reuse precondition:** all readers quiesced before the next `claim()` and prepare.

## Not tested

Lines-first composition, line-layer banding, city markers, overlays, the snapshot copy, `keep_partial`, any real concurrency (data races, memory ordering, cache behaviour), performance, OS 9 or Retro68 builds, or a GPU path. The matrix is finite, with random but fixed seeds.

The existing `x11_idle` test failed once when run in parallel with the new test under `ctest -j4` (CPU contention); alone and sequentially all 69 existing tests pass.

## Interpretation

The existing design already separates **request** (`Effects::refine` with a generation), **prepared work** (the `TerrainScene` arrays plus the camera, quality and palette values), **execution** (draw tasks keyed by band and pass) and **publication** (pass-boundary frames). They are fused only inside `RefinementJob::step`'s cursor machine. The smallest boundary the evidence supports:

- **Preparation** produces an immutable plan.
- **Execution** is `(band, pass)` work with row ownership.
- **Publication** happens at pass barriers.
- The application model needs only the generation, level and progress; it never sees bands.

## Recommendation

1. **Additional correctness research first (test-only):** cover the untested serial tail (line layer and composite, city markers, snapshot, `keep_partial`) the same way. This may need a minimal testability change, which would need separate authorization.
2. **Then a row-window prototype**, since memory is the one cost X0 measured clearly.
3. **Do not start a threaded executor.** There is no multicore target the project is committed to, and the cooperative path remains the reference. Reorderable work may still be useful single-threaded (for example, band priority), which is a separate product question.
4. No production refactor is justified yet.
