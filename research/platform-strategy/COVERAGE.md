# Platform coverage v0.2 — 71 historical scenarios, not 71 supported platforms

This is a **stacked research extension** to [draft PR #239](https://github.com/jackwillis/poodle-earth/pull/239), tracked in [#240](https://github.com/jackwillis/poodle-earth/issues/240). All 71 original OS/version/ISA records now have stable IDs, candidate *research* routes and evidence gates; no original RICE numbers are altered. **It does not establish compatibility or assign ports.**

## Run (standard-library Python, from repository root)

```sh
python3 research/platform-strategy/coverage.py
python3 research/platform-strategy/coverage.py --json > /tmp/poodle-platform-coverage.json
python3 -m unittest discover -s research/platform-strategy -p 'test_*.py' -v
python3 research/platform-strategy/portfolio.py --sensitivity
```

No new GitHub Actions workflow or non-FOSS dependency. The scripts do not write to the repository or connect to the network. The `portfolio.py` demo remains a **six-rule** optimization experiment; `coverage.py` now provides 71/71 coverage for evidence and route research. A *credible 71-target portfolio optimizer* must first collect target-specific resource costs and overlap evidence; do not optimize invented precision.

## Typed scenario routes

`coverage.py` derives 71 stable SHA-256 IDs from exact archival OS/family/version/ISA strings, never from mutable ranks. Every scenario gets one or more **candidate research routes**. Examples:

- Current Linux/BSD/illumos: FLTK Unix and/or Xlib candidates; old Linux may be Xlib-only.
- Windows: Win32/GDI research; modern Windows can also explore FLTK; Windows 2000/XP can explore independent Java 1.4.
- Mac OS X Tiger/Leopard PPC: separate Cocoa and Java Reader feasibility, not an automatic Cocoa or JVM support claim.
- Classic 68k: vector-only port idea; Classic PPC: existing Toolbox route.
- WebAssembly: browser Reader.
- 20 unusual/mobile targets: explicitly `platform-native` **unassessed**, not a fabricated FLTK/JVM compatibility claim.

A candidate route is an experiment hypothesis and **does not automatically activate any platform**. Toolkits, source compilation and package recipes cannot certify native installation or a supported release.

## Qualification evidence model

The `evidence.json` ledger has scoped, dated records linking target, route, revision, actual environment and source:

```
unknown
  -> source_compiled
  -> binary_built
  -> native_launched
  -> native_accepted
  -> package_accepted
  -> published
```

These are distinct claims, not a magic monotone conversion. `full_scenario: false` records evidence for **narrower configurations**, without upgrading the whole historical scenario. When `full_scenario: true`, the validator requires citations to prior stage evidence; publication additionally requires a publication URL and native validation. Human review must check that those citations genuinely support the claim. A reference to a build does not prove a GUI acceptance. No evidence is silently promoted.

Initial ledger entries are deliberately **partial**: bare OS 9.2.2/PPC release evidence and Linux x86_64 Xvfb smoke evidence. Neither covers the full archived multi-version scenario. Accordingly **zero entire 71-row scenarios are classified as accepted/supported** by this coarse-grained registry. This is a limitation of the scenario granularity, not a claim that Poodle Earth has never shipped.

## Calibration: collect observed costs instead of inventing them

Use `measurements-template.csv` as a header-only data contract. Collect actual owner review/integration/validation minutes, agent elapsed time, native environment, source revision and evidence link. **Do not** fill in zero for unknown effort. Timeboxes, aborted attempts and retries should be explicit rows, with a reason. Store redacted public records only, and keep owner-only device access outside Git.

The initial portfolio model's arbitrary cost and learning parameters remain **synthetic**; they are not recalibrated by this empty template. In later work, join evidence by scenario ID, route, revision and experiment. Avoid treating a Linux cross-build as Tiger/G4 evidence.

## Decision priorities / unresolved design questions

1. Validate actual Linux FLTK execution and compare matched Xlib runs [#183, PR #184].
2. Prove PE-JAVA-00 compiler/API/static negative controls and authentic historical JVM launch [#237]. Reuse reach across Tiger/XP/Solaris is a research opportunity, not a three-OS support claim.
3. Complete actual NetBSD pkgsrc native build/install/launch gates for v0.1.3 [#216/#212], separately from FLTK.
4. Benchmark the smallest native 68k line-only globe on a disclosed emulator target *after separate approval*.
5. Investigate FOSS-only Tiger Cocoa SDK/toolchain feasibility; explicitly record a no-go if it cannot be reproduced.

These are methodological priorities, **not** numerical expected value of information. The 71-row prior RICE cannot infer probabilities of success, audience overlap, AI time savings or costs of native hardware acceptance.

### Scope and authority

This is a research registry, not the release board. No user-facing platform claims, accepted ADR changes, G4 access, hardware acceptance, new licenses, changes to #212's milestone, or publication follows from this document. See [AGENTS.md](../../AGENTS.md).
