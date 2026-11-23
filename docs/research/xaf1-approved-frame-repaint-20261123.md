# X-AF1: approved-frame repaint on X11 — experiment report

**EXPERIMENTAL. Not approved for merge.** Test-only. No file under `src/` or `include/` changed.
This is a *baseline characterization* of the existing production behaviour, not a regression
acceptance test. Reproducing the defect is the intended observation, so the test does not fail on it.

## Provenance

- Baseline: `origin/main` `cd0718e` (`cd0718ede08aae71c0b4b58dd966a4cab55cc105`). Local `main` was not
  fetched or touched. Branch `research/xaf1-approved-frame-repaint-20261123`, worktree `globe-xaf1`.
- Files: `tests/test_x11_approved_frame.cpp`, 14 lines of CMake wiring, this report,
  `docs/research/xaf1-evidence/output-depth{24,16}.txt` (the raw `OBS` output).
- Platform: Fedora 44, Linux 7.2.10, x86-64, GCC 16.2.1 (Release; Debug + ASan/UBSan + leak detection),
  Xvfb 800x600 at depth 24 (32 bpp `XImage`) and depth 16 (16 bpp). No window manager, no compositor.
  Clang was not run for this test.
- Commands:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
xvfb-run -a -s "-screen 0 800x600x24" ./build/test_x11_approved_frame
xvfb-run -a -s "-screen 0 800x600x16" ./build/test_x11_approved_frame
ctest --test-dir build -j1          # 71/71, including x11_approved_frame_24 and _16
```

## What the harness is

It compiles the real adapter (`src/platform/x11/main.cpp`) into the test, as #255's `test_x11_resize`
does, so the real `handle()`, `apply()`, `upload()`, `resend()` and `refine_step()` run against a real
(software) X server. Private members are opened with a test-only `#define private public` after all
standard and X11 headers are included. **No production change or `friend` is needed**; #255 added one,
this harness does not depend on it.

- The window is **cleared by the server** (`XClearArea(..., exposures=True)`), which queues a genuine
  `Expose`. The harness checks the pane reads back as background before the repaint, so a repaint is
  required to restore anything. (Existing tests send synthetic `XSendEvent` Expose events, which do not
  clear the window.)
- Arms differ only in what the harness dispatches for that real `Expose` event:
  - **P** passes it to the adapter's `handle()`: the production path.
  - **S** calls `resend()` instead (stored-image repaint).
  - **L** calls `upload(0, height)` unconditionally (deliberate live conversion; negative control).
  - **N** sends no repaint (control for the release observation).
- Oracles (all independent of `verify_window()`, which compares against the live frame):
  - **Window readback** (`XGetImage` of the 200x150 globe pane, masked to the visual colour mask).
  - **Native image** (`image_->data`, read per pixel).
  - **Live composed**: the live `fb_` converted by `convert_pixels_generic` through a private
    `ShownFrame` exactly as `upload()` composes it (overlays, scale, compass, progress bar).
  - **Live raw**: the live `fb_` converted without composition.
- Country labels are off (`country_names_ = false`). The first state is a *known full-frame upload*
  (a moving frame while dragging); partial-row publication states are not characterized.

### State machine

1. Initial frame presented; `PointerDown` and `PointerMove` past the slop: `presentation_held`. The
   moving frame is presented and read back: **R0** (window), **native0**, **fb0**.
2. The drag pauses (`Tick` at `kDragPauseMs`, then `kRefineDelayMs`): a job starts. `refine_step()` runs
   with presentation held, so the live buffer advances and nothing is uploaded (checked).
3. Three advance depths: `min` (first step at which live ≠ approved), `mid` (at least 6 publications and
   at least 0.5% of the pane differing), `tail` (job finished while still held).
4. Window cleared; the arm's repaint runs; readback **R1**, native image **native1**.
5. The remaining refinement is stepped to completion (job identity, generation, completion checked).
6. A real `ButtonRelease` goes through the queue and handler; readback compared to the independent
   settled composed oracle.

## Results

All numbers are identical at depth 24 and depth 16, across two process runs (byte-identical output),
across arms (identical R0, native0, fb0, live hashes and step counts), and with ASan/UBSan (same output).

| | steps | publications | live ≠ approved (px) | P: R1≠R0 | P: R1≠live | S: R1≠R0 | L: R1≠live | L: R1≠R0 |
|---|---|---|---|---|---|---|---|---|
| min | 1 | 0 | 1,384 composed / 2,027 raw | 1,384 | 0 | 0 | 0 | 1,384 |
| mid | 3,285 | 6 | 4,485 / 4,468 | 4,485 | 0 | 0 | 0 | 4,485 |
| tail | 6,992 | 19 (finished) | 5,421 / 6,114 | 5,421 | 0 | 0 | 0 | 5,421 |

- **P (production Expose) reproduces the defect in all three states.** The window after Expose is
  bit-identical to the independently converted live composed frame (0 px differ) and differs from the
  approved image in 1,384 to 5,421 px (rows 15 to 149 in `min` and `mid`, 15 to 132 in `tail`). With
  uploads during Expose = 1, resends = 0.
- **The approved bytes are destroyed, not just hidden.** After P, `native1` differs from `native0` by the
  same pixel counts. Nothing then holds the approved image.
- **S (`resend()`) restores the approved image exactly** (R1 = R0, 0 px) with `native1 = native0`, and
  without touching `fb_`. The existing `XImage` is a sufficient approved store for this case.
- **L (live control) is sensitive:** R1 equals the live composed frame and differs from R0 in every state.
  Had L matched R0 the result would have been INCONCLUSIVE; that was a CHECK.
- **Smallest counterexample:** one `refine_step()` after the job starts, with no publication yet. The
  difference is the progress bar (1,384 px composed) plus the first live pixels; in the composed view it
  is already enough for Expose to change the window. So the defect does not need a half-drawn globe.
- **`fb_` is unchanged by every repaint** (hash equal before and after).

### Hold, cancellation, validity

- The job pointer, its active state (for `min` and `mid`) and the model generation were unchanged after
  every arm, and refinement completed afterwards (3,707 to 6,991 further steps).
- No upload happened during the held refinement (`pack_ms` count unchanged); only the Expose path uploaded.
- Cancellation by generation change, resize invalidation and allocation failure were **not** exercised
  here (they are #255's territory); nothing in these results speaks to them.

### Release after hold

- In `min` and `mid`, all four arms end with a window equal to the settled composed oracle after the real
  `ButtonRelease` (0 px), with one upload on release. Later steps set `upload_dirty_` again, so release
  recovers normally even after P.
- **Prediction falsified:** the read-only report's secondary hazard (an Expose after the last band clears
  `upload_dirty_` so release never shows the finished frame) did **not** occur. In `tail`/P and `tail`/L
  the Expose upload converts the *finished* live frame, so the window already equals the settled frame
  and release correctly uploads nothing (0 px, 0 uploads). The flag clearing is harmless there because
  the repaint itself displayed the result.
- **Unexpected but related:** that same `tail` Expose shows the finished refinement result **during the
  hold**, before release, which contradicts the held-presentation contract in `app.hpp`.
- With S, the finished frame appears only on release (as intended).

### No approved frame

- Fresh app, idle and with a held refinement advanced 200 steps, `frame_valid_` false, `need_redraw_` true:
  the real Expose under P uploads nothing and the window stays background (0 non-background px), even
  though the live buffer holds 220 non-background px (idle) or 27,732 (held). The production guard
  `!need_redraw_` suppresses it. The L control shows the oracle would see the live pixels in the same
  states (so this is not a vacuous pass). What the 220 idle pixels are (probably scale/compass overlay on
  a blank frame) is not established.
- Not covered: a state with no approved frame and `need_redraw_` false.

### Memory (measured at 200x150)

- Native image 120,000 B (4 B/px at depth 24; 2 B/px at 16), framebuffer 60,000 B, coverage 3,750 B.
- `mallinfo2` in-use heap delta during the Expose was **0 B** for P, S and L, so neither `upload()` nor
  `resend()` allocates net memory in this configuration. `resend()` needs no new framebuffer.
- A separate approved copy would add 4 B/px (32 bpp) per surface: about 1.9 MB at 800x600. Estimates for
  old and candidate surfaces during resize are arithmetic and unchanged from the read-only report.

## Evidence classes

- **Experimental (this report):** everything above under Results, hold, release, no-approved-frame,
  memory. Deterministic on this platform and Xvfb.
- **Code-reading predictions confirmed:** P reconverts the live buffer (`apply(Repaint)` calls `upload`);
  `resend()` is never reached by Expose; `image_` is written only by `upload()`.
- **Code-reading predictions falsified:** the stale-after-release hazard (above).
- **Not tested:** Classic (no hardware, and `os9/main.cpp` is not host-buildable); `reshow()` and other
  non-Expose paths that call `upload()` while held; labels on; resize; throttled partial-row publication;
  real window managers; remote or compositing servers; geometries other than 200x150.

## Limits

- Messages are applied directly with explicit times; only Expose, ConfigureNotify/Map and ButtonRelease are
  real X events. This validates the repaint decision, not pointer timing.
- Arm S substitutes the Expose dispatch inside the harness. It shows that `image_` is a valid approved
  store, not that a particular production patch works.
- The mid state depends on the harness's chosen thresholds (6 publications, 0.5%); the min state and the
  tail state do not.
- `#define private public` is test-only and relies on identical layout, which holds on GCC and Clang.

## Smallest proposed X11 correction (not implemented)

In `apply()`'s `Repaint` branch, when `presentation_held(model_)`, call `resend()` instead of
`upload(0, height)`. Smallest regression test: this harness's P arm at `min` and `tail` with an
expectation of R1 = R0, using the existing negative control L. Open question for that change: whether to
resend unconditionally for any valid frame (S shows it is safe in this experiment) or only while held.
Other held `upload()` callers (`reshow()`) are untested here.

## Classic (separate qualification, unchanged from the earlier report)

The same decision exists in `os9/main.cpp` (`copy(0, h, foreground_ && !pending)`). Host tests cannot
cover it; it needs owner-run G4 acceptance with a counter of foreground conversions during held update
events. Nothing from this X11 experiment qualifies it.
