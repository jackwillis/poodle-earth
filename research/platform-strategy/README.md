# Platform strategy research model v0.1 (2026-11-21)

**Status:** Runnable, illustrative research prototype. **Not** an accepted strategy, support matrix, observed adoption forecast, release gate, or automated decision-maker. Refs [#238](https://github.com/jackwillis/poodle-earth/issues/238), [#213](https://github.com/jackwillis/poodle-earth/issues/213), [#236](https://github.com/jackwillis/poodle-earth/pull/236), [#237](https://github.com/jackwillis/poodle-earth/issues/237) and [#212](https://github.com/jackwillis/poodle-earth/issues/212).

## Quick start

From the repository root, using only Python's standard library:

```sh
python3 research/platform-strategy/portfolio.py
python3 research/platform-strategy/portfolio.py --sensitivity
python3 research/platform-strategy/portfolio.py --owner-budget 15 --validation-budget 12
python3 -m unittest discover -s research/platform-strategy -p 'test_*.py' -v
```

The model reads the **unchanged canonical 71-row CSV** at `docs/research/platform-rice-scenario-inputs-2026-11.csv`, plus the deliberately small, auditable `assumptions.json`. The package-channel CSV (45 rows) is deliberately **not** combined with target benefits because format/channel reach overlaps platform reach.

## What this initial slice actually does

- Checks CSV scenario uniqueness and exact target selectors.
- Models investments as a typed prerequisite graph using `requires_all` and `requires_any`; rejects invalid/cyclic edges.
- Enumerates every feasible bundle of at most 20 investments, counting shared investments once.
- Screens by **owner-attention** and **validation** hours separately.
- Constructs a nondominated/Pareto set for **hypothetical reach×impact×confidence**, **learning proxy**, and **recurring maintenance hours** (minimized).
- Deduplicates competing frontends for the same illustrative audience bucket; e.g. Linux Xlib and Linux FLTK never produce two independent Linux x86_64 audiences.
- Compares different budgets via `--sensitivity`; results are reproducible and include the full selected-investment set.

There is no magic “AI speed multiplier.” The owner-time estimates in `assumptions.json` are manually chosen examples to be **replaced by measurements**. They do not represent project estimates or grants to work. No optimizer output is an evidence gate or release assignment.

## How to interpret output

`targets` means *hypothetically activated in a portfolio*, **not** built, runnable, accepted or shipped. All six target rules are exploratory illustrations with explicit archived CSV selectors, **not a complete decision model for all 71 operating-system combinations**. The broader 71 scenarios are loaded, validated, and available for extensions. In particular:

- FLTK is the owner's **intended future Unix default**, with Xlib as a compatibility path where FLTK is infeasible; shipping both on the same target is **not** a product goal. Both appear in one research bundle solely to verify audience deduplication, not to propose dual distribution.
- The Java 1.4 Reader is a **new implementation** with static checking, AWT and geographic Reader prerequisites, not an automatic shared C++ frontend. [PE-JAVA-00](https://github.com/jackwillis/poodle-earth/issues/237) is separate.
- A Tiger Cocoa *probe* is **not** evidence of Tiger GUI support; SDK licensing/toolchain questions remain.
- A vector-only 68k engine/port is speculative: Retro68 compile checks are not a shipping 68k application.
- The proposed FLTK source in [PR #184](https://github.com/jackwillis/poodle-earth/pull/184) has not demonstrated a completed FLTK-enabled runtime test.

## Limitations and required next iteration

1. **Typed capabilities are simplified into investments and prerequisites.** Add versioned OS/ISA/ABI predicates, AND/OR hyperedges and evidence-backed capabilities before operational ranking.
2. **Hypothetical audience buckets are an overlap *placeholder*.** Other cross-platform audience overlaps remain unknown. Max-per-bucket is conservative within each manually assigned bucket, not a statistically justified set union; the RICE reach values are fictional priors.
3. **The benefit surrogate is not utility.** It deliberately uses the archival `reach × impact × confidence` values without dividing by legacy effort; budgets now provide the resource constraint. Neither formula is a market forecast.
4. **Resource estimates are illustrative.** Validation costs are kept separate; maintenance is a separate Pareto axis. Add exclusive owner decision/review/integration accounting, per-machine queues, AI run spend and cash costs based on observations rather than inventing them.
5. **No probabilistic value of information yet.** `learning_proxy` is an explicit subjective score, **not** expected value of sample information, probability of successful ports or empirical confidence.
6. **No support evidence promotions.** Add revision-bound proof records and strict build → run → accepted → packaged → published gates before any output can qualify a platform.
7. **Not a full architecture selection solver.** Mutually exclusive *customer-facing frontends* and constraints need first-class representation; this pilot only prevents a benefit double count, not selecting both implementations.

**Best next measurement:** run the existing #183/#184 pinned Linux FLTK build and compare paired Xlib observations, then replace placeholder costs and uncertainty. Java #237 is a distinct experiment; neither determines v0.1.3 scope. No G4 use, release changes or migrations from this prototype.
