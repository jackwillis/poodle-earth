#!/usr/bin/env python3
"""Poodle Earth portfolio research v0.1; standard library only, NOT release policy."""
from __future__ import annotations

import argparse
import csv
import itertools
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SNAPSHOT = ROOT / "docs/research/platform-rice-scenario-inputs-2026-11.csv"
HERE = Path(__file__).resolve().parent


def load_scenarios(path=SNAPSHOT):
    with Path(path).open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    required = {"OS / family", "Version / scenario", "ISA / ABI",
                "Reach / quarter (hypothetical)", "Impact (hypothetical)",
                "Confidence (hypothetical)", "Effort person-months (hypothetical)"}
    if not rows or not required.issubset(rows[0]):
        raise ValueError("Missing or incompatible archived platform scenario snapshot")
    keys = [(r["OS / family"], r["Version / scenario"], r["ISA / ABI"]) for r in rows]
    if len(keys) != len(set(keys)):
        raise ValueError("Duplicate scenario identity")
    for r in rows:
        for name in ("Reach / quarter (hypothetical)", "Impact (hypothetical)",
                     "Confidence (hypothetical)", "Effort person-months (hypothetical)"):
            if float(r[name]) < 0:
                raise ValueError("Negative archived hypothesis")
    return rows


def load_catalog(path=HERE / "assumptions.json"):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    investments = data["investments"]
    if len(investments) != len(set(investments)):
        raise ValueError("Duplicate investment")
    for name, node in investments.items():
        if node["evidence"] not in ("observed", "draft", "upstream", "hypothesis", "blocked"):
            raise ValueError("Unknown evidence type for " + name)
        for dep in node.get("requires_all", []):
            if dep not in investments or dep == name:
                raise ValueError("Invalid AND prerequisite " + name)
        for alternatives in node.get("requires_any", []):
            if not alternatives or any(d not in investments or d == name for d in alternatives):
                raise ValueError("Invalid OR prerequisite " + name)
        if any(float(node.get(k, 0)) < 0 for k in ("owner_hours", "validation_hours", "maintenance_hours")):
            raise ValueError("Negative resource cost")
    def visit(name, path):
        if name in path:
            raise ValueError("Cycle in investment dependencies")
        node = investments[name]
        for d in node.get("requires_all", []):
            visit(d, path | {name})
        for ds in node.get("requires_any", []):
            for d in ds:
                visit(d, path | {name})
    for name in investments:
        visit(name, set())
    return data


def valid_bundle(names, investments):
    s = set(names)
    return all(set(investments[n].get("requires_all", [])).issubset(s) and
               all(set(options) & s for options in investments[n].get("requires_any", []))
               for n in s)


def match_row(row, spec):
    return all(row[field] == value for field, value in spec["selector"].items())


def evaluate(names, investments, targets, scenarios):
    selected = set(names)
    owner = sum(float(investments[n].get("owner_hours", 0)) for n in selected)
    validation = sum(float(investments[n].get("validation_hours", 0)) for n in selected)
    maintenance = sum(float(investments[n].get("maintenance_hours", 0)) for n in selected)
    learning = sum(float(investments[n].get("learning_proxy", 0)) for n in selected)
    activated = []
    # Only ONE scenario per stated audience bucket. These rough buckets are
    # demonstrative, NOT independently verified audience overlaps.
    buckets = {}
    for t in targets:
        if not set(t["requires_all"]).issubset(selected):
            continue
        matches = [r for r in scenarios if match_row(r, t)]
        if len(matches) != 1:
            raise ValueError("Target selector must match exactly one archived row: " + t["id"])
        r = matches[0]
        # Illustrative proxy; not observed adoption, causal value, or support.
        v = float(r["Reach / quarter (hypothetical)"]) * float(r["Impact (hypothetical)"]) * float(r["Confidence (hypothetical)"])
        # Alternative implementations for one target/audience should not double count.
        bucket = t["audience_bucket"]
        if v > buckets.get(bucket, 0):
            buckets[bucket] = v
        activated.append(t["id"])
    return {"selected": sorted(selected), "targets": sorted(activated),
            "reach_impact_proxy": round(sum(buckets.values()), 4),
            "learning_proxy": round(learning, 4),
            "owner_hours": round(owner, 4),
            "validation_hours": round(validation, 4),
            "maintenance_hours": round(maintenance, 4)}


def dominates(a, b):
    # Feasible budget already checked. Maintenance is a persistent objective.
    maxim = ("reach_impact_proxy", "learning_proxy")
    minim = ("maintenance_hours",)
    no_worse = all(a[k] >= b[k] for k in maxim) and all(a[k] <= b[k] for k in minim)
    strict = any(a[k] > b[k] for k in maxim) or any(a[k] < b[k] for k in minim)
    return no_worse and strict


def solve(catalog, scenarios, owner_budget, validation_budget):
    investments, targets = catalog["investments"], catalog["targets"]
    ids = sorted(investments)
    if len(ids) > 20:
        raise ValueError("v0.1 exhaustive enumeration limited to 20 investments")
    portfolios = []
    for bits in itertools.product((False, True), repeat=len(ids)):
        names = [k for k, flag in zip(ids, bits) if flag]
        if not valid_bundle(names, investments):
            continue
        result = evaluate(names, investments, targets, scenarios)
        if result["owner_hours"] > owner_budget or result["validation_hours"] > validation_budget:
            continue
        portfolios.append(result)
    return [p for p in portfolios if not any(dominates(q, p) for q in portfolios if q is not p)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--owner-budget", type=float, default=30)
    parser.add_argument("--validation-budget", type=float, default=20)
    parser.add_argument("--sensitivity", action="store_true", help="Print other budget settings")
    args = parser.parse_args()
    scenarios = load_scenarios()
    catalog = load_catalog()
    if args.owner_budget < 0 or args.validation_budget < 0:
        parser.error("Budgets must be nonnegative")
    budgets = [(args.owner_budget, args.validation_budget)]
    if args.sensitivity:
        budgets.extend([(args.owner_budget * .5, args.validation_budget),
                        (args.owner_budget, args.validation_budget * .5),
                        (args.owner_budget * 1.5, args.validation_budget * 1.5)])
    results = []
    for owner, validation in budgets:
        frontier = solve(catalog, scenarios, owner, validation)
        results.append({"budgets": {"owner_hours": owner, "validation_hours": validation},
                        "frontier": sorted(frontier, key=lambda x: (-x["reach_impact_proxy"], -x["learning_proxy"], x["maintenance_hours"]))})
    print(json.dumps({"archived_scenario_count": len(scenarios), "model_kind": "illustrative research",
                      "qualification": "No platform support, demand or experimental success inferred",
                      "runs": results}, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
