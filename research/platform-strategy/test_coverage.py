"""Coverage model invariants: python3 -m unittest discover -s research/platform-strategy -p 'test_*.py' -v"""
import importlib.util
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("coverage", HERE / "coverage.py")
coverage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(coverage)


class CoverageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = coverage.load_rows()
        cls.records = coverage.load_ledger()
        cls.registry = coverage.build_registry(cls.rows, cls.records)

    def find(self, family, version, isa):
        return next(r for r in self.rows if (r["OS / family"], r["Version / scenario"],
                                            r["ISA / ABI"]) == (family, version, isa))

    def test_exact_71_unique_stable_ids(self):
        self.assertEqual(len(self.registry), 71)
        self.assertEqual(len({r["scenario_id"] for r in self.registry}), 71)
        self.assertEqual(coverage.key_for(self.find("Classic Mac", "System 7.1.2–9", "PPC")),
                         "6807e01f27808e53")

    def test_every_scenario_has_explicit_routes(self):
        for r in self.registry:
            self.assertTrue(r["routes"], r["scenario_id"])
            self.assertTrue(r["no_assumed_support"])
            self.assertTrue(all(p["route"] in coverage.ROUTES for p in r["routes"]))

    def test_java_not_everywhere(self):
        tiger = self.find("Mac OS X", "10.4/10.5", "PowerPC")
        self.assertIn("java14-reader", coverage.candidate_routes(tiger))
        old_classic = self.find("Classic Mac", "System 7 / 8 / 9", "68k")
        self.assertEqual(coverage.candidate_routes(old_classic), ["vector-68k"])
        linux = self.find("Linux", "modern distributions", "x86_64")
        self.assertEqual(coverage.candidate_routes(linux), ["fltk-unix", "xlib-unix"])

    def test_partial_evidence_does_not_promote_full_scenario(self):
        classic = next(x for x in self.registry if x["family"] == "Classic Mac"
                       and x["isa"] == "PPC")
        self.assertEqual(classic["routes"][0]["claim_level"], "unknown")
        self.assertEqual(classic["routes"][0]["partial_records"], 1)
        linux = next(x for x in self.registry if x["family"] == "Linux"
                     and x["version"] == "modern distributions" and x["isa"] == "x86_64")
        self.assertTrue(all(p["claim_level"] == "unknown" for p in linux["routes"]))
        self.assertEqual(coverage.summarize(self.registry)["without_full_scenario_evidence"], 71)

    def test_evidence_cannot_use_unrelated_route(self):
        wrong = dict(self.records[0], route="java14-reader")
        with self.assertRaises(ValueError):
            coverage.validate_ledger([wrong], self.rows)

    def test_no_unproven_acceptance(self):
        fake = dict(self.records[0], level="published", native_validation=None)
        with self.assertRaises(ValueError):
            coverage.validate_ledger([fake], self.rows)

    def test_unknown_provenance_rejected(self):
        fake = dict(self.records[0], scenario_id="not-in-snapshot")
        with self.assertRaises(ValueError):
            coverage.validate_ledger([fake], self.rows)

    def test_requires_full_scenario_boolean(self):
        fake = dict(self.records[0])
        fake.pop("full_scenario")
        with self.assertRaises(ValueError):
            coverage.validate_ledger([fake], self.rows)

    def test_full_scenario_cannot_skip_stages(self):
        fake = dict(self.records[0], full_scenario=True)
        with self.assertRaises(ValueError):
            coverage.validate_ledger([fake], self.rows)
        fake["verified_stages"] = {
            stage: "https://github.com/jackwillis/poodle-earth/issues/95"
            for stage in coverage.LEVELS[1:coverage.LEVELS.index("published")]
        }
        with self.assertRaises(ValueError):
            coverage.validate_ledger([fake], self.rows)  # publication_url still missing
        fake["publication_url"] = "https://github.com/jackwillis/poodle-earth/releases/tag/v0.1.2"
        coverage.validate_ledger([fake], self.rows)
        # This proves schema acceptance ONLY, not whether that evidence is truthful.

    def test_ledger_schema(self):
        with TemporaryDirectory() as d:
            path = Path(d) / "bad.json"
            path.write_text(json.dumps({"schema": 2, "records": []}), encoding="utf-8")
            with self.assertRaises(ValueError):
                coverage.load_ledger(path)


if __name__ == "__main__":
    unittest.main()
