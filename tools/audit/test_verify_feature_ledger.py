#!/usr/bin/env python3
"""Focused negative tests for the V1 feature-ledger verifier.

Each test mutates an in-memory copy of the real ledger (or production-debt
baseline), writes it to a temporary file, and asserts that
``verify_feature_ledger.validate`` reports the expected failure mode. The
remaining inputs (editor sources, asset registry, CMake engine globs) point
at the real repository so the checks exercise production data.

Run directly (``python tools/audit/test_verify_feature_ledger.py``) or via
CTest (label ``audit``); no third-party dependencies are required.
"""

from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import verify_feature_ledger as vfl  # noqa: E402


ROOT = vfl.ROOT


def load_ledger() -> dict:
    return json.loads((ROOT / vfl.LEDGER_REL).read_text(encoding="utf-8"))


def load_debt() -> dict:
    return json.loads((ROOT / vfl.DEBT_REL).read_text(encoding="utf-8"))


def find_feature(ledger: dict, feature_id: str) -> dict:
    for feature in ledger["features"]:
        if feature["id"] == feature_id:
            return feature
    raise KeyError(feature_id)


class VerifierFailureModes(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = Path(self._tmp.name)

    def _layout(self, ledger: dict | None = None, debt: dict | None = None) -> vfl.Layout:
        layout = vfl.Layout.from_root(ROOT)
        if ledger is not None:
            path = self.tmp / "feature_ledger.json"
            path.write_text(json.dumps(ledger), encoding="utf-8")
            layout.ledger_path = path
        if debt is not None:
            path = self.tmp / "editor_production_debt.json"
            path.write_text(json.dumps(debt), encoding="utf-8")
            layout.debt_path = path
        return layout

    def assert_error_contains(self, errors: list[str], needle: str) -> None:
        self.assertTrue(
            any(needle in error for error in errors),
            msg=f"expected an error containing {needle!r}; got: {errors}",
        )

    # -- baseline ---------------------------------------------------------
    def test_baseline_is_clean(self) -> None:
        self.assertEqual(vfl.validate(vfl.Layout.from_root(ROOT)), [])

    def test_action_index_is_stable_and_owned(self) -> None:
        index = vfl.build_action_index(vfl.Layout.from_root(ROOT))
        self.assertGreater(len(index), 0)
        self.assertEqual(len(index), len(set(index)))
        self.assertTrue(all(isinstance(owner, str) and owner for owner in index.values()))

    # -- schema / structure ----------------------------------------------
    def test_wrong_schema_version(self) -> None:
        ledger = load_ledger()
        ledger["schemaVersion"] = 2
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)), "schemaVersion must be 1"
        )

    def test_duplicate_feature_id(self) -> None:
        ledger = load_ledger()
        ledger["features"].append(copy.deepcopy(ledger["features"][0]))
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)), "duplicate feature id"
        )

    def test_missing_acceptance_key(self) -> None:
        ledger = load_ledger()
        find_feature(ledger, "release-contract")["acceptance"].pop("productionReady")
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)), "missing acceptance keys"
        )

    def test_complete_feature_with_failed_acceptance(self) -> None:
        ledger = load_ledger()
        feature = find_feature(ledger, "release-foundation")
        feature["status"] = "complete"
        feature["blockers"] = []
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)), "has failed acceptance"
        )

    def test_complete_feature_with_blockers(self) -> None:
        ledger = load_ledger()
        feature = find_feature(ledger, "visual-script-retirement")
        feature["status"] = "complete"
        feature["blockers"] = ["still not done"]
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)), "cannot have blockers"
        )

    # -- panel ownership --------------------------------------------------
    def test_registered_panel_not_owned(self) -> None:
        ledger = load_ledger()
        find_feature(ledger, "blueprint")["editor"]["panelIds"].remove("blueprint")
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)),
            "registered editor panel is not owned by the ledger: blueprint",
        )

    def test_stale_ledger_panel(self) -> None:
        ledger = load_ledger()
        find_feature(ledger, "blueprint")["editor"]["panelIds"].append("no_such_panel")
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)),
            "stale ledger panel id is not registered by the editor: no_such_panel",
        )

    # -- action-source ownership -----------------------------------------
    def test_interactive_source_not_owned(self) -> None:
        ledger = load_ledger()
        find_feature(ledger, "git-integration")["editor"]["actionSources"].remove(
            "apps/editor_cpp/src/editor_version_control.cpp"
        )
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)),
            "interactive editor source is not owned by the ledger",
        )

    def test_stale_action_source_without_controls(self) -> None:
        ledger = load_ledger()
        # editor_asset_db_core.cpp exists but declares no interactive controls.
        find_feature(ledger, "git-integration")["editor"]["actionSources"].append(
            "apps/editor_cpp/src/editor_asset_db_core.cpp"
        )
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)),
            "stale ledger action source has no interactive controls",
        )

    def test_action_source_missing_path(self) -> None:
        ledger = load_ledger()
        find_feature(ledger, "git-integration")["editor"]["actionSources"].append(
            "apps/editor_cpp/src/does_not_exist.cpp"
        )
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)), "referenced path does not exist"
        )

    # -- asset-format registry -------------------------------------------
    def test_malformed_asset_format(self) -> None:
        ledger = load_ledger()
        find_feature(ledger, "blueprint")["assetFormats"].append("NotAnExtension")
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)), "malformed asset extension"
        )

    def test_duplicate_asset_format(self) -> None:
        ledger = load_ledger()
        find_feature(ledger, "blueprint")["assetFormats"].append(".dbp")
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)), "duplicate asset extension"
        )

    # -- runtime-consumer registration -----------------------------------
    def test_runtime_consumer_not_registered(self) -> None:
        ledger = load_ledger()
        # Real file, but outside every compiled engine source root.
        find_feature(ledger, "git-integration")["runtimeConsumers"].append(
            "tools/audit/verify_feature_ledger.py"
        )
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)),
            "not registered in dse_engine",
        )

    # -- production-debt reconciliation ----------------------------------
    def test_production_debt_not_assigned(self) -> None:
        ledger = load_ledger()
        find_feature(ledger, "blueprint")["productionDebtIds"].remove(
            "EDITOR-BLUEPRINT-SIMULATED-DEBUG"
        )
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)),
            "production debt is not assigned to a ledger feature",
        )

    def test_stale_debt_id(self) -> None:
        ledger = load_ledger()
        find_feature(ledger, "blueprint")["productionDebtIds"].append("NO-SUCH-DEBT")
        self.assert_error_contains(
            vfl.validate(self._layout(ledger=ledger)),
            "stale production debt id in feature ledger: NO-SUCH-DEBT",
        )

    # -- release gate -----------------------------------------------------
    def test_release_gate_blocks_incomplete_ledger(self) -> None:
        errors = vfl.validate(vfl.Layout.from_root(ROOT), release=True)
        self.assert_error_contains(errors, "V1 release blocked by")


if __name__ == "__main__":
    unittest.main(verbosity=2)
