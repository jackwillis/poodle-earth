# Cursor affordances and native providers — proposed design

**Status:** Proposal for review, November 21, 2026; [tracking issue #242](https://github.com/jackwillis/poodle-earth/issues/242). **Not implemented, accepted, assigned to a release, or a release gate.** No authorization to change native hardware, merge, or alter milestone criteria.

## Problem and existing behavior

Poodle Earth already separates shared `CursorIntent` from native rendering. `cursor_for(model, pointer, target)` in `src/core/app.cpp` is deterministic; adapters apply it. On Classic Mac, `src/platform/os9/main.cpp` uses bundled 16×16 QuickDraw bitmaps via `SetCursor` and `InitCursor`. On X11, `XDefineCursor` uses X cursor-font glyphs.

Three mismatches warrant a design pass:

1. `CursorTarget { selection, object }` conflates a selected country with a selectable object. Once a country is selected, even the ocean requests a pointing hand just because clicking there clears selection.
2. The X11 glyph mapping collapses *OpenHand* and *PointingHand* to `XC_hand2` and represents *ClosedHand* with `XC_fleur` (move), not a visibly grasping hand.
3. The QuickDraw fallback fixed the pre-startup `AppearanceLib--SetThemeCursor` failure on Mac OS 8.1 ([#174](https://github.com/jackwillis/poodle-earth/issues/174)), but later Macs no longer receive their system theme cursors. Closed [#44](https://github.com/jackwillis/poodle-earth/issues/44) records successful earlier cursor behavior; do not reinterpret its acceptance as this redesign having been tested.

**Product rule:** the cursor predicts the **primary next interaction**. A secondary side effect (such as deselecting by clicking the sea) is not a reason to advertise every pixel as a link.

## Ownership boundaries

| Concern | Owner | Rule |
| --- | --- | --- |
| Camera, drag threshold and active drag, semantic commands | Portable `Model` / `Msg` / `update` | Remain authoritative; no native cursor handles or platform APIs |
| Measure mode, focused geographic entity | Current overlay/interaction owner | Reuse existing semantics; do not move all overlay state to Model merely for cursor work |
| Pointer, frontmost window, native chrome/text focus, pointer capture | Frontend adapter | Transient presentation state; no geographic generation change on mere pointer motion |
| Cheap hover target (e.g. existing city-marker pick) | Geographic presentation/hit-testing seam | Must correspond to actual current click behavior, with caching/invalidation; no expensive country polygon pick each motion |
| Desired cursor | Small pure shared policy | Platform-independent `CursorIntent`, derivable from an explicit snapshot |
| Cursor bitmap/theme activation | Frontend cursor provider | QuickDraw/Appearance Manager, Xlib or future FLTK; cache last successfully requested state |

The pure policy need not consume raw OS event objects or demand that every mouse movement dispatch `Msg`. For a cursor-only hover change, read the current Model and relevant overlay state, supply cheap hit context, derive an intent, and ask the native provider to set it **only when the effective intent changes**. If hover later changes a card, selected entity or other meaningful behavior, that change must follow its appropriate semantic event/Effects path.

Proposed types are **illustrative, not an approved new shared API**:

```cpp
enum class HitKind : std::uint8_t { GlobeBackground, SelectablePlace };
// The adapter has already handled native chrome, text and focus.
struct CursorContext {
    bool foreground;
    bool in_globe_window;
    bool in_globe_viewport;
    bool active_drag;
    bool measuring;
    HitKind hit;
};
CursorIntent cursor_for(const CursorContext& context);
```

Don't eagerly add `Country`, `Text`, `UIControl`, `Inspect` or stable entity IDs to the first contract without a demonstrated need. Native text I-beams and control affordances often belong to the host widget/window layer. A future semantic hover target can grow beyond a boolean when #220 tooltips or #37 readout require identity; keep picking and cardinality bounded.

## Proposed priority/transition rules

Apply the **current gesture and window ownership** before interpreting a geographic hover hit. This table is a UX proposal; settle exact captured-drag behavior with tests.

| Situation | Requested cursor | Explanation |
| --- | --- | --- |
| App inactive; another window in front; menu bar/window chrome; outside the interactive globe | Native/default arrow or native control cursor | Don't override other windows or the system |
| Globe is actively dragging, including a captured drag across the limb | Closed hand | Continue to describe the ongoing gesture; clear on release, loss of capture or suspend |
| Mouse button down but below existing drag threshold | Previous applicable hover/open-hand intent | Do not imply rotation before it actually starts |
| Existing Measure mode, within globe surface and not dragging | Crosshair (new intent, if accepted) | Indicate choosing a position; preserve the native click/measurement contract |
| Pointer on a cheaply and accurately recognized selectable city/place marker in Explore | Pointing hand | Clicking acts on the place |
| Pointer on background globe, land or ocean in Explore | Open hand | Primary affordance is drag-to-turn; ordinary click selection remains possible |
| Country already selected, pointer over empty ocean | Open hand | Clearing the selection is incidental |
| Text-entry context or future command palette input | Native I-beam | Managed by actual text widget/focus, not by geographic Model |
| Unknown/invalid target or lost pointer state | Conservative native arrow or open hand, by region | Never leave a stale pointing hand over invalidated geometry |

**Important ambiguity:** a click on country land may open a country card even while an open hand is shown. This first pass treats hand as the primary drag affordance rather than claiming country hover identity without an affordable pick. A later cheap land hit-test or explicit Inspect mode can refine this, but expensive polygon work on every move is not acceptable.

Recompute when relevant state changes **even if the pointer has not moved**: camera generation, viewport geometry, effective target, active/foreground window, selected tool, drag threshold, pointer capture and measure mode. Avoid duplicate expensive geographic picks and avoid cursor flicker during progressive refinement. Invalidation is not a reason for a geographic redraw.

## Optional Appearance Manager — Classic Mac compatibility

The shipped baseline remains QuickDraw `SetCursor` / `InitCursor` and existing 16×16 bitmaps. **Never restore a mandatory `AppearanceLib` or `SetThemeCursor` import.** This is a startup safety requirement, not merely a fallback after a failing API call.

Candidate mechanism on PowerPC CFM: resolve optional Appearance Manager availability and the actual `SetThemeCursor` entry at runtime through CFM `GetSharedLibrary` / `FindSymbol`, or evaluate a provably weak-linked import. **Do not select one mechanism without verifying the Retro68/Multiversal headers, PEF linkage, symbol names, ABI/calling convention, connection lifecycle and behavior when missing.** A Gestalt flag or operating-system version check by itself is insufficient: the required symbol may still be absent. Do symbol discovery once, not on every pointer move; treat failure or an unsupported theme-cursor result as a QuickDraw fallback. The platform provider can map known theme cursors only after testing the API's identifiers and return status.

Apple documentation on historical CFM weak linking ([TN1083](https://leopard-adc.pepas.com/technotes/tn/tn1083.html)), explicit fragment APIs ([Code Fragment Manager Reference](https://leopard-adc.pepas.com/documentation/Carbon/Reference/Code_Fragment_Manager/index.html)) and Appearance Manager ([reference](https://leopard-adc.pepas.com/documentation/Carbon/Reference/Appearance_Manager/index.html)) is background, **not proof the proposed bridge already compiles**.

Examine the actual built PowerPC PEF import table: no *required* AppearanceLib or SetThemeCursor import, even on a build machine that has AppearanceLib. Distinguish earliest System 7 PowerPC emulation, Mac OS 8.1 without the symbol, Mac OS 8.5/9 Appearance installations, and bare 9.2.2 G4 verification. Run only authorized emulators/hardware and identify exact guest, artifact and scope. No claim about 68k runtime follows from a 68k compile probe.

## Platform-specific rendering and icons

- **Classic Mac:** preserve QuickDraw glyph artwork as a tested fallback, use the OS theme when safely available; preserve correct hotspots, 1-bit masks and the system arrow outside the globe.
- **X11:** pick visually distinct glyphs or bundled cursor assets *only after checking availability on supported X servers*. The current `XC_hand2`/`XC_fleur` mapping is insufficient for the desired distinction.
- **FLTK:** a later target-dependent qualification maps the **same semantic intent** to native affordances. No FLTK build, runtime or release commitment follows.
- **Application icons / Tools palette:** do not conflate menu-button icons, app identity and native pointer glyphs. A command palette is *not* required to fix cursors; any future palette should dispatch the same validated semantic commands used by menus and scripting.

## Bounded implementation and verification plan

**Slice A — pure semantics (smallest independent improvement):** write the transition table as tests; refactor `CursorTarget` narrowly; fix selection-clearing ocean semantics; check active vs pending drag, window activation and suppression in Measure. Existing overlay/country click behavior must not change. Keep one shared policy used by Classic and X11.

**Slice B — native providers:** separately prototype optional Appearance Manager on PowerPC, inspect PEF imports, maintain QuickDraw fallback. Correct X11 glyph mapping independently. Neither change should force an Inspect tool or large shared presentation refactor.

**Slice C — acceptance:** host tests for every table row, stationary-pointer invalidation, button threshold, drag ending outside the globe, focus/suspend, missing place data, active measurement and menu transitions. Check X11 pointer appearance under a real X server/Xvfb where possible, inspect graphics using screenshots; inspect Classic exact preview builds and verify native cursor appearance on explicitly authorized target systems. Compare file size, runtime scratch, cursor-change count and per-motion cost; ensure no allocations, new frame generation or map redraw due only to cursor feedback.

For releases **after v0.1.4**, PoodleScripts may exercise state transitions and assert desired cursor intent via a future read-only test/diagnostics interface. It need not extend the scripting grammar in every release. Such a semantic assertion proves *requested intent*, not native glyph presentation: the latter requires OS-level evidence at the point it is observable.

## Scheduling and non-goals

Candidate **pre-v0.2 patch**, potentially v0.1.5 after the already selected v0.1.3 Unix and v0.1.4 PoodleScripts work. This document **does not assign that milestone or make it a v0.2 gate**. v0.2 should assess interaction coherence during integrated usability acceptance; later iconography, a full Inspect tool and a searchable command palette can be scoped separately.

Related: [#35](https://github.com/jackwillis/poodle-earth/issues/35), [#37](https://github.com/jackwillis/poodle-earth/issues/37), [#44](https://github.com/jackwillis/poodle-earth/issues/44), [#148](https://github.com/jackwillis/poodle-earth/issues/148), [#174](https://github.com/jackwillis/poodle-earth/issues/174), [#220](https://github.com/jackwillis/poodle-earth/issues/220), [#232](https://github.com/jackwillis/poodle-earth/issues/232).
