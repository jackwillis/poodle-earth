# Camera interaction study: KDE Marble and Poodle Earth

Research report for [#245](https://github.com/jackwillis/poodle-earth/issues/245) · 2026-11-21 · Claude (Claude Code, Sonnet 5.5) · read-only study, no source change

| Pinned revision | |
|---|---|
| Poodle Earth | `main` at `18725cf` (merge of #233), 2026-11-21 |
| KDE Marble | `62418529b228e4a119f7795b3209f369e93d62da` (`v24.01.90-903-g62418529b`, committed 2026-11-20), read from a local clone of `KDE/marble` |
| Host used for runs | Fedora Linux 7.2 x86-64, GCC 16.2.1, `-O1`. Not a G4. |

Evidence labels used throughout:

- **[S]** read directly from source at the pinned revision.
- **[D]** stated in upstream documentation (`doc/index.docbook` at the Marble pin; the web copy at docs.kde.org was not fetched).
- **[R]** measured by running the scratch harness in [Appendix A](#appendix-a-host-harness) on this host.
- **[H]** hypothesis, inferred and not measured.
- **[N]** not run.

No Marble code is copied or proposed for copying. Per-file licences in the Marble tree differ (for example `kineticmodel.cpp` is BSD-3-Clause, the input handler LGPL); `docs/licensing-policy.md` applies before any adoption, and none is proposed. Everything below is behaviour, numbers and our own algebra.

## 1. Findings in brief

1. **Marble's drag is not pointer-anchored.** It is a north-up turntable: pointer travel is converted to a longitude/latitude offset from the press-time centre, at 90° per globe radius [S, §3]. Marble's own manual says the "location below mouse pointer will follow the cursor exactly" mode would let north drift, and that the default north-up mode "slightly detaches from the cursor" [D]. The option for the anchored mode is hidden in the UI and has no implementation in the pinned tree [S].
2. **Poodle Earth already has Marble's drag model**, at a lower gain (57.3° per radius), plus an exactly anchored mode (Trackball) that the owner rejected on 2026-11-01 because it rolled the globe and behaved oddly at the rim (`docs/m1-plan.md:14`), after which the north-up turntable was confirmed to "feel right" (`docs/adr/0001-m2-to-m5-decisions.md:45`). At the view centre near the equator the Turntable tracks the pointer to 0.3 px [R].
3. **The measurable weakness of today's Turntable is off-centre and high-latitude drag.** Over a 40 px drag grid at 640×480 the grabbed point ends 8.7 px from the pointer on average at 0° latitude, 18.5 px at 60° and 29.3 px at 85°, with worst cases of 55–70 px [R, §5]. Marble's 90°-per-radius gain is worse on every row [R].
4. **What Marble does that Poodle does not**, and which plausibly adds to its feel: inertial spin after release (on by default), zoom about the pointer on wheel, and a travel animation (off by default on desktop) [S]. Their contribution to "intuitive" is [H]; no user study exists.
5. **A north-up camera can hold the grabbed point exactly under the pointer** in 334 of 384 grid cases (96.9% up to 40° latitude, 87% overall), with a closed form that is verified against Poodle's own camera to 1e-13 px and costs about 200 ns on this host [R]. This is the only change tested that removes the measured drift without leaving north-up. The simple fix people reach for, scaling longitude by 1/cos(latitude), makes the average worse [R].
6. **Recommendation: one host-only experiment on anchored north-up drag (§10).** Inertia, animated recentre (#41) and pivot zoom are listed as later, independent candidates with their costs. Doing nothing is a legitimate outcome and is kept as the baseline.

## 2. Poodle Earth today (shipped behaviour)

All references are to `18725cf`.

| Topic | Behaviour | Source |
|---|---|---|
| Camera | Orthonormal basis (`right`, `up`, `forward`), `radius_px`, viewport size and centre. Orientation is a full 3-D basis, so it can roll. | `include/globe/projection.hpp`, `Camera` |
| Drag modes | `DragMode::Turntable` (default) and `Trackball`; `T` toggles. | `include/globe/app.hpp:27-31`, `src/core/app.cpp:122-124` |
| Turntable | Per move: `lon -= dx/R`, `lat += dy/R` (radians per pixel = 1/radius), then rebuild a north-up camera with latitude clamped to ±89.9°. Incremental from the last pointer position. | `src/core/app.cpp:22, 52-69` |
| Trackball | Unproject the old and new pointer to the unit sphere (points off the disc project onto the rim), rotate about their cross product. Exactly anchored, rolls, speeds up at the rim. | `src/core/trackball.cpp:22-65` |
| Click versus drag | Press starts nothing visible. A drag begins when the pointer is more than 3 px from the press; the press position is `drag_last`, so nothing lags. A release with no drag is a click and picks after 400 ms. | `app.hpp:225-229`, `app.cpp:372-381, 387-400, 409-417` |
| Double click | Two presses within 400 ms and 6 px, second one on the globe: centre on the clicked point (north-up in Turntable, rotation in Trackball) and zoom ×2, in one jump. | `app.cpp:345-371`, `app.hpp:206-223` |
| Press off the globe | A single press still starts a drag. Only the double click checks `on_globe`. The cursor is an arrow off the disc. | `app.cpp:372-381`, `app.cpp:271-280` |
| Wheel and `+`/`-` | Zoom ×1.1 per step, about the **view centre**; no pointer pivot. Radius clamped to 0.2–8.0 of the short side. | `app.cpp:17-20, 38-49, 425-429` |
| Arrow keys | About 10° per press (Turntable) or an equivalent trackball rotation. | `app.cpp:23-24, 101-119` |
| Align north | Rotates about the view axis; centre unchanged. | `app.cpp:120-121` |
| Quality while moving | Camera change sets `Moving` and bumps `generation`; after 150 ms idle `Settle`, then refinement. A drag that pauses 300 ms refines but is not presented until release. | `app.cpp:91-99, 458-489, 525-532`, `app.hpp:231-241` |
| Zoom preview | A pure zoom about the centre scales the last finished frame instead of re-rendering (about 5 ms on the G4 per #41). It is only valid while the **orientation is unchanged**. | `include/globe/zoom.hpp`, `app.cpp:247-269` (`same_orientation`) |
| Input and time | Time arrives in messages; `update` never reads a clock. `Effects::wake_after_ms` is a single replaceable deadline. | `app.hpp:86-156` |
| X11 | Motion events are coalesced to the latest one; buttons 4/5 are the wheel. | `src/platform/x11/main.cpp:895-930` |
| Classic Mac | Pointer position is polled with `GetMouse` once per loop turn while dragging. No wheel (not delivered to non-Carbon programs). | `src/platform/os9/main.cpp:436-441`; `docs/os9-proof.md` ("Scroll wheel") |
| Kinetic spin, animation, reduced motion | **None.** | no code; #41 and #20 are open |

Proposals that are **not** shipped: Option-drag momentary trackball and Option-double-click zoom out (#35 comment, 2026-11-08, "unimplemented"); animated zoom and recentre (#41). The proposal keeps "Drag rotates, north-up (unchanged)".

## 3. KDE Marble at the pin (source)

All paths under `src/lib/marble/` at `62418529b`.

| Topic | Behaviour | Source |
|---|---|---|
| Drag formula | On press, store the pointer and the centre longitude/latitude. On each move: `Δlon = −(π/2)·dx/radius`, `Δlat = +(π/2)·dy/radius`, added to the **press-time** centre, then `centerOn`. The angle comes from `Quaternion::fromSpherical(...)` then `getSpherical`, which returns the same two angles. Gain is 90° per radius, 1.57× Poodle's. | `MarbleInputHandler.cpp:453-477, 719-743`; `Quaternion.cpp:31-52` |
| Pointer alignment | None beyond the north-up turntable. The `dragLocation` setting (`KeepAxisVertically`) exists in `marble.kcfg` and the `.ui` file but is hidden and has no consumer in `src/lib`. | `QtMarbleConfigDialog.cpp:109-110`; `MarbleGlobal.h:118`; grep of `src` |
| Rotation with heading | When the map has a heading (right-button drag), the offset is rotated by it before adding. | `MarbleInputHandler.cpp:732-736, 764-777` |
| Poles | `ViewportParams::centerOn` clamps latitude to ±90° for non-traversable projections (spherical is one). Longitude is wrapped. No special handling near ±90°, so longitude per pixel is unscaled there. | `ViewportParams.cpp:214-229`; `projections/SphericalProjection.cpp:34-45`; `AbstractProjection.cpp:91-94, 140-143` |
| Press outside the globe | Press events only start a drag when the pointer is over the map or a feature. | `MarbleInputHandler.cpp:708-714` |
| Pointer leaves the globe while dragging | The drag ends and kinetic spin starts; the cursor becomes one of eight directional arrows, and a press on those pans one step. | `MarbleInputHandler.cpp:626-672` |
| Click versus drag | Drag threshold 3 px (15 px on small screens), or any move once the 400 ms click timer has expired. | `MarbleInputHandler.cpp:202, 453-462, 725` |
| Inertia | On by default (`m_inertialEarthRotation(true)`; settings default true). Velocity is tracked in degrees per second as `0.2·old + 0.8·new`, sampled at most every half interval. Spin starts only if the last move was within two intervals (70 ms) of release. Deceleration is linear to zero over 1403 ms, update every 35 ms, each step capped at 100 ms of motion. | `MarbleInputHandler.cpp:59, 236-239, 521-532`; `kineticmodel.cpp:38, 86-110, 156-180, 182-232`; `QtMarbleConfigDialog.cpp:475-478` |
| Wheel zoom | Zoom about the pointer: find the surface point under it, compute the centre that keeps that point at the same screen position at the new radius, then fly there. Zoom is `200·ln(radius)`, and a wheel notch adds the raw angle delta (120 per notch, so ×e^0.6 ≈ 1.8 in radius per notch) [H on the notch size; arithmetic from `MarbleAbstractPresenter.cpp:40-48` and `MarbleInputHandler.cpp:318-351`, not run]. A 400 ms timer restores full quality. | `MarbleAbstractPresenter.cpp:309-349`; `MarbleInputHandler.cpp:318-351` |
| Double click | `moveTo`: centre on the clicked point at 0.67 of the current range (about ×1.5 in). | `MarbleInputHandler.cpp:304-316`; `MarbleAbstractPresenter.cpp:351-366` |
| Animated travel | `MarblePhysics::flyTo`. `Linear` runs 300 ms (ease-out); `Jump` runs 1000 ms (2000 ms when the target is off screen or hidden), interpolating the centre by quaternion slerp along the great circle and the range along a parabola. `Automatic` picks `Jump` for hidden targets or a range change over 10, else `Linear`. **Animations are off by default on desktop** (`animateTargetVoyage` defaults to the small-screen flag); with them off, `flyTo` is instant. | `MarblePhysics.cpp:20-92, 94-135`; `MarbleAbstractPresenter.cpp:27-35, 63-96`; `QtMarbleConfigDialog.cpp:469-473`; `MarbleGlobal.h:161-166` |
| Quality while moving | Press, drag and wheel switch the view context to `Animation` (lower map quality); `Still` is restored on release, at the end of a spin, or 400 ms after the last wheel step. This is the analogue of Poodle's `Moving`/`Settle`. | `MarbleInputHandler.cpp:67-69, 286-292, 453-458` |
| Heading (roll) | Right-button drag rotates the map's heading; "Mouse view rotation" is a documented setting. Poodle's equivalents are Trackball roll and `N`. | `MarbleInputHandler.cpp:750-778`; manual [D] |
| Documented UI | "Inertial globe rotation" (default on), "Animate voyage to the target", "Mouse view rotation", and the commented-out "Drag location" text quoted in §1. | `doc/index.docbook:866-899` [D] |

## 4. Why Marble feels intuitive (and what it does not explain)

Question 1 asked why Marble's drag feels intuitive and how its "pointer-aligned" rotation differs from Poodle's. The source answers part of that:

- **It is not pointer-aligned.** Marble and Poodle's Turntable are the same family: constant angle per pixel, north-up. Calling Marble's drag "globe-surface anchored" is wrong for this revision [S, D].
- **Gain.** Marble turns the globe 1.57× further per pixel at any zoom. With the grid in §5 that is *further* from the pointer than Poodle's, on average [R]. Higher gain feels more responsive, not more anchored; whether it feels better is [H].
- **Inertia.** The spin carries the globe after release, which hides the stop-start of drag-release-drag. This is the largest behavioural difference in the drag path, on by default [S].
- **Zoom about the pointer.** Wheel zoom keeps the thing under the pointer under the pointer [S]. Poodle zooms about the centre, so the point of interest drifts toward the rim and has to be re-dragged. A fair candidate for a feel difference [H].
- **Quality while moving.** Both trade quality for frame rate during motion. Poodle already does this, and skips the coarse refine levels after a pure zoom [S].
- **Not established.** Frame rate, the Qt event pipeline and long-time familiarity with the application all affect feel and are outside what source reading can show [N].

## 5. Measured drift on the host

Setup [R]: 640×480 viewport, home-sized globe (R = 192 px), centre at the stated latitude and longitude 0. Press points: the centre, then eight directions at 0.4 R and 0.8 R. Drags of 40 px right, left, down and up, one `PointerDown` and one `PointerMove` through the real `update()`. Cases whose end point lies beyond 0.97 R are dropped. The measure is the distance between the pointer and where the grabbed surface point appears after the drag. Harness and commands are in Appendix A.

**Turntable, Trackball and Marble's gain** (mean / max drift in px):

| Centre lat | Turntable (today) | Trackball (today) | Turntable at Marble's 90°/R |
|---:|---:|---:|---:|
| 0° | 8.7 / 18.5 | 0.0 / 0.0 | 11.8 / 21.7 |
| 20° | 10.3 / 28.8 | 0.0 / 0.0 | 14.0 / 26.5 |
| 40° | 14.0 / 42.2 | 0.0 / 0.0 | 17.4 / 46.0 |
| 60° | 18.5 / 55.6 | 0.0 / 0.0 | 21.9 / 64.3 |
| 75° | 22.2 / 64.6 | 0.0 / 0.0 | 24.9 / 78.3 |
| 85° | 29.3 / 69.6 | 0.0 / 0.0 | 32.9 / 86.3 |

Single cases from a first scratch version of the harness (one press at the centre; that version is not reproduced in Appendix A): a 40 px rightward drag from the exact centre drifts 0.3 px at 0° but 20.2 px at 60° and 36.5 px at 85°, because at latitude φ a pixel of east-west travel moves the surface point only `cos φ` as far along the screen. A 40 px downward drag from the centre drifts 0.3 px at every latitude up to 60° [R].

**Longitude scaled by min(cap, 1/cos φ)** (mean / max):

| Centre lat | Today (cap 1) | Cap 2 | Cap 4 |
|---:|---:|---:|---:|
| 40° | 14.0 / 42.2 | 14.1 / 43.5 | 14.1 / 43.5 |
| 60° | 18.5 / 55.6 | 23.7 / 73.2 | 23.7 / 73.2 |
| 75° | 22.2 / 64.6 | 27.5 / 88.5 | 44.7 / 138.2 |
| 85° | 29.3 / 69.6 | 35.1 / 98.6 | 53.2 / 155.8 |

Result: **scaling by latitude fails.** It is exact only for the point at the view centre; every other point moves along a differently curved parallel, so the average gets worse. This candidate is closed [R].

**North-up camera that holds the grabbed point** (closed form, Appendix A): reachable cases per latitude, and the worst residual after placing the camera:

| Centre lat | Reachable | | Centre lat | Reachable |
|---:|---:|---|---:|---:|
| 0° | 64 / 64 | | 60° | 56 / 64 |
| 20° | 62 / 64 | | 75° | 50 / 64 |
| 40° | 60 / 64 | | 85° | 42 / 64 |

Total 334 / 384, worst residual 1.1e-13 px, no hidden-hemisphere or residual failures [R]. The unreachable cases are where no north-up centre can show the grabbed point at the pointer (near the poles and rim); there the current Turntable must be the fallback. Host cost of one solve including `make_camera`: about 200 ns [R, this host]. On the G4, using the libm costs recorded in the project's portability notes (`sin`/`cos` 0.3 µs, `atan2` 0.6 µs, `sqrt` 1.2 µs, `asin`/`acos` not measured), the same call is on the order of 10–30 µs [H], against about 30 ms for a moving-frame vector redraw (#41) and a 20 ms input check. Not measured there, and not to be without owner authorisation.

## 6. Interaction comparison matrix

"Not shipped" means designed or proposed in #35/#41 and absent from `main`.

| Interaction | Poodle Earth `18725cf` | Proposed, not shipped | Marble `62418529b` |
|---|---|---|---|
| Drag model | North-up turntable, 1 rad per radius; optional trackball | None to change north-up (#35) | North-up turntable, π/2 rad per radius [S]; anchored mode hidden, unimplemented |
| Grabbed point stays under pointer | Trackball exactly; Turntable approximately (§5) | n/a | No (manual says it "detaches") [D] |
| Click versus drag | 3 px slop; click picks after 400 ms | n/a | 3 px (15 small screen) or timer |
| Double click | Centre + ×2, instant | Option-double-click zooms out (#35); animation (#41) | Centre at ×1.5 in; instant unless voyage animation is on |
| Wheel / keys zoom | ×1.1 per step about centre | Menu items and keys (#35) | About pointer, ≈×1.8 per notch [H] |
| Arrow keys | 10° per press | n/a | Step by `moveStep` (10° or less) |
| Inertia | None | None proposed | On by default; linear decel over 1.4 s |
| Animated recentre/zoom | None | #41 (open, no prototype) | Slerp great circle, 300 ms or 1–2 s; off by default on desktop |
| Align north | `N` | n/a | Right-drag heading; reset by the user |
| Poles | Latitude clamped to ±89.9° | n/a | Clamped to ±90°; no longitude compensation |
| Limb | Trackball clamps to the rim; Turntable has no rim notion | n/a | Press requires the pointer on the map |
| Outside the globe | Press starts a drag (arrow cursor) | n/a | No drag; leaving ends the drag and starts spin; arrow pan |
| Quality while moving | Moving → Settle (150 ms) → Refine; presentation held while dragging | n/a | `Animation` context while pressed/zooming; `Still` after |
| Reduced motion | None | #20 (open) | `animateTargetVoyage` and `inertialEarthRotation` settings |
| Modifiers | None for camera | Option-drag trackball (#35) | Ctrl-drag selection rectangle |
| Right button | Unused | Never essential (#35) | Rotates heading |

## 7. Answers to the research questions

1. **Why does Marble's drag feel intuitive; how does it differ?** Not through anchoring (§4). It is a turntable like Poodle's, with 1.57× the gain, and its inertia and pointer-pivot zoom are the likelier contributors. Poodle's exact-anchor mode is the Trackball, which the owner found odd. The untested middle is an anchored mode that keeps north up (§5, §10).
2. **Zoom, recentre, navigation to a selected place.** Today every one is an instant jump. Marble's recentre is a quaternion slerp of the centre with a parabolic range; its default on desktop is instant. For Poodle, a navigation plan is two cameras and an interpolation parameter: compute the axis and angle once when navigation begins, then `camera(t) = rotate(start, axis, angle·ease(t))` per Tick, which is a few trig calls per step. Each step is a real frame, because a rotation breaks the zoom preview (§2). A pure zoom step can still use the preview (about 5 ms on the G4 per #41). Pivot zoom needs a rotation per step as well, so it costs real frames and is useful on X11 only (the Classic wheel is absent). Design in §8.
3. **Is kinetic rotation desirable; costs on slow machines?** Plausibly desirable for X11, unproven for Poodle. Marble's spin spends up to 1403 ms / 35 ms ≈ 40 frames per flick. At about 30 ms per moving-frame redraw on the G4 [per #41, hypothesis for spin], that is a continuous 100% busy loop for 1.4 s after every flick. A budgeted design: compute the release velocity and total glide distance once at `PointerUp`, step along a closed form by elapsed time (so the final camera does not depend on frame rate), cap the step count, stop on any input, and fall back to a fixed-length glide of at most a few frames when the machine is slow. Needs release samples in the model (two timestamped positions) and uses the existing replaceable `wake_after_ms`.
4. **Poles, limb, outside the globe.** Poodle clamps to ±89.9° and keeps longitude defined; Marble clamps to ±90° and does nothing more. Neither compensates, and the grid shows the drift grows with latitude (§5). Recommended behaviour: keep the clamp; a press off the globe should not start an anchored drag (it can keep today's Turntable behaviour); where an anchored solution does not exist, fall back to today's behaviour for that move; do not add inertia that could carry the view past the pole.
5. **What can be done independently, without redesign?** Four changes touch separate parts of `update` and can ship or be rejected independently: anchored north-up drag (one function plus one stored point), inertia (release samples plus a glide plan, uses `Tick`), animated recentre (a plan plus `Tick`, §8; this is #41), and pivot zoom (X11 only; breaks the preview for its steps).
6. **Platform split.** See §9.

## 8. Alternatives

Each candidate is independently adoptable. "Cost" is hypothesis for the G4 unless noted.

| # | Candidate | UX benefit | Model / Msg / Effects | CPU, memory | Fallback and accessibility | Verdict |
|---|---|---|---|---|---|---|
| A | **Baseline** (change nothing) | Known and liked at home view | none | none | n/a | Valid outcome; the control for every test |
| B | **Marble gain** (π/2 per radius) | Faster globe, but drift is worse everywhere [R] | constant only | none | none | Not recommended; evidence against |
| C | **Latitude-scaled turntable** | none (worse) [R] | one expression | none | none | Closed |
| D | **Anchored north-up drag** | Grabbed point stays under the pointer where reachable (97% up to 40° [R]); keeps north up | one stored unit vector set when a drag begins; replaces the body of `turntable_drag`; no new Msg, Effect or timer | one closed-form solve per move; 0 allocations; 24 bytes of Model | falls back to today's behaviour when unreachable or press off the globe; Trackball and keys unchanged | **Recommended experiment (§10)** |
| E | **Restrained inertia** | Smooth release; matches Marble's default | release samples (two points and times) and a glide plan in the Model; steps ride `Tick`; any input cancels | up to ~40 full moving frames per flick at the cap Marble uses; a smaller cap or a time budget is the control [H] | off by default or off under #20 reduced motion; needs a Preferences flag, which means consent and storage rules (#42, #49) | Later; depends on D's outcome and #20 |
| F | **Animated recentre/zoom** (#41) | Orientation after double click or selection | plan (start camera, axis, angle, start time, duration) in the Model; `Tick` advances it; input cancels like a refinement | one real frame per step (rotation), zoom-only steps can use the preview [per #41] | instant under reduced motion; final camera identical to the jump | Later, inside #41 |
| G | **Pivot zoom** (wheel about the pointer) | Keeps the area of interest under the pointer | `Wheel` carries the position (a field added to the existing message) | rotation per step, so each step is a real frame; preview unusable | X11 only; keys and menu keep centre zoom | Later; X11 follow-up |

## 9. Platform responsibilities

- **Portable core (all front ends).** The camera maths, drag anchor, any glide or travel plan, thresholds, and the bookkeeping for cancel and settle. No platform headers, no clock, no exceptions or RTTI, bounded loops (`AGENTS.md` §5; `docs/cpp-guidelines.md`). Time comes in through message `time_ms`.
- **Classic Mac OS (Toolbox).** Supplies pointer samples per loop turn (`GetMouse` polling, no per-event timestamps beyond the loop clock), one-button mouse, no wheel, Option as the only modifier idiom proposed in #35. Inertia samples would be as sparse as the loop turns, so any velocity estimate must be tolerant of irregular spacing. Cursors: `cursor_for` already gives the intent; the front end draws it. Wake: a single replaceable deadline passed to `WaitNextEvent`.
- **Xlib.** Supplies coalesced motion, buttons 4/5 for the wheel (so pivot zoom belongs here), real timestamps, and a `select` deadline. Coalescing means fewer velocity samples than a raw stream. It is the reference platform for host tests and the first place an owner A/B can run.
- **Future FLTK.** Same core messages; no assumption of features identical to Classic. Whether to offer inertia or travel animation per platform is a front-end preference, not a core one.
- **Not platform-specific:** the anchored-drag maths, the glide and travel plans, and the "final camera is identical however the intermediate frames were delivered" rule.

## 10. Recommended experiment: anchored north-up drag (host only)

**Scope.** In Turntable mode only: when a drag begins on the globe, store the unit vector of the grabbed surface point in the Model (the same vector the Trackball already computes via `unproject_to_sphere`). On each move compute the north-up centre that puts that point at the pointer, using the closed form in Appendix A, and build the camera with the existing `make_camera`. If no centre exists (§5), or the press was off the globe, use today's incremental Turntable step for that move. Nothing else changes: Trackball, arrow keys, double click, wheel, picking and refinement are untouched. No new `Msg`, no new `Effects` field, no timer, no allocation, no preferences, no disk write, no data. Camera state stays one function of the pointer.

**Why this and not the others.** It is the only candidate that removes measured error rather than adding motion. It needs no animation state, so it cannot interact with settled-only labels, reduced motion, or the G4's frame budget. It is independent of #41 and does not preempt any of #35's gestures. Inertia and travel animation can be judged afterwards against a drag that already feels attached.

**Where it lives.** A new, separately scoped, owner-reviewed implementation issue under #35, not inside #41. It touches drag feel, which the owner has called "great" at the home view, so it needs owner readiness before an agent implements it.

**Pre-registered success criteria** (all on host, no G4):

| # | Criterion | Measure |
|---|---|---|
| S1 | Accuracy | Harness grid: mean drift at 40° at least 70% below today's 14.0 px (≤ 4.2 px) and at 60° at least 55% below 18.5 px (≤ 8.3 px), counting the fallback cases as today's drift |
| S2 | No surprise at the home view | For a press at the centre and a 40 px drag at 0° to 20° latitude, the grabbed point lands within 4 px of where today's Turntable puts it (today's own drift there is 0.3 to 3 px); existing `test_app` Turntable and double-click tests pass unmodified, or each change is explained and approved |
| S3 | Continuity | On a recorded sweep that crosses the reachable boundary, no single move changes the camera by more than twice today's turn for the same pointer step (angle comparison) |
| S4 | Determinism | Replaying one trace twice gives bit-identical cameras; replaying it with moves thinned to every 5th and with jittered `time_ms` gives the same final camera |
| S5 | Safety | Latitude stays within ±89.9°; no NaN or non-finite value for any trace in §11, including presses off the globe and at the poles; policy script and the Retro68 probe compile clean |
| S6 | Cost | Per-move cost on the host no more than 2 µs (measured 0.2 µs for the solve alone); no heap use in the path (checked by the existing ledger or by inspection) |

**Failure and stop conditions.** The experiment is a no-go and today's Turntable stays if any of S1 to S5 is missed, or if S3 fails in a way that needs a blend function (that would be new design, not this experiment), or if the owner, shown both on X11 in a blind A/B, prefers today's behaviour. Stopping at S1 to S6 passing but before an owner A/B is a valid result: report numbers and ask.

**Owner decisions needed.** (1) Readiness for a scoped implementation issue; the #35 note "drag rotates north-up (unchanged)" is honoured, but the pointer-to-surface mapping changes. (2) Whether the A/B happens on X11 first, with G4 acceptance a separate, later authorisation. (3) Whether off-centre stickiness is acceptable given the 2026-11-01 verdict on the Trackball (that was about roll and rim speed; this mode does neither, but the owner should say).

## 11. Host test plan

The current build already supports scripted replay (`include/globe/trace.hpp` verbs `down`, `move`, `up`, `wheel`, `key`, `tick`, `resize`; `tests/data/bench_drag.txt` is an example; `docs/g4-benchmark.md:113` shows the `globe-bench` replay command). The cases below are not run as traces in this study; only the harness cases in §5 were run [N].

| # | Case | Setup | Pass (for any change) |
|---|---|---|---|
| T1 | Equator drag | centre lat 0, drag 40 px from centre and from 0.8 R | grabbed point within the drift of today's Turntable or better |
| T2 | High latitude | centre lat 60, 75, 85 (set `Model::camera` with `make_camera`, as the harness does) | drift not worse than today; lat clamp holds |
| T3 | Pole crossing | drag toward and across the pole from 80° | no NaN, no longitude flip larger than the pointer step explains |
| T4 | Near the limb | press at 0.97 R, drag along and across the rim | no jump; unreachable cases fall back |
| T5 | Press outside the globe | `down 0 10 10` at 640×480 (corner is off the disc), drag inward | behaves as today (no anchor taken) |
| T6 | Click versus drag | `down`/`up` with moves of 2.9 px, then 3.1 px | 2.9 px is a click (model unchanged), 3.1 px a drag |
| T7 | Fast flick | 12 moves 8 ms apart, 30 px each, then `up` | final camera identical in any `time_ms` spacing; no glide today |
| T8 | Wheel while dragging | `down`, `move`, `wheel T 1`, `move`, `up` | zoom takes effect, drag continues, no stale anchor radius |
| T9 | Low frame rate | the same drag with moves every 100 ms | same final camera as dense moves |
| T10 | 640×480 | all of the above at `--size 640x480` | as above |
| T11 | Second input during a voyage | needs #41; not testable on `main` | later |
| T12 | Reduced motion | needs #20; today everything is instant | later |
| T13 | No writes | run each trace under `strace -f -e trace=openat,write` or an equivalent | no files created or modified by the camera path |

Reproduce the §5 numbers from the repository root (about 10 seconds):

```
g++ -std=c++20 -O1 -Iinclude marble-camera-245-harness.cpp src/core/app.cpp src/core/trackball.cpp \
    src/core/projection.cpp src/core/geometry.cpp -o harness
./harness drift; ./harness compensate; ./harness anchor; ./harness timing
```

(save Appendix A as `marble-camera-245-harness.cpp`).

## 12. Limitations and unrun items

- No GUI was run. Nothing here says how any variant *feels*; "intuitive" is not measured, and the §4 attributions are hypotheses.
- Nothing ran on a G4 or Classic Mac OS; all G4 cost statements are hypotheses derived from recorded project numbers (#41, `docs/os9-proof.md`, the portability notes).
- `globe-bench`, the project test suite (`ctest`), the policy script and the Retro68 probe were **not run** for this report. The harness compiles only the core files named above.
- Marble was read, not run; its behaviour on a real display (frame rate, effective inertia feel) is inferred from source.
- The docs.kde.org web page was not fetched; the manual text is from `doc/index.docbook` at the pin.
- The anchored solution is derived and checked numerically on a grid, not proven; the grid does not include presses at the exact rim or at exactly ±90°.
- The §5 grid is synthetic (uniform polar sampling of press points, 40 px steps), not recorded human drags.
- Wheel notch size (§3) is arithmetic on source constants, not observed.
- Unknown: how much of Marble's perceived smoothness comes from inertia versus gain versus frame rate; no user study exists for either application.

## Appendix A: host harness

Scratch program, `GPL-3.0-or-later`, compiled against the pinned `main` core only. It does not modify the repository.

```cpp
// Research harness for issue #245 (scratch; not built by the project, not shipped).
// Question: after a drag, how far is the surface point grabbed at press from the pointer?
//   drift      current Turntable, current Trackball, and a Marble-gain turntable (90 deg per radius)
//   compensate Turntable with longitude scaled by 1/cos(lat) (capped)
//   anchor     closed-form north-up centre that puts the grabbed point under the pointer
//   timing     host cost of the closed form
// Build (from the repo root):
//   g++ -std=c++20 -O1 -Iinclude harness.cpp src/core/app.cpp src/core/trackball.cpp
//       src/core/projection.cpp src/core/geometry.cpp -o harness && ./harness drift
#include "globe/app.hpp"
#include "globe/trackball.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
using namespace globe;

constexpr int W = 640, H = 480;
constexpr double kLats[] = {0, 20, 40, 60, 75, 85};

static Vec3 world_under(const Camera& c, ScreenPoint p) {
    const Vec3 u = unproject_to_sphere(c, p);
    return c.right * u.x + c.up * u.y + c.forward * u.z;
}
static Camera cam_at(const Camera& like, double lat, double lon) {
    lat = std::clamp(lat, -89.9, 89.9);
    return make_camera({{lat, lon}, 0.0, like.radius_px, like.width, like.height}).value_or(like);
}
static Camera start_camera(double lat) { return make_camera({{lat, 0}, 0.0, 0.4 * H, W, H}).value(); }
static double drift_px(const Camera& before, const Camera& after, ScreenPoint p0, ScreenPoint p1, bool& visible) {
    const Vec3 cs = to_camera_space(after, world_under(before, p0));
    visible = cs.z > 0.0;
    const ScreenPoint s = to_screen(after, cs);
    return std::hypot(s.x - p1.x, s.y - p1.y);
}
// Gain in radians of rotation per radius of pointer travel; 1.0 is today's Turntable.
static Camera turntable(const Camera& c0, ScreenPoint a, ScreenPoint b, double gain, double lon_comp_cap) {
    LatLon c = center_of(c0);
    double f = 1.0;
    if (lon_comp_cap > 1.0) f = std::min(lon_comp_cap, 1.0 / std::max(1e-9, std::cos(c.lat_deg * kDegToRad)));
    const double k = gain * kRadToDeg / c0.radius_px;
    c.lon_deg -= (b.x - a.x) * k * f;
    c.lat_deg += (b.y - a.y) * k;
    return cam_at(c0, c.lat_deg, c.lon_deg);
}
// North-up centre that puts G at screen point p; false when no such centre shows G.
static bool anchored(const Camera& c0, Vec3 g, ScreenPoint p, double cur_lat_deg, Camera& out) {
    const LatLon G = from_unit_vector(g);
    const double sp = std::sin(G.lat_deg * kDegToRad), cp = std::cos(G.lat_deg * kDegToRad);
    const double x = (p.x - c0.center_x) / c0.radius_px, y = (c0.center_y - p.y) / c0.radius_px;
    if (std::fabs(x) > cp) return false;
    const double D = std::asin(x / cp), K = cp * std::cos(D);
    const double A = std::sqrt(sp * sp + K * K), beta = std::atan2(K, sp);
    if (A < 1e-12 || std::fabs(y) > A) return false;
    const double a = std::acos(y / A);
    bool have = false;
    double best = 0, bestd = 1e9;
    for (double s : {+1.0, -1.0}) {
        double p0 = s * a - beta;
        while (p0 > M_PI) p0 -= 2 * M_PI;
        while (p0 < -M_PI) p0 += 2 * M_PI;
        if (std::fabs(p0) > M_PI / 2) continue;
        if (std::sin(p0) * sp + std::cos(p0) * cp * std::cos(D) <= 0) continue; // G would be hidden
        const double d = std::fabs(p0 * kRadToDeg - cur_lat_deg);
        if (d < bestd) { bestd = d; best = p0; have = true; }
    }
    if (!have) return false;
    out = cam_at(c0, best * kRadToDeg, G.lon_deg - D * kRadToDeg);
    return true;
}
// Press points: the centre, then 8 directions at 0.4 R and 0.8 R; drags of 40 px right/left/down/up.
template <class F> static void grid(const Camera& c0, F&& f) {
    for (double r : {0.0, 0.4, 0.8})
        for (int a = 0; a < 8; ++a) {
            if (r == 0.0 && a > 0) continue;
            const double th = a * M_PI / 4;
            const ScreenPoint p0{c0.center_x + r * c0.radius_px * std::cos(th), c0.center_y + r * c0.radius_px * std::sin(th)};
            for (int d = 0; d < 4; ++d) {
                const ScreenPoint p1{p0.x + (d == 0 ? 40 : d == 1 ? -40 : 0), p0.y + (d == 2 ? 40 : d == 3 ? -40 : 0)};
                if (std::hypot(p1.x - c0.center_x, p1.y - c0.center_y) > 0.97 * c0.radius_px) continue;
                f(p0, p1);
            }
        }
}
int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "drift";
    if (!std::strcmp(mode, "drift")) {
        std::printf("mean/max drift (px) over the grid; R=192, %dx%d\n%4s | %-15s | %-15s | %-15s\n", W, H, "lat",
                    "turntable", "trackball", "marble 90deg/R");
        for (double lat : kLats) {
            const Camera c0 = start_camera(lat);
            double s[3] = {}, mx[3] = {};
            int n = 0;
            grid(c0, [&](ScreenPoint p0, ScreenPoint p1) {
                bool v;
                double e[3];
                for (int m = 0; m < 2; ++m) {
                    Model md = initial_model(W, H);
                    md.camera = c0;
                    md.drag_mode = m == 0 ? DragMode::Turntable : DragMode::Trackball;
                    update(md, PointerDown{1000, p0.x, p0.y});
                    update(md, PointerMove{1010, p1.x, p1.y});
                    e[m] = drift_px(c0, md.camera, p0, p1, v);
                }
                e[2] = drift_px(c0, turntable(c0, p0, p1, M_PI / 2, 0), p0, p1, v);
                for (int m = 0; m < 3; ++m) { s[m] += e[m]; mx[m] = std::max(mx[m], e[m]); }
                ++n;
            });
            std::printf("%4.0f | %5.1f / %5.1f | %5.1f / %5.1f | %5.1f / %5.1f\n", lat, s[0] / n, mx[0], s[1] / n, mx[1], s[2] / n, mx[2]);
        }
    } else if (!std::strcmp(mode, "compensate")) {
        std::printf("Turntable with longitude x min(cap, 1/cos(centre lat)): mean/max drift px\n%4s | %-15s | %-15s | %-15s\n", "lat",
                    "cap 1 (today)", "cap 2", "cap 4");
        for (double lat : kLats) {
            const Camera c0 = start_camera(lat);
            double s[3] = {}, mx[3] = {};
            int n = 0;
            grid(c0, [&](ScreenPoint p0, ScreenPoint p1) {
                bool v;
                const double caps[3] = {1, 2, 4};
                for (int m = 0; m < 3; ++m) {
                    const double e = drift_px(c0, turntable(c0, p0, p1, 1.0, caps[m]), p0, p1, v);
                    s[m] += e; mx[m] = std::max(mx[m], e);
                }
                ++n;
            });
            std::printf("%4.0f | %5.1f / %5.1f | %5.1f / %5.1f | %5.1f / %5.1f\n", lat, s[0] / n, mx[0], s[1] / n, mx[1], s[2] / n, mx[2]);
        }
    } else if (!std::strcmp(mode, "anchor")) {
        std::printf("closed-form north-up anchored centre: reachable cases and worst residual\n");
        int tn = 0, tok = 0, bad = 0;
        double worst = 0;
        for (double lat : kLats) {
            const Camera c0 = start_camera(lat);
            int n = 0, ok = 0;
            grid(c0, [&](ScreenPoint p0, ScreenPoint p1) {
                ++n;
                Camera out;
                const Vec3 g = world_under(c0, p0);
                if (!anchored(c0, g, p1, lat, out)) return;
                ++ok;
                const Vec3 cs = to_camera_space(out, g);
                const ScreenPoint s = to_screen(out, cs);
                const double res = std::hypot(s.x - p1.x, s.y - p1.y);
                worst = std::max(worst, res);
                if (res > 0.25 || cs.z <= 0) ++bad;
            });
            std::printf("lat %3.0f: %2d/%2d reachable\n", lat, ok, n);
            tn += n; tok += ok;
        }
        std::printf("total %d/%d reachable; worst residual %.3g px; failures %d\n", tok, tn, worst, bad);
    } else if (!std::strcmp(mode, "timing")) {
        const Camera c0 = start_camera(40);
        const ScreenPoint p0{c0.center_x + 60, c0.center_y - 30};
        const Vec3 g = world_under(c0, p0);
        Camera out = c0;
        double sink = 0;
        const int N = 1000000;
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < N; ++i) {
            anchored(c0, g, {p0.x + (i & 31) * 0.5, p0.y + 20}, 40, out);
            sink += out.forward.x;
        }
        const auto t1 = std::chrono::steady_clock::now();
        std::printf("anchored(): %.0f ns/call on this host (%s); sink %.3g\n",
                    std::chrono::duration<double, std::nano>(t1 - t0).count() / N, "includes make_camera", sink);
    }
}
```
