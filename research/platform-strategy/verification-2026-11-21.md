# Local reconstructed-source verification — 2026-11-21

**Status:** Python execution evidence for the research-model code, **not CI, native application testing or a cryptographically verified Git checkout**.

## Method and provenance

The session used the GitHub connector to read the text of these draft-branch files and copied their logic into a fresh local container workspace under `/mnt/data/poodle_portfolio_checkout`:

- `research/platform-strategy/portfolio.py`, `assumptions.json`, `test_portfolio.py` from draft [PR #239](https://github.com/jackwillis/poodle-earth/pull/239);
- `research/platform-strategy/coverage.py`, `evidence.json`, `test_coverage.py` from stacked draft [PR #241](https://github.com/jackwillis/poodle-earth/pull/241);
- `docs/research/platform-rice-scenario-inputs-2026-11.csv` from the project snapshot, reconstructed as 71 records.

The container could not resolve `github.com` for a direct `git clone`, so this **was not a Git checkout**; local files were transcribed from remote text, not verified against the Git blob hashes. An initial transcription discrepancy in the *unused* Genode/Sculpt historical effort value was corrected to the source's 6.2 before the final test run. Do **not** treat these tests as an exact-revision reproducibility certificate.

## Observed execution

Environment: Linux container, **Python 3.13.5**.

```sh
cd /mnt/data/poodle_portfolio_checkout
python3 -m unittest discover -s research/platform-strategy -p 'test_*.py' -v
python3 research/platform-strategy/coverage.py --json
python3 research/platform-strategy/portfolio.py --sensitivity
```

- **17 unit tests passed**, including prereqs/conflicts, sample Pareto dominance, 71 unique scenario IDs, partial-evidence protection and promotion-stage guards.
- Coverage CLI: **71 scenarios**, **106 candidate research routes**, all 71 whole-scenario claim levels `unknown`; two intentionally partial ledger records.
- Baseline synthetic budgets (owner 30 h, validation 20 h): **6 nondominated portfolios**; highest hypothetical benefit proxy **234.18**.
- Owner 15 h / validation 20 h: **4 nondominated portfolios**, highest proxy **184.5**.
- Owner 30 h / validation 10 h: **3 nondominated portfolios**, highest proxy **184.5**.
- Owner 45 h / validation 30 h: **9 nondominated portfolios**, highest proxy **234.18**.

The model is *sensitive to budgets* as intended; these values are **hypothetical priors and illustrative costs, not measured demand, forecast benefit, feasibility or platform support**.

## Still required before merge or decision use

1. Execute the same commands in an **actual checkout of exact PR heads**, capturing Python version, commit hashes and test output. Establish CI only through an expressly authorized workflow change; none is added here.
2. Review the evidence ledger's citations for their narrow scope. URL presence is syntactic proof, not verification that the linked material demonstrates the claimed stage.
3. Replace synthetic owner/validation and learning values with timestamped observations. The portfolio optimizer still has **six detailed target alternatives**, while the separate registry classifies all 71. There is not yet a calibrated 71-target portfolio solver.
4. No release, G4, archived RICE, accepted ADR, licensing or product support change is requested.
