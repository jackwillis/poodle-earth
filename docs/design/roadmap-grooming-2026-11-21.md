# Poodle Earth — unmilestoned backlog grooming and v0.3–v0.8 roadmap proposal

**Status:** Proposed for owner review — not adopted; no GitHub milestone/issue metadata changed.
**Snapshot:** 2026-11-21, live GitHub issue-search responses; 49 open unmilestoned issues, 19 open v0.3 issues, 9 open v0.4 issues. Counts exclude PRs and closed issues. Repository activity may change after snapshot.
**Authority:** [AGENTS.md](../../AGENTS.md), [live issues](https://github.com/jackwillis/poodle-earth/issues), [backlog policy](../backlog.md), [product direction](../design/product-direction-2026-11-10.md). GitHub milestone metadata and explicit owner decisions override this proposal.

## Decision requested
Adopt the *planning method* and later approve specific issue moves and any new milestone descriptions. **This PR is documentation only.** It does not create milestones, set due dates, assign issues, enable CI, select interpreters, approve data licenses, change accepted ADRs, impose new platform support claims or release gates, or authorize hardware work.

## Working rules
1. **Product milestone:** a memorable user-visible outcome plus explicit integration/acceptance. An issue belongs to one release only once approved; a research dependency does not become a mandatory gate by mention.
2. **Cross-release horizontal work:** architecture, platform/CI, rendering and memory improvements can land as bounded slices in any patch; track with umbrella issues and links, not giant release blockers.
3. **Research:** independent feasibility investigation with measurable stop conditions; never silently promote a prototype into support.
4. Preserve minor-release functional parity across owner-approved supported frontends, with explicitly recorded exceptions, and per-artifact native/release acceptance; patch releases may carry documented parity debt (see [backlog policy](../backlog.md)).
5. Owner attention, native verification, integration and maintenance are scarce despite AI-assisted coding. No work can silently touch the G4 or published release metadata.

## Release sequence — established vs suggested

| Version | Standing | Product promise / proposed narrow outcome | Candidate core or evidence |
| --- | --- | --- | --- |
| v0.1.3 | Existing release scope; verify live milestone | Unix distribution, reproducible/source-complete packaging and README; maintain agreed scope | [#212](https://github.com/jackwillis/poodle-earth/issues/212), [#230](https://github.com/jackwillis/poodle-earth/issues/230), [#231](https://github.com/jackwillis/poodle-earth/issues/231), [#234](https://github.com/jackwillis/poodle-earth/issues/234) |
| v0.1.4 | Roadmap scope, formal milestone/assignments need reconciliation | PoodleScripts acceptance and geographic tours; no mandatory general-purpose interpreter | [#232](https://github.com/jackwillis/poodle-earth/issues/232), #221–225; cross-platform features only when separately verified |
| v0.2 | Existing thematic milestone | Coherent geographic UI; labels, readout, meaningful interaction, copyable sourced cards | Live milestone and [#35](https://github.com/jackwillis/poodle-earth/issues/35), [#38](https://github.com/jackwillis/poodle-earth/issues/38), [#114](https://github.com/jackwillis/poodle-earth/issues/114) research |
| **v0.3** | **Existing: Memory and Packs** | Safe optional external resources within bounded memory; read-only volume and meaningful failure/degradation states | [#26](https://github.com/jackwillis/poodle-earth/issues/26), [#48](https://github.com/jackwillis/poodle-earth/issues/48), [#49](https://github.com/jackwillis/poodle-earth/issues/49), [#55–60](https://github.com/jackwillis/poodle-earth/issues/55); first complete on-target lifecycle |
| **v0.4** | **Existing: The Expandable Atlas** | A reproducible, richer, distributable offline full/regional edition, with provenance and actual load/install evidence | [#50](https://github.com/jackwillis/poodle-earth/issues/50), [#51](https://github.com/jackwillis/poodle-earth/issues/51); edition source qualification and [#72](https://github.com/jackwillis/poodle-earth/issues/72); optional visuals only after core |
| **v0.5** | **New candidate, not created** | **Living Atlas:** a bounded offline geographic hypermedia showcase (e.g. Wisconsin places and links) using accepted identity/knowledge foundations | [#168](https://github.com/jackwillis/poodle-earth/issues/168), existing [#137](https://github.com/jackwillis/poodle-earth/issues/137); small curated example before encyclopedic ingestion |
| **v0.6** | **New candidate, not created** | **Portable Workbench:** shared semantics and one properly qualified additional native frontend/environment, rather than every platform | [#183](https://github.com/jackwillis/poodle-earth/issues/183), [#114](https://github.com/jackwillis/poodle-earth/issues/114), [#247](https://github.com/jackwillis/poodle-earth/issues/247); native install/launch evidence |
| **v0.7** | **Directional candidate only** | **Geographic Workstation:** an approachable first useful geographic tool (e.g. radius, reach or antipode), opt-in expert tools | [#169](https://github.com/jackwillis/poodle-earth/issues/169); keep UI usable without scripting |
| **v0.8+** | **Research horizon only** | Further editions and platforms chosen through measured portfolio/maintenance evidence | [#176](https://github.com/jackwillis/poodle-earth/issues/176), [#178](https://github.com/jackwillis/poodle-earth/issues/178), [#237](https://github.com/jackwillis/poodle-earth/issues/237), [#238](https://github.com/jackwillis/poodle-earth/issues/238), [#240](https://github.com/jackwillis/poodle-earth/issues/240) |

**Naming note:** “Full Atlas” is a user-facing narrative for the existing v0.4 **Expandable Atlas** scope, not a silent renaming of that milestone. “Living Atlas,” “Portable Workbench,” and later names are proposals only.

## v0.3 review: classify current 19 open issues, do not automatically move

**Candidate integration path:** #17 immutable data/workspaces → #26 handles → #48 external Classic loader → #49 read-only behavior → #55 ledger → #56 budget ladder / #57 eviction → #58 low-memory fallback → #59 diagnostics → #60 stable host memory gate. The edges are planning hypotheses, not a claim that all steps are unimplemented or strictly sequential. Confirm existing PR/merge evidence per issue before work.

| Type | Current v0.3 issues | Grooming decision proposal |
| --- | --- | --- |
| Core pack lifecycle | #17, #26, #29, #31, #48, #49 | Keep, but scope first end-to-end read-only load and safe rejection path; don't require network |
| Memory/correctness | #10, #55, #56, #57, #58, #59, #60, #105 | Keep relevant parts; #105 is an audit and should not become an unbounded acceptance gate |
| User-facing complements | #24, #42, #43, #62 | Optional narrow slices or proposed deferral after owner review, not mandatory memory blockers |
| Civil time | #173 | Propose deferring to v0.5+ or independent research due to dataset/licensing and timezone correctness; do not incorporate ODbL data without owner approval |

**Graduation illustration:** on actual supported target, load a vetted optional pack from a read-only edition; use it, evict it, handle corrupt/absent packs and memory pressure without crash; preserve embedded offline baseline. Collect evidence per released artifact and frontend. Not adopted as an additional gate until owner acceptance.

## v0.4 review: classify current 9 open issues

| Type | Current v0.4 issues | Grooming decision proposal |
| --- | --- | --- |
| Offline full edition | #50, #51, #72, #122 | Prioritize reproducible edition build, notices/corresponding source, disk-image install/load tests and optical-media behavior evidence |
| Visual and data detail | #25, #30, #75 | Optional bounded visual/data experiments; require measured usefulness and cost before promotion |
| Fetching | #28 | Optional *after* a fully usable offline edition; consent, TLS and source policy are independent decisions |
| Developer console | #63 | Optional research; interpreter is unselected, and PoodleScripts does not imply a Lisp/Forth dependency |

**Graduation illustration:** approved-source full/regional edition has reproducible pack generation, accurate source/license records and works from a realistic distribution medium on the target. No new online dependency is imposed.

## Cross-release horizontal slices (examples, not gates)

| Release | Small compatible investment |
| --- | --- |
| v0.1.3 | Packaging path coverage, Unix package qualification, GCC floor investigation |
| v0.1.4 | PoodleScripts acceptance scenarios with exact semantics and transcript provenance |
| v0.2 | Frontend interaction parity and bounded native visual proof |
| v0.3 | Allocation-failure, resource IDs, bounded packs and diagnostic evidence |
| v0.4 | Edition install/load tests and data provenance reproducibility |
| v0.5 | Cross-frontend offline entity/card navigation tests |
| v0.6 | One FLTK or other proved native frontend; build proof distinct from runtime support |
| v0.7+ | Portability matrix additions only after evidence and maintenance review |

Use [#247](https://github.com/jackwillis/poodle-earth/issues/247) for workstream coordination and [#249](https://github.com/jackwillis/poodle-earth/issues/249) for tiered CI strategy. Neither becomes a mandatory release gate simply by being referenced.

## Complete snapshot: all 49 unmilestoned open issues

Disposition language: **candidate** means suggest owner evaluation, not an automatic milestone change. Cross-release epics, research and coordination are deliberately permitted to remain unassigned.

| Issue | Live title | Proposed disposition |
| --- | --- | --- |
| [#249](https://github.com/jackwillis/poodle-earth/issues/249) | Propose tiered, evidence-driven CI strategy and release-by-release verification slices | Keep unmilestoned — cross-release strategy and research |
| [#247](https://github.com/jackwillis/poodle-earth/issues/247) | Plan continuous platform workstreams without expanding release gates | Keep unmilestoned — cross-release strategy and research |
| [#245](https://github.com/jackwillis/poodle-earth/issues/245) | Research: KDE Marble camera feel and direct-manipulation alternatives | Early patch/v0.2 candidate (not automatic) |
| [#242](https://github.com/jackwillis/poodle-earth/issues/242) | Design a coherent cursor-intent contract with optional Classic Appearance Manager | Early patch/v0.2 candidate (not automatic) |
| [#240](https://github.com/jackwillis/poodle-earth/issues/240) | Research: v0.2 evidence-aware coverage for all 71 platform scenarios | Keep unmilestoned — cross-release strategy and research |
| [#238](https://github.com/jackwillis/poodle-earth/issues/238) | Research: reproducible platform portfolio model v0.1 (71 scenarios, dependencies, Pareto) | Keep unmilestoned — cross-release strategy and research |
| [#237](https://github.com/jackwillis/poodle-earth/issues/237) | PE-JAVA-00: verify Java 1.4 build/API compatibility and authentic JVM launch | Keep unmilestoned — cross-release strategy and research |
| [#232](https://github.com/jackwillis/poodle-earth/issues/232) | v0.1.4: ship PoodleScripts, acceptance exercise and geographic tours | v0.1.4 scope reconciliation |
| [#225](https://github.com/jackwillis/poodle-earth/issues/225) | Script output: say in Save As that only new files are created | v0.1.4 scope reconciliation |
| [#224](https://github.com/jackwillis/poodle-earth/issues/224) | Script JSONL: add monotonic elapsed time and optional start time | v0.1.4 scope reconciliation |
| [#223](https://github.com/jackwillis/poodle-earth/issues/223) | Classic: run a script by dropping it on the Globe window | v0.1.4 scope reconciliation |
| [#221](https://github.com/jackwillis/poodle-earth/issues/221) | Classic: first cooperative Run Script workflow and bounded JSONL transcript | v0.1.4 scope reconciliation |
| [#212](https://github.com/jackwillis/poodle-earth/issues/212) | Release roadmap: v0.1.3 Unix distribution + source CD; v0.1.4 PoodleScripts | Keep unmilestoned — cross-release umbrella |
| [#183](https://github.com/jackwillis/poodle-earth/issues/183) | Prototype a small FLTK Unix globe workbench (Linux; future NetBSD/macppc) | v0.6 candidate — earlier horizontal experiments allowed |
| [#178](https://github.com/jackwillis/poodle-earth/issues/178) | Research: WebAssembly CPU Reader as a browser and future renderer testbed | Keep unmilestoned — cross-release strategy and research |
| [#176](https://github.com/jackwillis/poodle-earth/issues/176) | Research: Win9x cross-compilation and native Win32 vs FLTK frontend feasibility | Keep unmilestoned — cross-release strategy and research |
| [#169](https://github.com/jackwillis/poodle-earth/issues/169) | Idea: Overlay quick wins (radius and reach rings, antipode) | v0.7+ candidate |
| [#168](https://github.com/jackwillis/poodle-earth/issues/168) | Idea: Wisconsin hypermedia showcase (Great Lakes lighthouses first) | v0.5 candidate |
| [#167](https://github.com/jackwillis/poodle-earth/issues/167) | Idea: 1-bit and engraving-style looks (hatching, dither styles) | v0.4 optional visual experiment |
| [#166](https://github.com/jackwillis/poodle-earth/issues/166) | Idea: Tracker and docs hygiene automation (consistency lint, generated STATE.md, docs checker) | v0.3 candidate |
| [#164](https://github.com/jackwillis/poodle-earth/issues/164) | Idea: CI matrix hardening (GCC 12, big-endian run, pinned toolchain) | v0.3 candidate |
| [#165](https://github.com/jackwillis/poodle-earth/issues/165) | Idea: Pack hardening (reader fuzzing, schema-first format pilot, allocation-failure injection) | v0.3 candidate |
| [#157](https://github.com/jackwillis/poodle-earth/issues/157) | Research: fully FOSS Classic Mac runtime smoke tests in Linux CI | Keep unmilestoned — cross-release strategy and research |
| [#153](https://github.com/jackwillis/poodle-earth/issues/153) | Calibrate population rules and the country-local selection threshold (follow-up to #91) | v0.4 data qualification |
| [#152](https://github.com/jackwillis/poodle-earth/issues/152) | Adjudicate the duplicate place groups from the #91 research (follow-up to #91) | v0.4 data qualification |
| [#151](https://github.com/jackwillis/poodle-earth/issues/151) | Verify capital and territorial-center identities for country-aware selection (follow-up to #91) | v0.4 data qualification |
| [#148](https://github.com/jackwillis/poodle-earth/issues/148) | Pre-v0.2: native multi-window workspace (Globe, Information and Looks/Tools) | Early patch/v0.2 candidate (not automatic) |
| [#147](https://github.com/jackwillis/poodle-earth/issues/147) | Prototype subtle camera-relative globe lighting as a final visual refinement | v0.4 optional visual experiment |
| [#145](https://github.com/jackwillis/poodle-earth/issues/145) | Research precision-aware math primitives and PowerPC reciprocal-square-root acceleration | Keep unmilestoned — cross-release strategy and research |
| [#126](https://github.com/jackwillis/poodle-earth/issues/126) | Plan v0.2 repository modernization and simpler developer builds | Early patch/v0.2 candidate (not automatic) |
| [#120](https://github.com/jackwillis/poodle-earth/issues/120) | Classic: application icon | Early patch/v0.2 candidate (not automatic) |
| [#114](https://github.com/jackwillis/poodle-earth/issues/114) | Research: shared presentation model and native frontend semantic parity | Early patch/v0.2 candidate (not automatic) |
| [#90](https://github.com/jackwillis/poodle-earth/issues/90) | Review and prune low-value UI controls after sidebar integration | Early patch/v0.2 candidate (not automatic) |
| [#54](https://github.com/jackwillis/poodle-earth/issues/54) | Epic: Memory budgets and runtime resource management | Keep unmilestoned — cross-release umbrella |
| [#53](https://github.com/jackwillis/poodle-earth/issues/53) | Audit atlas coverage and prototype source-driven editions | v0.4 data qualification |
| [#52](https://github.com/jackwillis/poodle-earth/issues/52) | Epic: Atlas production, geographic integrity and edition rules | Keep unmilestoned — cross-release umbrella |
| [#32](https://github.com/jackwillis/poodle-earth/issues/32) | Evaluate OS 9 wheel input and Carbon feasibility | Early patch/v0.2 candidate (not automatic) |
| [#23](https://github.com/jackwillis/poodle-earth/issues/23) | Complete native geographic interaction parity and layers | Early patch/v0.2 candidate (not automatic) |
| [#22](https://github.com/jackwillis/poodle-earth/issues/22) | Benchmark dirty-region repair against canonical repaint | Keep unmilestoned — cross-release strategy and research |
| [#21](https://github.com/jackwillis/poodle-earth/issues/21) | Prototype adaptive presentation only after UX baseline | Keep unmilestoned — cross-release strategy and research |
| [#19](https://github.com/jackwillis/poodle-earth/issues/19) | Research adaptive, instant and expressive refinement presentation | Keep unmilestoned — cross-release strategy and research |
| [#16](https://github.com/jackwillis/poodle-earth/issues/16) | Extract shared render lifecycle coordination | Keep unmilestoned — cross-release strategy and research |
| [#15](https://github.com/jackwillis/poodle-earth/issues/15) | Name refinement phases; preserve behavior | Keep unmilestoned — cross-release strategy and research |
| [#6](https://github.com/jackwillis/poodle-earth/issues/6) | Epic: Release engineering and verification | Keep unmilestoned — cross-release umbrella |
| [#5](https://github.com/jackwillis/poodle-earth/issues/5) | Epic: Offline data and acquisition | Keep unmilestoned — cross-release umbrella |
| [#4](https://github.com/jackwillis/poodle-earth/issues/4) | Epic: Geographic discovery and visual design | Keep unmilestoned — cross-release umbrella |
| [#3](https://github.com/jackwillis/poodle-earth/issues/3) | Epic: Portable application architecture | Keep unmilestoned — cross-release umbrella |
| [#2](https://github.com/jackwillis/poodle-earth/issues/2) | Epic: Responsive progressive rendering | Keep unmilestoned — cross-release umbrella |
| [#1](https://github.com/jackwillis/poodle-earth/issues/1) | Epic: Classic Macintosh experience | Keep unmilestoned — cross-release umbrella |

## Dependencies, risk and stopping rules

- **Data:** stable identity, correct geography and data-license provenance precede a curated knowledge/detailed regional edition; keep externally sourced ids distinct from the globe's own ids.
- **Resource safety:** allocation failures, corrupt/absent pack behavior, safe eviction, read-only preferences and the embedded atlas all need independent tests before full-edition expansion.
- **Portability:** classify *compile*, *test*, *native launch*, *package*, *feature behavior* and *hardware acceptance* separately. No Java/68k/Win9x/browser claim from compilation alone.
- **Performance:** bounded host instrumentation first; G4 access only with owner authorization. Reject optimizations whose quality/cost or regression evidence fails.
- **Prioritization:** prefer one useful end-to-end slice over an entire epic; defer if a task would expand scope, source-license policy, owners' attention budget, or platform claims.
- **Stop:** pause any milestone promotion when actual GitHub metadata, latest owner decisions, accepted ADRs, CI or target evidence conflicts with this snapshot.

## Suggested owner review sequence (no writes performed)

1. Reconcile current v0.1.3/0.1.4 issue/milestone metadata with #212/#232; don't disturb accepted scope.
2. Approve a *single* v0.3 essentials-versus-optionals list; inspect completion/PR status for #26/#48/#55–60 before any actual move.
3. Approve v0.4 offline-edition-first scope; explicitly decide #28 and #63 treatment.
4. Approve or rename v0.5 and v0.6 **descriptions only**; create milestones only after owner agreement and then selectively assign implementation issues.
5. Leave v0.7+ directional; revisit after a proven v0.4 edition and platform portfolio evidence.
6. For every accepted move, log origin, destination, rationale, owner approval and acceptance/parity impact on the affected issue.

**Recommended next task:** owner review of v0.3 mandatory/optional scope and the #173 timezone deferral. It gives the largest reduction in potential release-gate inflation without requiring implementation, hardware or license decisions.


## Owner-approved v0.3 tightening — November 22, 2026

**Decision:** v0.3's defining deliverable is *reliable external offline data within bounded memory*, not a general feature bundle. The owner approved the scope tightening in the project steering conversation. **This section supersedes the preceding speculative v0.3 disposition where they differ.** It is a release-scope planning decision; GitHub issue milestone changes remain separately auditable.

### Mandatory end-to-end release outcome

On an accepted target, launch with the embedded baseline, load a vetted optional local pack, use its detail, navigate away and evict as needed, return and reload, and degrade safely with a missing, corrupt or unaffordable pack. Ordinary interaction stays responsive, no network is required, and read-only distribution does not assume writes beside the application.

**Keep in the v0.3 critical path (scope only as necessary for that demonstration):** #26 stable handles, #48 Classic external pack lifecycle (already in review; do not take over its PR), #49 read-only-volume behavior, #55 ledger, #56 automatic budget/degradation ladder, #57 safe eviction and lazy loading, #58 low-memory/failure behavior, #59 bounded metrics, #60 deterministic memory regression; #10 partitions/requirements and #17 immutable-data/mutable-workspace boundary feed acceptance. #29 and #31 contribute only the bounded format/cache requirements necessary for vetted packs and may retain broader nonblocking investigations. #105 contributes targeted parser/error/allocation tests, not a blanket audit-completion gate.

**Release candidate sequence (bounded vertical slices):**
1. **A: load and fallback.** Document and qualify one real optional pack through resource identity, correct Classic file-path handling, corruption rejection and read-only use. No network, downloader or compressed-format redesign prerequisite.
2. **B: account and bound.** Track peak counted memory and demonstrate a fixed budget and safe eviction while retaining the embedded world.
3. **C: survive failures.** Inject allocation, I/O, malformed-pack and cancellation failures; verify no dangling handles, crash or unbounded modal alerts.
4. **D: evidence.** Run deterministic host tests and appropriate native Classic/X11 functionality, G4/OS 9 memory acceptance only with separate machine authorization, and exact released-package/source verification.

**Optional, not v0.3 graduation blockers, and approved for removal from v0.3 assignment when possible:**
- #173 Time on Earth: defer to a later geographic-knowledge/overlay milestone (v0.5 *candidate*, not a new data-license or release promise). Keep offline timezone/tzdb/polygon and ODbL qualification in the issue.
- #43 Save/copy view: opportunistic patch or separate UX milestone; no v0.3 gate.
- #42 Remember view/look: opportunistic patch; coordinate consent and #49 Preferences path, but persistence is not the pack-release gate.
- #24 Expanded place search: v0.4 *candidate* aligned with larger editions; not essential for pack memory safety.
- #62 Activity Log window: opportunistic patch; #59 instrumentation itself remains v0.3 essential; the GUI viewer does not.
- #105 broad mutation/audit closeout: leave targeted correctness tests in the v0.3 path, but do not require all research completion.

**Explicit non-goals for v0.3:** timezone overlays, HTTPS download/distribution (#28), arbitrary full-edition data volume (#50), a general script interpreter (#63), predictive resource management, all-platform ports, or visual polish unrelated to an accepted safe pack lifecycle. Such work can ship earlier independently if authorized, without altering the v0.3 graduation story.

### Issue metadata reconciliation checklist

The live issue tracker still assigns #173, #43, #42, #24 and #62 to v0.3 at the decision point. Remove their v0.3 milestone assignments or move to a *formally approved existing* appropriate milestone; do **not** create or assume v0.5 solely to move #173. Keep these issues open, their work preserved and their priorities/statuses unchanged. The connected GitHub issue update action currently cannot clear milestones; record this as an *outstanding metadata operation* until verified through an authorized action. Avoid falsely claiming the official milestone has been tightened until the moves actually land. Also inspect #105's milestone: its audit scope is nonblocking, even if the issue remains under v0.3 for coordination.

### Tests and acceptance boundaries

This document is planning, not a build or qualification run. A merged implementation PR is not sufficient on its own to certify the end-to-end workflow. Actual native tests, hardware observations, source/data licenses, and release authorization remain separate. Keep v0.3 optional improvements as opportunities, not automatic "status: ready" assignments.
