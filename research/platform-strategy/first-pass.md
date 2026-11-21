# Platform-strategy v0.1 — first illustrative portfolio comparison

**Date:** 2026-11-21. **Status:** Independent arithmetic/check of the committed *illustrative* catalog and archived CSV, not a runtime test of the Python CLI, not a demand forecast, not a platform support finding, not a roadmap decision.

## Reproduction contract

Model inputs: 71 archival OS/version/ISA rows, 10 synthetic investments, 6 exactly mapped target alternatives. Budgets: **30 hypothetical owner hours** and **20 hypothetical validation hours**. Enumerate 1,024 subsets, reject unsatisfied prerequisites, conflicting Linux frontend choices and blocked Tiger Cocoa candidate, then filter budgets and calculate dominance. Independent JavaScript evaluation in the GitHub-connected research session found **26 feasible portfolios** and **6 nondominated** (benefit proxy ↑, learning proxy ↑, maintenance hours ↓). This independently checks the catalog arithmetic; the committed Python CLI and unit tests have **not** been executed in this environment.

| Nondominated bundle | Activated hypothetical target(s) | Reach×impact×confidence proxy | Learning proxy | Maintenance h |
| --- | --- | ---: | ---: | ---: |
| FLTK Linux probe + FLTK frontend + NetBSD FLTK acceptance | Linux x86_64 FLTK; NetBSD/macppc FLTK | 234.18 | 11 | 7 |
| FLTK Linux probe + FLTK frontend + Java static gate + 68k vector probe | Linux x86_64 FLTK | 184.50 | 23 | 5 |
| FLTK Linux probe + Java static gate + 68k vector probe + Xlib Linux package | Linux x86_64 Xlib | 184.50 | 22 | 3 |
| FLTK Linux probe + Java static gate + Xlib Linux package | Linux x86_64 Xlib | 184.50 | 15 | 2 |
| FLTK Linux probe + Java static gate + 68k vector probe | None | 0 | 21 | 1 |
| FLTK Linux probe + Java static gate | None | 0 | 14 | 0 |

**Interpretation:** Learning investments can be Pareto-optimal even before shipping another target; selecting existing Xlib or future FLTK as alternative frontends prevents double-counting one Linux audience. The top hypothetical bundle is not a release recommendation: it depends on FLTK acceptance that has not occurred and uses invented effort/value proxies. Java 1.4 reader and complete 68k vector port do not activate under these budgets; that is a function of toy costs, not evidence against those architectures.

## Research integrity

- The existing 71-scene RICE input values are *hypotheses*, and audience overlaps between OS families remain unmeasured. The per-bucket maximum only prevents same-target frontend double counting.
- FLTK, Java, Tiger Cocoa, 68k and Xlib are not certified by being named in a portfolio. Blocked Tiger Cocoa investment is ineligible by default.
- Owner and validation hour budgets are illustrative, not estimates collected from production. Maintenance is a third Pareto axis. No probabilistic VOI or empirically calibrated AI multiplier is claimed.
- **Pending:** Run `python3 -m unittest discover -s research/platform-strategy -p 'test_*.py' -v` and `python3 research/platform-strategy/portfolio.py --sensitivity` in a real checkout; compare CLI output with this independent calculation. Document exact Python/version/platform results. No GitHub Actions workflow was introduced.

Next research task: collect real owner/CI/acceptance effort and target-specific verification evidence, especially the FLTK Linux #184 gate and PE-JAVA-00 #237, before numerical preference calibration.
