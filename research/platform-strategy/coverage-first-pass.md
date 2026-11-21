# Evidence-aware coverage: preliminary findings (2026-11-21)

**Analysis type:** Read-only independent JS classification of the unchanged archival CSV using the same stated route rules as `coverage.py`. Python CLI/unit tests must still be executed in an actual checkout. All unverified candidates are hypotheses, not support.

## Coverage and uncertainty

- 71 distinct archived scenarios mapped (the original snapshot remains the source of truth).
- 106 **candidate route–scenario combinations**; do **not** count them as supported OS binaries.
- 20 niche, mobile, or nonstandard scenarios intentionally remain **platform-native feasibility unassessed**, rather than acquiring fictitious FLTK support.
- 3 scenarios have an additional Java 1.4 Reader candidate (Tiger/Leopard PPC, Win2000/XP, historical Solaris), contingent on separate runtime and AWT proof. An additional Java target is not a separate audience automatically.
- The two dated ledger entries cover **partial subsets only** (OS 9 PPC release; Fedora/Debian Linux Xvfb). **0 of 71 entire coarse scenarios** have full-scenario evidence. This is not a denial of the published Classic PPC release.
- The small six-path Pareto optimizer under PR #239 is **not yet a fully calibrated 71-target optimization engine**. It remains useful to test dependency logic, not to rank all supported platforms.

### Candidate route breakdown

| Hypothesis | Candidate scenarios |
| --- | ---: |
| FLTK Unix/Cocoa research | 30 |
| Xlib Unix | 34 |
| Win32/GDI | 7 |
| FLTK Windows | 2 |
| Cocoa native | 7 |
| Java 1.4 Reader | 3 |
| Classic Toolbox PPC | 1 |
| 68k vector only | 1 |
| WebAssembly Reader | 1 |
| Generic platform-native, feasibility unknown | 20 |
| **Total candidate routes** | **106** |

## What this changes

It exposes a substantial **evidence coverage gap** without inventing a false numerical platform reach. The next rational investment is in targeted verification and actual cost collection, not choosing a winning historical architecture from hypothetical RICE decimals.

**Research interpretation:** FLTK is the intended default Unix frontend only after qualifying each target. Xlib remains the present near-term Unix release path and possible compatibility fallback; no dual customer-facing package objective. Java 1.4 offers leverage only if *authentic* historical JVMs and a separately maintained geographic Reader prove practical. 68k vector work is a capability-tier experiment, not an automatic cut-down PPC build.

## Limits of inference

No empirical audience measurement, verified overlap structure, native performance, measured owner-time, probabilistic VOI, accepted platform-specific support certification, or complete 71-target Pareto calculation exists yet. The ledger is a review tool, not self-authenticating evidence: referenced artifacts must be checked by humans for their exact target/scope. Do not use route counts or hypothetical RICE scores as an autonomous merge/release recommendation.
