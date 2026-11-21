"""Research-model invariants; run: python3 -m unittest discover -s research/platform-strategy -p 'test_*.py'."""
import importlib.util
import json
from pathlib import Path
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("portfolio", HERE / "portfolio.py")
portfolio = importlib.util.module_from_spec(spec)
spec.loader.exec_module(portfolio)


class PortfolioTests(unittest.TestCase):
    def setUp(self):
        self.rows = portfolio.load_scenarios()
        self.catalog = portfolio.load_catalog()

    def test_archived_scenario_count(self):
        self.assertEqual(len(self.rows), 71)
        self.assertEqual(len(self.catalog["targets"]), 6)

    def test_dependency_enforced(self):
        inv = self.catalog["investments"]
        self.assertFalse(portfolio.valid_bundle(["java14_reader"], inv))
        self.assertTrue(portfolio.valid_bundle(["java14_static_gate", "java14_awt_pilot", "java14_reader"], inv))
        self.assertFalse(portfolio.valid_bundle(["fltk_shared_frontend"], inv))

    def test_alternative_frontends_not_additive(self):
        inv = self.catalog["investments"]
        both = ["xlib_linux_package", "fltk_linux_probe", "fltk_shared_frontend"]
        fltk = ["fltk_linux_probe", "fltk_shared_frontend"]
        a = portfolio.evaluate(both, inv, self.catalog["targets"], self.rows)
        b = portfolio.evaluate(fltk, inv, self.catalog["targets"], self.rows)
        self.assertEqual(a["reach_impact_proxy"], b["reach_impact_proxy"])
        self.assertEqual(len(a["targets"]), 2)

    def test_dependencies_and_budgets_respected(self):
        frontier = portfolio.solve(self.catalog, self.rows, 30, 20)
        self.assertTrue(frontier)
        for p in frontier:
            self.assertTrue(portfolio.valid_bundle(p["selected"], self.catalog["investments"]))
            self.assertLessEqual(p["owner_hours"], 30)
            self.assertLessEqual(p["validation_hours"], 20)
        for a in frontier:
            self.assertFalse(any(portfolio.dominates(b, a) for b in frontier if b is not a))

    def test_negative_budget_and_cycles_fail(self):
        spec = {"investments": {
            "a": {"evidence": "hypothesis", "requires_all": ["b"]},
            "b": {"evidence": "hypothesis", "requires_all": ["a"]}
        }, "targets": []}
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as temp:
            path = Path(temp) / "catalog.json"
            path.write_text(json.dumps(spec))
            with self.assertRaises(ValueError):
                portfolio.load_catalog(path)

    def test_target_selector_is_exact(self):
        bad = [{"id": "bad", "selector": {"OS / family": "Linux"},
                "requires_all": [], "audience_bucket": "linux"}]
        with self.assertRaises(ValueError):
            portfolio.evaluate([], self.catalog["investments"], bad, self.rows)

    def test_pareto_objectives(self):
        a = {"reach_impact_proxy": 10, "learning_proxy": 2, "maintenance_hours": 2}
        b = {"reach_impact_proxy": 9, "learning_proxy": 2, "maintenance_hours": 3}
        c = {"reach_impact_proxy": 11, "learning_proxy": 1, "maintenance_hours": 2}
        self.assertTrue(portfolio.dominates(a, b))
        self.assertFalse(portfolio.dominates(a, c))


if __name__ == "__main__":
    unittest.main()
