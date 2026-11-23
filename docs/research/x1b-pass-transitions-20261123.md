# X1b: pass-transition equivalence — experiment report

**Status:** Experimental research, 2026-11-23. Owner-authorized, host-only, test-only. **Not approved for merge.** No change to `src/` or `include/`, a golden image, an ADR or a milestone. The production `RefinementJob` (directly, or through `RefineRunner`) is the only lifecycle driver. Companion to [X0](x0-partition-invariance-20261123.md) (#257) and [X1a](x1a-publication-lifecycle-20261123.md) (#258), both untouched. Classic's approved-frame repaint belongs to #256 and is not touched here.

## Question

Is each frame the production code publishes reproduced by a small model applied to the *previous published frame*, when the model draws the terrain work of each pass in a different order?

```
F_{k+1} = markers( stamp( restore( draw_pass( clear( F_k ) ) ) ) )     (the steps that apply to transition k)
```

`stamp` and `markers` are pure per-pixel overlays; `draw_pass` draws one ring or the limb pass with the existing public `TerrainScene::draw_cell`. Production supplies every F_k; the model is never run from its own output.

## What the model contains, and what it does not

| In the test | Size and origin |
|---|---|
| `prepare()`: the job's project, bound, assign and list phases, run while no job is active | about 15 lines, a copy of the job's calls (X0 showed it equivalent) |
| `clear_rows`, `restore_rows` | about 25 lines, a copy of the job's row operations |
| Line stamp | a mask from two lines-only production jobs; copy values at mask pixels |
| City markers | the real `PlaceScene::draw` |
| Marker hook replay (snapshot only) | 4 lines mirroring `RefineRunner::step`; must reproduce every runner publication (it does) |
| Own `Framebuffer` coverage plane (preserved case) | rebuilt by the model's own draws; never read from production |

**Not rebuilt:** the phase machine, cursor state, line layer and its list or scan compositor, snapshot adoption, cancel, generation tokens, progress. No private state, production hook or accessor was needed, so none of the stop conditions fired.

## Method

- Baseline `origin/main` `cd0718e`. Branch `research/x1b-pass-transitions-20261123`. Files: `tests/test_pass_transitions.cpp`, one token in the CMake `foreach`, this report.
- Host: Fedora 44, Linux 7.2.10, x86-64. GCC 16.2.1, Clang 22.1.8, Clang with ASan and UBSan. Output is byte-identical across all three and across repeat runs. About 0.4 s.
- Commands: `cmake -S . -B build -G Ninja && cmake --build build --target test_pass_transitions && ./build/test_pass_transitions .`; `ctest -j1` (70/70); `clang-format`, `clang-tidy -p build` (one scoped NOLINT for optional access), `tools/check-policy.sh`.
- Cases: `RefineRunner` fresh and preserved (real 1° pack, synthetic lines, real cities); a bare job on a Relief/flat polar view; two views of the 24×12 contrast grid. Bare-job cases use the production focus (view centre).

## Results: what matched

| Arm | Fresh runner | Preserved runner | Polar flat | Limb, X0 view | Limb, searched view |
|---|---|---|---|---|---|
| Calibration (cooperative order) | 19/19 | 20/20 | 19/19 | 19/19 | 19/19 |
| Bands reversed | 19/19 | 20/20 | 19/19 | 19/19 | 19/19 |
| Bands shuffled (2 seeds) | 19/19 ×2 | 20/20 ×2 | 19/19 ×2 | 19/19 ×2 | 19/19 ×2 |
| Bands interleaved, 4-slice chunks | 19/19 | 20/20 | 19/19 | 19/19 | 19/19 |
| Snapshot reproduced (each arm) | yes | yes | yes | yes | yes |

Every transition of every arm reproduced the published frame, including the markers. Production frames are identical at work budgets 7 and 64.

**Mid-chunk diagnostic** (draw only; every pixel after each continuation of four slices must be the pre-pass or the finished-pass pixel): 0 outside on all real-data cases (0 of about 14 to 33 million checks each); 0 to 27 checks outside on the two limb views (multi-writer pixels). Diagnostic only. It is a draw-only two-value rule, not X1a's four-value production rule.

## Controls

| Control | Result | Reading |
|---|---|---|
| No pass barrier (two rings drawn before one publication) | 4 to 6 of 19 reproduced | Required: the barrier is what makes frames match |
| Markers before the stamp | 1 of 19 (fresh), 2 of 20 (preserved) | Marker and stamp order is part of the published frame |
| Cells reversed within a pass | real data: all reproduced; X0 view: publication 17 differs; searched view: publications 17 and 18 differ | Per-row write order *inside* a pass matters wherever pixels have two writers; the band orders above keep each band's cell order |
| Limb cells reversed, X0 view | 19 of 19 published frames reproduced; **pre-stamp snapshot not reproduced** | The two differing pixels are line pixels, which the stamp rewrites: published frames cannot see this ordering, the snapshot can |
| Limb cells reversed, searched view | 18 of 19 (publication 18 differs); snapshot not reproduced | A fixture where limb order survives into a published frame (found after 12 candidates; exact parameters printed by the test: lat −39.87661600112915, lon −79.189023971557617, heading 28.621766567230225, radius 36.746217012405396, 97×65) |

## Observed today, reported separately from the intended contract

These are **not requirements for a future executor.**

- **Snapshot content.** Intended: terrain only. Observed: it differs from the terrain-only final at 2,402 px (fresh): all 1,821 city-marker pixels and 581 line pixels; preserved: 2,936 px (2,183 marker, 753 line); nothing else.
- **Carry-through to the zoom preview.** A preview built from today's snapshot (radius factor 1.1) matches the real runner preview at **0 px**, so the replica is valid. A preview built from an intended terrain-only snapshot differs from the real one at 2,034 px (fresh; 1,818 black or white) and 2,443 px (preserved; 2,198). Markers (and some line pixels) carry into the scaled preview; whether this is visible as doubling needs a display and is assessed separately.
- **Preserve mode.** 20 publications (fresh: 19). The restore is published as #18, before the final stamp, and lacks **25 line pixels** (markers excluded). An intended restore-with-lines frame differs from it at exactly those 25 pixels, all on line pixels; publication #19 (after the stamp) equals the intended frame. The model reproduces today's sequence (observed), and the intended variant only as a computed comparison.

## Erratum to X1a (#258), found while building X1b

X1a's bare-job cases used the default `RenderQuality::focus` (0,0), not production's view centre (the runner cases were correct). Re-running with the centre focus (uncommitted check, reverted) gives: budget invariance, bounds and the real-data views unchanged (0 violations); on the limb fixture the mid-pass violation pixels move ((26,38), (34,14), (42,54); the (59,12) coincidence with X0 does not reproduce); the "prepare leaves the frame unchanged in 195,306 of 195,306 states" count includes the state that draws the first slice, so the true statement is "all but that state". The finding about the transition from X1a stands; the figures in 3 and 9 need that correction. #258 was not modified.

## Where the evidence stops

- Equivalence is shown for **transitions between published frames**: 96 production transitions in five cases, each reproduced under the cooperative order and four reordered schedules (480 comparisons), not for the whole lifecycle running under a reordered pass.
- The production `Composite`, snapshot, markers, cancel, `keep_partial`, progress and generation code paths were **not** exercised under reordered execution. They were exercised only in the production runs that supply F_k.
- Published frames cannot see limb-order differences that fall on line pixels; the snapshot can.
- Mid-chunk states in the model are draw-only; production's mid-stamp and mid-restore states are X1a's observations, not X1b's.
- Per-pass work was a single task list; task size and cancel latency inside a continuation were counted in slices, not measured in time. No threads, no display, no Classic or X11 timing.
- Two cases use contrast grids (limb-sensitive by construction); the real-data cases never showed a within-pass or limb-order effect.

## Interpretation

The pass transition (published frame k → published frame k+1) is reproducible from the previous published frame by a very small model, and the work inside each pass can be reordered by band, in four-slice chunks, without changing a published frame or the snapshot, provided the pass barrier holds and each band keeps its cell order. The model did not need the job's phase machine.
