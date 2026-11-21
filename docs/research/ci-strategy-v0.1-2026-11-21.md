# PE-CI-01 v0.1 — tiered verification and platform evidence

**Poodle Earth | Proposed strategy | 2026-11-21**
**Status:** Owner-authorized *documentation proposal*, **not adopted policy, activated automation, a release gate or a platform support claim**. Tracking [#249](https://github.com/jackwillis/poodle-earth/issues/249). The owner reviews any subsequent changes to required checks, schedules, runners, protected status checks, release scope and hardware use.

## Decision question and objective

What is the smallest maintainable set of automated checks that protects a portable C++20 geographic core, two currently working native presentation families, and new release artifacts while preserving confidence in old hardware?

**Thesis:** optimize for coverage of distinct *failure modes and verified claims*, not a Cartesian product of compiler x OS x architecture x frontend x edition. Perform cheap deterministic checks at every relevant pull request, broader targeted checks on demand, exact-artifact qualification before publication, and protected native-hardware acceptance when required. No cross-build, simulator pass, pkgsrc metadata check or semantic assertion can substitute for a different evidence category.

Follow [AGENTS.md](../../AGENTS.md), the existing [release checklist](../release-checklist.md), the approved release-direction record [#212](https://github.com/jackwillis/poodle-earth/issues/212), the related horizontal platform workstream [#247](https://github.com/jackwillis/poodle-earth/issues/247), and the existing [toolchain](../toolchain.md) and [architecture](../architecture.md) contracts. This proposal supplements rather than supersedes them.

## 1. Factual baseline (current `main` at `18725cfe9f6a4b1d61d3f6968ebe32310fc93ab1`)

| Existing workflow | What it actually defines | Gap / caution |
| --- | --- | --- |
| `.github/workflows/checks.yml`: `host` | Ubuntu 24.04 GCC, warnings, ASan/UBSan, CTest, repository policy, release-manifest unit tests, clang-format/tidy, full-edition size report | No explicit GCC 12 portability-floor job. This describes workflow configuration, **not** an independently confirmed current green run |
| `checks.yml`: `clang` | Ubuntu 24.04 Clang compile and CTest | Same Linux host family; not independent NetBSD or hardware runtime acceptance |
| `checks.yml`: `retro68` | Retro68 PPC and 68k *compile probes*, Classic build, floppy package, fork/CD reproducibility and artifacts | A 68k core compile is not a 68k app. The checks workflow image is referenced by tag, unlike the release workflow's digest. Artifact inspection is not native OS execution |
| `.github/workflows/release.yml` | Classic release/previews using pinned toolchain images; creates artifacts, does not publish | No Linux archive or pkgsrc release qualification; publication remains owner-controlled |
| `.github/workflows/pages.yml` | Website deployment on `main` site changes | Deployment automation is a different workflow, not a test matrix or permission to deploy |
| Unix release drafts [#211](https://github.com/jackwillis/poodle-earth/pull/211), [#215](https://github.com/jackwillis/poodle-earth/pull/215), [#216](https://github.com/jackwillis/poodle-earth/pull/216) | Program name, Linux archive and pkgsrc recipe proposals | Packaging archive and pkgsrc install/build/launch checks are **not** currently exercised by configured CI |

**Immediate gap:** `checks.yml` path filters include `tools/**` but not `packaging/**`; neither that workflow nor `release.yml` is guaranteed to run for a packaging-only change. Docs-only changes can omit the Checks workflow entirely. Therefore, using a path-filtered heavy job as a required status check can create a missing-check/pending-check hazard. Confirm actual branch-protection configuration before changing any required status context. A small, **always-present PR verdict/metadata check** and selectively gated expensive jobs are a candidate solution, not implemented here.

The checks are configured on PRs and selected pushes, not a periodic nightly schedule. No NetBSD native CI job is present in the inspected workflows. The current local NetBSD 11.1/amd64 KVM fixture is a separately maintained *in-progress* experimental environment, **not** a demonstrated green hosted CI job.

## 2. Verification tiers and proposed cadence

| Tier | Trigger / scope | Candidate evidence | Blocking posture |
| --- | --- | --- | --- |
| **T1 — PR core** | Every relevant change, with a stable PR status present even for docs-only changes | Existing GCC+sanitizers+CTest, Clang+CTest, Retro68 PowerPC build/probe, 68k *compile-only*, repository policy and appropriate packaging/unit checks | Required only after reliable jobs and exact GitHub status-context rules are owner-reviewed; keep expensive checks path-targeted |
| **T2 — expanded** | Manual/targeted, or *optional owner-approved* scheduled checks once stable | GCC 12 floor [#102](https://github.com/jackwillis/poodle-earth/issues/102), Linux ABI and distro matrix, NetBSD/amd64 pkgsrc VM, macppc cross-compile/ELF audit, isolated big-endian portable test [#164](https://github.com/jackwillis/poodle-earth/issues/164), experimental frontend work | Initially advisory. Promote narrow stable checks with measured signal/cost and an explicitly assigned maintainer; no nightly schedule is enabled by this document |
| **T3 — release candidate** | Exact candidate source revision and archive/package per advertised target | Independent install, launch, input, data, readback, clean exit, remove; inspected ABI/runtime floor, checksums, source/licensing, and reproducibility where demonstrated | Required *as applicable* by existing release checklist and owner-approved platform scope; don't invent a new release gate |
| **T4 — native acceptance** | Explicitly owner-authorized run on real target/hardware | NetBSD 11.1/macppc G4 execution and package behavior, bare Classic Mac OS interaction, memory, swap, visual/presentation evidence | No unattended G4 CI; publish only the claims actually validated and record exceptions |

**Tests are not interchangeable:** host sanitizer success says nothing about X11 scanout, a pkgsrc PLIST pass says nothing about application launch, a PoodleScripts assertion says nothing about physical pixels, and a Retro68 68k compile probe says nothing about a 68k application existing.

## 3. Sparse candidate coverage matrix

Rows are *target/evidence slices*, not a promise of independent new jobs on every commit.

| Candidate slice | T1 PR | T2 targeted | T3 candidate | T4 native |
| --- | --- | --- | --- | --- |
| Linux host GCC sanitizer/CTest + Clang/CTest | Existing jobs | Only for regression triage | Re-run relevant tests | — |
| Classic PPC Retro68 compile / archive / hybrid-CD inspection | Existing job | Emulator smoke if appropriate | Exact release fork/resource/source checks | Owner-approved Classic PPC run |
| 68k compile probe | Existing probe | Older compiler/ISA experiments | No runtime claim | No 68k acceptance implied |
| Linux x86-64 glibc archive | Packaging PR after trigger fix | Old-ABI distro/negative control/Xvfb/real-desktop candidates | Actual source/ABI/artifact install/run/remove | Real desktop observation if needed |
| NetBSD 11.1/amd64 pkgsrc | Initially no PR job | Local pinned KVM fixture; candidate ephemeral VM after independent qualification | Native pkgsrc build/PLIST, `pkg_add`, X11 run, `pkg_delete` | VM is native-ISA guest evidence, not PowerPC |
| NetBSD 11.1/macppc | Future pinned cross-compile/ABI audit | Big-endian/32-bit core checks | Target-specific artifact/package verification | Owner-controlled G4 application and package acceptance |
| FLTK, Win32, Cocoa, Java, other research | No required PR additions until implemented and stable | Hypothesis/feasibility work only | Only if actually shipped | Independent target-specific proof |

For the macppc path, the existing GCC 12.5 cross toolchain currently uses a NetBSD **11.0** sysroot; do not silently call that a NetBSD 11.1 build or use it to claim that release's ABI. A native NetBSD/macppc test is feasible on the owner's running G4, but **never compile there**, and do not access/modify it without separately granted authorization. A QEMU/macppc guest is currently deferred; it is not a prerequisite for doing bounded G4 acceptance.

The NetBSD/amd64 VM is valuable for inexpensive package verification, but a demonstrated amd64 package does not validate 32-bit PowerPC ABI, byte order, r128 display or hardware responsiveness.

## 4. Evidence contract: record claims rather than a synthetic “platform PASS”

Every meaningful CI or native record should identify:

- **Subject:** source commit, artifact/package SHA-256, edition, package/distribution revision, source-data/pack version, intended architecture and frontend.
- **Execution:** runner or hardware, OS and version, compiler/toolchain revision, sysroot, dependency/runtime versions, explicit command(s), test suite or scripted scenario identifier.
- **Observation:** result and exit status, structured logs, semantic outcome, frame/readback counts when available, package file list, relevant metrics with units, limits and reproduction notes.
- **Limits:** unrun tests, fallback/waivers, known noncomparable environments, manual visual versus instrumented evidence, and owner sign-off where required.

Maintain **independent claim states**: `builds`, `tests`, `runs`, `package verified`, `presentation observed`, `hardware/resource accepted`, `release accepted`. These are not an automatic monotonic ladder; a working binary can precede packaging and a package can install but not render. Tie each state to the exact artifact/environment; changing the commit or candidate invalidates uncontrolled carry-over.

For scripted rendering: `verified_frames == presentations` verifies readback completeness, **not semantic geographic correctness or native visual presentation**. Do not require a hard-coded frame count of 113 across environments; use the trace oracle implemented by the test contract. Distinguish deterministic semantic assertions, pixel validation, native screenshot/observation, and timing evidence.

## 5. Path selection, cost and reliability

1. **Always present PR decision:** propose one lightweight status context that is created for all PRs, including documentation-only changes, and accurately says whether targeted checks ran, skipped or failed. Avoid false “green” from skipped workflows. Confirm compatibility with existing branch protection before adoption.
2. **Changed-file coverage:** explicitly include `packaging/**`, `.github/workflows/**`, release manifest/data generation scripts, source, tests and toolchain input changes in the relevant validation plan. A package-only PR must never silently skip the *package* check after implementation; a docs-only PR should not need to rebuild Retro68.
3. **Approximate budget:** aim for roughly four or five stable required PR checks (including an always-present verdict), measured by runtime, failure yield, owner attention and retry cost; this is a planning target, **not** an adopted quota or requirement to add jobs immediately.
4. **Promotion:** prove a check in the relevant environment, document a reproducible local command and a clean signal on representative PRs, pin inputs, measure costs, decide who maintains it, then request owner approval for new required status/schedule. A failing experimental job remains visible but nonblocking until that decision.
5. **Demotion/quarantine:** when a check is flaky, isolate and log the defect; retain coverage through other trustworthy evidence. Don't silently turn a broken required gate into a permanent pass, lower thresholds after observing failure, or enable unreviewed retries to mask issues.
6. **Reusability:** write scripts so the same test commands run from a developer shell, the persistent local KVM fixture and potential GitHub guest, without GitHub-specific logic as the source of truth. Do not commit images, credentials, large binaries or local paths. Pin containers, guest images, packages and third-party Actions at appropriate immutable versions/hashes after qualification.

Third-party hosted NetBSD VM Actions are **candidates**, not preapproved security dependencies. Evaluate licenses, execution privilege, supply chain, NetBSD 11.1 guest availability, runtime costs and pinning before adoption. Never use hosted guest success as proof of G4 acceptance.

## 6. Horizontal slices mapped onto product releases

These are **possible integration windows**, not amendments to release commitments or assignment of issue milestones.

| Window | Narrow CI/qualification slice | Dependency and exit criterion | Separation |
| --- | --- | --- | --- |
| **v0.1.3 “Unix distribution”** | Fix packaging path-trigger risk and add/verify Linux archive + NetBSD/amd64 pkgsrc acceptance commands. Use exact source and packages; consider macppc cross-build and owner-authorized native G4 acceptance as separately evidenced | Existing [#211](https://github.com/jackwillis/poodle-earth/pull/211) → [#215](https://github.com/jackwillis/poodle-earth/pull/215)/[#216](https://github.com/jackwillis/poodle-earth/pull/216), [#212](https://github.com/jackwillis/poodle-earth/issues/212). A package must really install/run/remove to make a target claim; **permanent hosted NetBSD CI is not a v0.1.3 blocker** | Do not pull FLTK, AppImage, extra Linux distros or QEMU/macppc into the release merely to fill matrix cells |
| **v0.1.4 “PoodleScripts”** | Test the bounded script parser, cancellation, state transitions, report semantics and packaged acceptance/tour examples with the existing runner [#232](https://github.com/jackwillis/poodle-earth/issues/232) | Actual implemented grammar and owner-approved native Classic observation; avoid invented script capabilities | Unix script parity is not implied by a Classic script release |
| **Subsequent v0.1.x patches** | With each release, extend automated feature regression coverage for actual changed behavior; choose language additions only where they enable useful tests/workflows | A versioned semantic scenario and host evidence when possible, plus native observation for visual behavior; candidate cursor policy [#242](https://github.com/jackwillis/poodle-earth/issues/242) | Do not turn every feature into a new syntax keyword or demand every scenario run on every frontend |
| **v0.2.0 atlas integration** | Cross-frontend semantic journeys for the agreed atlas feature set, platform presentation exceptions and G4 performance/resource acceptance | Existing owner-approved minor-release parity and hardware signoff policy; recorded waivers where appropriate | T2 research does not become a new v0.2 blocker |
| **v0.2.x and later** | Optional FLTK, Win32, Tiger, big-endian emulation and wider platform CI **after** prototype qualification | Promote one platform/check at a time, with maintenance owner and measurable payoff | Do not build an exhaustive platform x compiler x toolkit matrix |

**GCC 12 floor and big-endian checks** already have implementation/discovery ownership in [#102](https://github.com/jackwillis/poodle-earth/issues/102) and [#164](https://github.com/jackwillis/poodle-earth/issues/164). The wider workstream sequencing is in [#247](https://github.com/jackwillis/poodle-earth/issues/247). This proposal must not duplicate or take ownership of those tickets.

## 7. First recommended implementation assignment (requires separate owner authorization)

**One bounded slice: correct CI trigger/required-status coverage for packaging-only and docs-only PRs.** First inspect the actual required-status/ruleset configuration and examples of current docs-only and package-only PR results. Propose a stable lightweight PR status and focused packaging/test routing; update only the smallest workflow files needed in an isolated implementation PR. Verify, with reproducible evidence, a docs-only PR, a packaging-only PR and a C++/data PR all produce truthful statuses and run the intended tests. Preserve existing host and Retro68 checks and PR cost bounds. No changes to platform release promises, no branch-protection update without owner approval, no automatic nightly runs.

**Stop/rollback criteria:** unexpected required-check deadlock, false-green skipped jobs, inappropriate expensive docs-only runs, runtime/cost regression, third-party input/credential risk, or changed published artifact behavior. Report the issue and revert only within an authorized branch; do not modify another agent's work.

### Adoption and revision procedure

This file is **PE-CI-01 v0.1, a versioned proposal**. Future revisions should state changed assumptions, reviewed evidence, resulting decision and owner approval. Only an owner-approved decision can convert parts into required CI policy or an explicit release gate. Any implementation should live in independently reviewable tickets/PRs with exact environment and results. Never use a docs PR itself as authorization to enable scheduled automation, invoke the G4, modify protected branch rules, or publish.
