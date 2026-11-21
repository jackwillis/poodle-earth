#!/usr/bin/env python3
"""Poodle Earth v0.2 platform coverage/evidence registry; no support certifications."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SNAPSHOT = ROOT / "docs/research/platform-rice-scenario-inputs-2026-11.csv"
LEDGER = Path(__file__).with_name("evidence.json")
LEVELS = ("unknown", "source_compiled", "binary_built", "native_launched",
          "native_accepted", "package_accepted", "published")
# Route hints are *testable hypotheses*, not eligibility or compatibility proof.
ROUTES = {
    "classic-toolbox": "Existing PPC Classic implementation; actual OS/version acceptance required",
    "vector-68k": "Separate reduced vector renderer; no 68k executable established",
    "fltk-unix": "FLTK toolkit candidate; requires target runtime, ABI and GUI validation",
    "xlib-unix": "Minimal X11 candidate; requires X server and native validation",
    "fltk-windows": "Windows FLTK backend candidate; no target compatibility claim",
    "win32-gdi": "Native Win32/GDI research alternative; historical CRT/API unresolved",
    "cocoa-native": "Native Cocoa candidate; SDK/toolchain/OS evidence needed",
    "java14-reader": "Independent Java 1.4 AWT Reader; only compatibility protocol proposed",
    "wasm-reader": "Emscripten/browser CPU Reader research path",
    "platform-native": "Unknown platform-specific port; feasibility unassessed",
}
KEYS = ("OS / family", "Version / scenario", "ISA / ABI")


def load_rows(path=SNAPSHOT):
    with Path(path).open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    if len(rows) != 71:
        raise ValueError("Expected 71 archived scenario rows; investigate source drift")
    if len({tuple(r[k] for k in KEYS) for r in rows}) != len(rows):
        raise ValueError("Duplicated OS/version/ISA scenario")
    for r in rows:
        for key in KEYS:
            if not r.get(key):
                raise ValueError("Missing scenario identity")
    return rows


def key_for(row):
    # Content-derived and independent of workbook rank/order; no guessed slug collisions.
    return hashlib.sha256(json.dumps([row[k] for k in KEYS],
                                     ensure_ascii=False).encode("utf-8")).hexdigest()[:16]


def candidate_routes(row):
    family, ver, isa = (row[k] for k in KEYS)
    if family == "Classic Mac":
        return ["vector-68k"] if isa == "68k" else ["classic-toolbox"]
    if family == "Mac OS X":
        if "10.4/10.5" == ver:
            return ["cocoa-native", "java14-reader"]
        return ["cocoa-native"]
    if family == "macOS":
        return ["cocoa-native", "fltk-unix"]
    if family == "Windows":
        routes = ["win32-gdi"]
        if ver == "2000 / XP":
            routes.append("java14-reader")
        elif ver == "10/11":
            routes.append("fltk-windows")
        return routes
    if family == "Solaris":
        return ["xlib-unix", "java14-reader"]
    if family == "WebAssembly":
        return ["wasm-reader"]
    if family == "Linux" and ("legacy" in ver or "i586" in isa):
        return ["xlib-unix"]
    unix_families = ("Linux", "Linux / musl", "NetBSD", "FreeBSD", "OpenBSD",
                     "illumos / Tribblix", "illumos / OpenIndiana",
                     "illumos / OmniOS", "DragonFly BSD")
    if family in unix_families:
        return ["fltk-unix", "xlib-unix"]
    return ["platform-native"]


def load_ledger(path=LEDGER):
    obj = json.loads(Path(path).read_text(encoding="utf-8"))
    if obj.get("schema") != 1 or not isinstance(obj.get("records"), list):
        raise ValueError("Unexpected evidence ledger schema")
    return obj["records"]


def validate_ledger(records, rows):
    index = {key_for(row): row for row in rows}
    for rec in records:
        if rec.get("scenario_id") not in index:
            raise ValueError("Unrecognized evidence scenario_id")
        if rec.get("route") not in ROUTES:
            raise ValueError("Unrecognized evidence route")
        if rec["route"] not in candidate_routes(index[rec["scenario_id"]]):
            raise ValueError("Evidence route not mapped to scenario")
        if rec.get("level") not in LEVELS[1:]:
            raise ValueError("Unknown or unsupported evidence level")
        for required in ("source", "date", "environment", "scope", "revision"):
            if not isinstance(rec.get(required), str) or not rec[required].strip():
                raise ValueError("Incomplete evidence provenance: " + required)
        if not isinstance(rec.get("full_scenario"), bool):
            raise ValueError("Evidence needs explicit full_scenario boolean")
        if rec["level"] in LEVELS[4:] and not rec.get("native_validation"):
            raise ValueError("Acceptance/published requires native_validation evidence")
    return index


def build_registry(rows, records):
    validate_ledger(records, rows)
    result = []
    by_id = {}
    for rec in records:
        by_id.setdefault((rec["scenario_id"], rec["route"]), []).append(rec)
    for row in rows:
        sid = key_for(row)
        routes = []
        for route in candidate_routes(row):
            evidence = by_id.get((sid, route), [])
            full = [r for r in evidence if r["full_scenario"]]
            level = max((r["level"] for r in full), key=LEVELS.index, default="unknown")
            routes.append({"route": route, "claim_level": level,
                           "full_scenario_records": len(full),
                           "partial_records": len(evidence) - len(full),
                           "hypothesis": ROUTES[route]})
        result.append({
            "scenario_id": sid,
            "family": row[KEYS[0]], "version": row[KEYS[1]], "isa": row[KEYS[2]],
            "historical_rice": float(row["Digital RICE (workbook one-decimal value)"]),
            "historical_reach_prior": float(row["Reach / quarter (hypothetical)"]),
            "routes": routes,
            "no_assumed_support": True,
        })
    return result


def summarize(registry):
    assert len(registry) == 71
    levels = Counter(max((r["claim_level"] for r in item["routes"]),
                         key=LEVELS.index) for item in registry)
    paths = Counter(r["route"] for item in registry for r in item["routes"])
    missing = [r for r in registry if all(p["claim_level"] == "unknown" for p in r["routes"])]
    return {"scenarios": len(registry), "distinct_scenarios": len({r["scenario_id"] for r in registry}),
            "routes_by_hypothesis": dict(sorted(paths.items())),
            "best_evidence_by_scenario": dict(sorted(levels.items())),
            "without_full_scenario_evidence": len(missing),
            "disclaimer": "Candidates are NOT verified runtime support. RICE priors are unmeasured."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="Include all 71 scenario records")
    parser.add_argument("--evidence", type=Path, default=LEDGER,
                        help="Explicit provenance ledger (default: evidence.json)")
    args = parser.parse_args()
    rows = load_rows()
    registry = build_registry(rows, load_ledger(args.evidence))
    output = {"summary": summarize(registry)}
    if args.json:
        output["scenarios"] = registry
    print(json.dumps(output, indent=2, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
