# X1a: characterization of the cooperative publication lifecycle — experiment report

**Status:** Experimental research, 2026-11-23. Owner-authorized, host-only, test-only. **Not approved for merge.** It changes no production source, golden image, ADR or milestone, and it proposes no architecture. Companion to [X0](x0-partition-invariance-20261123.md) (PR #257, untouched). The cooperative renderer is the authority throughout.

**What it is:** a recorder. `tests/test_publication_lifecycle.cpp` drives the real `RefinementJob`, `RefineRunner`, `ZoomPreview`, `ShownFrame` and `PlaceScene` and prints what they do at their public boundaries (`OBS` lines). Its CHECKs cover only harness validity and contracts the existing tests already pin (19 fresh publications, a cancelled job is inert). A difference between budgets or configurations is reported, not failed. It reuses `test_line_layer` and `test_zoom` unchanged as the existing coverage and does not recreate it.

## Logical publication versus display

- **Logical publication** is a returned `RenderStep` with `band_ready` set, after the runner's own side effects.
- **X11** converts and uploads at `band_ready` (throttled; the last frame is always shown): displayed frames are a subset of logical publications.
- **Classic** only requests a capture at `band_ready`. `capture()` and `present_step()` later read the live `*fb_` and the live `runner_.job()->progress()`, after further steps may have run (`src/platform/os9/main.cpp:234-262, 535`). A displayed Classic frame can therefore be a mid-pass frame. The simulation below shows how often. Nothing here measures display time.

## Method

- Baseline `origin/main` `cd0718e`. Branch `research/x1a-publication-lifecycle-20261123`. Files: the test, one token in the CMake `foreach`, this report. No change under `src/` or `include/`.
- Host: Fedora 44, Linux 7.2.10, x86-64. GCC 16.2.1, Clang 22.1.8, Clang with ASan and UBSan. The report is byte-identical across all three and across repeat runs. About 1.4 s.
- Commands: `cmake -S . -B build -G Ninja && cmake --build build --target test_publication_lifecycle && ./build/test_publication_lifecycle .`; the full suite with `ctest -j1` (70/70); `clang-format`, `clang-tidy -p build` (one documented NOLINT scope for the optional-access check), `tools/check-policy.sh`.
- Views (120×90, or the stated size): real 1° pack with land classes, Map/smooth/skip, fresh and preserved (runner path: Wheel zoom over a preview); a Relief/flat polar-south view; the 24×12 contrast-grid view that X0 showed to be limb-order sensitive. A synthetic atlas plus graticule gives the lines; the real `data/cities.gplc` gives the markers.
- Two drivers: the runner (Model, markers, zoom cache) and a bare job (explicit `line_list_limit`, harness-owned snapshot buffer). Calibration: a bare job plus a four-line replay of the runner's marker hook reproduces **all** runner publications (19 fresh, 20 preserved).
- Budgets 1, 7, 64 (production Classic uses 64).

## Findings

**1. The publication sequence is well defined.** Fresh: 19 publications (lines, 16 rings, limb, end). Preserved: **20**. For every view and both drivers, budgets 1, 7 and 64 give identical publications: frame bytes (markers included), rows, `active()` after, progress, slice count, zoom flags and cumulative work units.

**2. The final publication is returned while `active()` becomes false** (`active_after=0` at publication 19 and 20, rows 0..90). After it, further steps return no work and no publication and leave the frame unchanged. The Model sees `refine_active` true until `RefinementFinished` is applied.

**3. Lines first, before terrain preparation.** The first publication comes after 1,779 units (Map view) and **1,739 units for both a 288-cell and a 64,800-cell grid**. The prepare phases (about 195,000 units) leave the frame bytes unchanged in every state (195,306 of 195,306).

**4. Progress at publications** is monotone and budget-invariant: 0.060, 0.152, 0.204, …, 0.980, 1.000. In preserve mode the restore publication and the next one both report 0.980. Between publications `progress()` differs from the last published value in 6,562 of 6,581 steps (a late Classic capture would show an intermediate bar).

**5. Composition mode.** `line_list_limit = 1` (scan) and 100,000 (sparse) give identical frames, progress and slices, **but different work-unit positions** (e.g. 212,132 versus 210,691 units to the last publication; 3,949 versus 2,888 on the small grid). Publication positions are therefore logical indices, not unit counts.

**6. Preserve mode.** The extra (18th) publication is the restore: it **lacks 42 of 1,167 line pixels** until the final stamp. No existing test pins the preserved publication count or this frame.

**7. Snapshot (surprising).** `ZoomPreview`'s snapshot is taken on entry to the final `Composite`, 2 steps and 54 units before the final publication (fresh, budget 64; 17 of 19 publications already returned). It **is not terrain-only**: it differs from the terrain-only final at 2,402 px, of which **all 1,821 city-marker pixels** and 581 line pixels (preserved: 2,183 marker, 753 line); nothing else differs. The cause is order: stamps after earlier rings and the markers drawn after publication 17 persist until the limb pass overwrites their cells. The code comments call it "the terrain alone". Whether the preview shows doubled lines or markers is a visual question for a real display.

**8. Marker ordering.** `RefineRunner::step` runs `job.step`, then `zoom_.after_step(job)`, then draws markers only when `band_ready`. Consequence: the final frame equals terrain + line stamp + markers at 0 differing pixels (both modes), and markers are intact at every publication; but a mid-stamp frame holds line colours where the publication holds marker colours.

**9. Mid-pass frames.** Hypothesis H-M: a state between publications *i* and *i+1* has every pixel in {publication *i*, publication *i+1*, terrain-only frame after the pass}. Result: **0 violations** in 17,597 changed states (about 163 million pixel checks) on three real-data job views, fresh and preserved. On the limb-sensitive fixture: 11 checks at 3 distinct pixels, including (59,12), the pixel where X0 found the limb-reversed control to differ on this view. With markers (the runner), the three-set rule fails at **193,365 / 109,007** pixel checks, all on marker pixels; adding the pre-marker publication as a fourth allowed value brings both to **0**.

**10. Cancellation.** After `cancel()` the next step is inert (0 units, no publication, frame unchanged) at all eight points tried, fresh and preserved. In a fresh run, `keep_partial` makes the cache **exactly equal to the live frame at cancel**, baked, at six points (right after the lines publication through the limb pass). Between snapshot adoption and the final stamp, and after the end, the finished snapshot is kept. In a preserved run the older finished cache is always kept: partial frames never replace it. A generation change that the platform tells the runner about leaves a baked partial cache that the next moving frame uses as a zoom preview, so **a mid-pass partial frame can reach the screen through the preview**. If the generation advances without `runner.cancel`, the job silently deactivates and `keep_partial` never runs.

**11. Workspace reuse.** A second job claiming the terrain workspace stops the first at its next step (0 units, frame unchanged); the second job's final frame equals a from-scratch job. (Creating a job in the harness out of order did exactly this to the harness's own walker; hence each walker is built immediately before use.)

**12. ShownFrame.** `begin()`/`end()` leaves the rendered frame unchanged at every publication and does not alter the trace; the shown frame differs from the rendered one at all 19 publications (the bar).

**13. Classic-style capture.** A capture 0 steps after a publication equals it in 19 of 19 cases; 5 and 16 steps (64-unit steps) later, in 3 of 17 cases. A displayed Classic frame is usually *not* a published frame.

**14. Count-based bounds.** At budgets 7 and 64 no step exceeds its budget in units or 4 cell-slices (budget 1: 1).

## Contract implications (summary)

The returned-event sequence, its frame contents, the snapshot/marker order, the preserve restore publication, budget invariance and the membership structure are measurable and stable in the cooperative path. A scheduler that reorders work inside a pass has to reproduce the incidental behaviour in 6–8, or the owner has to decide to change it.

## Not tested

Resize and multi-level refinement chains; outline-only (terrain-free) jobs, which publish top-down 16-row bands; overlays other than the default `ShownFrame`; display time; platform presentation; threads; any alternative scheduler.
