#!/usr/bin/env python3
"""Validate the V1 feature ledger against production editor entry points."""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
LEDGER_PATH = ROOT / "tools" / "audit" / "feature_ledger.json"
DEBT_PATH = ROOT / "tools" / "audit" / "editor_production_debt.json"
EDITOR_SOURCE = ROOT / "apps" / "editor_cpp" / "src"

VALID_STATUSES = {"planned", "in_progress", "blocked", "complete", "retired"}
VALID_ACCEPTANCE = {True, False, "not_applicable"}
ACCEPTANCE_KEYS = {
    "editorRoundTrip",
    "runtimeConsumption",
    "explicitDiagnostics",
    "versionedMigration",
    "platformMatrix",
    "automatedTests",
    "cleanMachine",
    "productionReady",
}
ACTION_PATTERN = re.compile(
    r"ImGui::(?:MenuItem|Button|SmallButton|Selectable|Checkbox|"
    r"Input[A-Za-z]*|Drag[A-Za-z]*|Slider[A-Za-z]*|Combo)\s*\("
)
PANEL_PATTERN = re.compile(r'\badd\(\s*"([^"]+)"')
# Panels that self-register via DSE_EDITOR_PANEL(...) set their id inside the
# registrar lambda as `e.id = "..."`; capture those blocks across all sources.
SELF_REGISTER_BLOCK = re.compile(r"DSE_EDITOR_PANEL\(.*?\n\}\);", re.DOTALL)
SELF_REGISTER_ID = re.compile(r'\.id\s*=\s*"([^"]+)"')
# Some panels register from a tools table whose id is assigned via a variable
# (e.g. editor_2d_tools.cpp's ToolReg table: {"sprite_slicer", "Sprite Slicer",
# &state, DrawFn}). The plain SELF_REGISTER_ID regex cannot see those, so scan
# the table rows explicitly. Requiring `&` as the third element keeps the match
# specific to registrar tables (state-pointer rows) and avoids generic
# {"key", "label", ...} matches elsewhere.
TOOL_TABLE_ROW = re.compile(r'\{\s*"([a-z0-9_]+)"\s*,\s*"[^"]+"\s*,\s*&')


def fail(errors: list[str], message: str) -> None:
    errors.append(message)


def load_json(path: Path, errors: list[str]) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(errors, f"{path.relative_to(ROOT)}: {exc}")
        return {}


def relative_existing_path(value: str, errors: list[str], owner: str) -> None:
    path = ROOT / value
    if not path.exists():
        fail(errors, f"{owner}: referenced path does not exist: {value}")


def collect_registered_panels(errors: list[str]) -> set[str]:
    app_path = EDITOR_SOURCE / "editor_app.cpp"
    try:
        source = app_path.read_text(encoding="utf-8")
    except OSError as exc:
        fail(errors, f"cannot read {app_path.relative_to(ROOT)}: {exc}")
        return set()
    panels = set(PANEL_PATTERN.findall(source))

    # Panels registered from other modules via the DSE_EDITOR_PANEL macro never
    # appear in editor_app.cpp's add(...) calls, so scan every editor source for
    # the id assigned inside each self-registrar block.
    try:
        for path in EDITOR_SOURCE.rglob("*.cpp"):
            module_source = path.read_text(encoding="utf-8", errors="replace")
            if "DSE_EDITOR_PANEL(" not in module_source:
                continue
            for block in SELF_REGISTER_BLOCK.findall(module_source):
                panels.update(SELF_REGISTER_ID.findall(block))
            # Tool-table registrars assign `e.id` from a variable, so the literal
            # ids only appear in the table rows (see TOOL_TABLE_ROW).
            panels.update(TOOL_TABLE_ROW.findall(module_source))
    except OSError as exc:
        fail(errors, f"cannot scan self-registered editor panels: {exc}")
    return panels


def collect_action_sources(errors: list[str]) -> set[str]:
    result: set[str] = set()
    try:
        paths = EDITOR_SOURCE.rglob("*.cpp")
        for path in paths:
            if "ui_tests" in path.parts:
                continue
            source = path.read_text(encoding="utf-8")
            if ACTION_PATTERN.search(source):
                result.add(path.relative_to(ROOT).as_posix())
    except OSError as exc:
        fail(errors, f"cannot scan editor action sources: {exc}")
    return result


def validate(release: bool = False) -> list[str]:
    errors: list[str] = []
    ledger = load_json(LEDGER_PATH, errors)
    debt = load_json(DEBT_PATH, errors)
    if not ledger or not debt:
        return errors

    if ledger.get("schemaVersion") != 1:
        fail(errors, "feature ledger schemaVersion must be 1")
    if debt.get("schemaVersion") != 1:
        fail(errors, "production debt schemaVersion must be 1")

    stages = ledger.get("stages")
    features = ledger.get("features")
    if not isinstance(stages, list) or not stages:
        fail(errors, "feature ledger requires a non-empty stages array")
        stages = []
    if not isinstance(features, list) or not features:
        fail(errors, "feature ledger requires a non-empty features array")
        features = []

    stage_numbers: list[int] = []
    stage_ids: list[str] = []
    for index, stage in enumerate(stages):
        owner = f"stages[{index}]"
        number = stage.get("number")
        stage_id = stage.get("id")
        if not isinstance(number, int):
            fail(errors, f"{owner}: number must be an integer")
        else:
            stage_numbers.append(number)
        if not isinstance(stage_id, str) or not stage_id:
            fail(errors, f"{owner}: id must be a non-empty string")
        else:
            stage_ids.append(stage_id)
        if not isinstance(stage.get("name"), str) or not stage["name"]:
            fail(errors, f"{owner}: name must be a non-empty string")

    if stage_numbers != list(range(19)):
        fail(errors, "stages must contain dependency-ordered numbers 0 through 18")
    for duplicate in sorted(key for key, count in Counter(stage_ids).items() if count > 1):
        fail(errors, f"duplicate stage id: {duplicate}")

    feature_ids: list[str] = []
    feature_stages: list[int] = []
    ledger_panels: list[str] = []
    ledger_action_sources: list[str] = []
    ledger_debt_ids: list[str] = []

    for index, feature in enumerate(features):
        feature_id = feature.get("id")
        owner = f"features[{index}]"
        if not isinstance(feature_id, str) or not feature_id:
            fail(errors, f"{owner}: id must be a non-empty string")
            feature_id = owner
        else:
            feature_ids.append(feature_id)
            owner = feature_id

        stage = feature.get("stage")
        if not isinstance(stage, int) or stage not in stage_numbers:
            fail(errors, f"{owner}: stage must reference a declared stage number")
        else:
            feature_stages.append(stage)

        status = feature.get("status")
        if status not in VALID_STATUSES:
            fail(errors, f"{owner}: invalid status {status!r}")
        if not isinstance(feature.get("summary"), str) or not feature["summary"]:
            fail(errors, f"{owner}: summary must be a non-empty string")

        editor = feature.get("editor")
        if not isinstance(editor, dict):
            fail(errors, f"{owner}: editor must be an object")
            editor = {}
        panel_ids = editor.get("panelIds")
        action_sources = editor.get("actionSources")
        if not isinstance(panel_ids, list) or not all(isinstance(v, str) for v in panel_ids):
            fail(errors, f"{owner}: editor.panelIds must be a string array")
            panel_ids = []
        if not isinstance(action_sources, list) or not all(
            isinstance(v, str) for v in action_sources
        ):
            fail(errors, f"{owner}: editor.actionSources must be a string array")
            action_sources = []
        ledger_panels.extend(panel_ids)
        ledger_action_sources.extend(action_sources)

        list_fields = (
            "runtimeConsumers",
            "assetFormats",
            "requiredPlatforms",
            "tests",
            "cleanMachineTests",
            "productionDebtIds",
            "evidencePaths",
            "blockers",
        )
        for field in list_fields:
            values = feature.get(field)
            if not isinstance(values, list) or not all(isinstance(v, str) for v in values):
                fail(errors, f"{owner}: {field} must be a string array")

        for path in action_sources:
            relative_existing_path(path, errors, owner)
        for field in ("runtimeConsumers", "tests", "cleanMachineTests", "evidencePaths"):
            for path in feature.get(field, []):
                relative_existing_path(path, errors, owner)

        debt_ids = feature.get("productionDebtIds", [])
        ledger_debt_ids.extend(debt_ids)

        acceptance = feature.get("acceptance")
        if not isinstance(acceptance, dict):
            fail(errors, f"{owner}: acceptance must be an object")
            acceptance = {}
        missing_acceptance = ACCEPTANCE_KEYS - set(acceptance)
        extra_acceptance = set(acceptance) - ACCEPTANCE_KEYS
        if missing_acceptance:
            fail(errors, f"{owner}: missing acceptance keys: {sorted(missing_acceptance)}")
        if extra_acceptance:
            fail(errors, f"{owner}: unknown acceptance keys: {sorted(extra_acceptance)}")
        for key, value in acceptance.items():
            if value not in VALID_ACCEPTANCE:
                fail(errors, f"{owner}: acceptance.{key} has invalid value {value!r}")

        if status in {"complete", "retired"}:
            false_keys = sorted(key for key, value in acceptance.items() if value is False)
            if false_keys:
                fail(errors, f"{owner}: {status} feature has failed acceptance: {false_keys}")
            if feature.get("blockers"):
                fail(errors, f"{owner}: {status} feature cannot have blockers")

    for duplicate in sorted(key for key, count in Counter(feature_ids).items() if count > 1):
        fail(errors, f"duplicate feature id: {duplicate}")
    for missing_stage in sorted(set(stage_numbers) - set(feature_stages)):
        fail(errors, f"stage {missing_stage} has no feature entry")

    registered_panels = collect_registered_panels(errors)
    ledger_panel_set = set(ledger_panels)
    for panel_id in sorted(registered_panels - ledger_panel_set):
        fail(errors, f"registered editor panel is not owned by the ledger: {panel_id}")
    for panel_id in sorted(ledger_panel_set - registered_panels):
        fail(errors, f"stale ledger panel id is not registered by the editor: {panel_id}")
    for duplicate in sorted(key for key, count in Counter(ledger_panels).items() if count > 1):
        fail(errors, f"editor panel is owned by multiple ledger features: {duplicate}")

    scanned_action_sources = collect_action_sources(errors)
    ledger_action_set = set(ledger_action_sources)
    for path in sorted(scanned_action_sources - ledger_action_set):
        fail(errors, f"interactive editor source is not owned by the ledger: {path}")
    for path in sorted(ledger_action_set - scanned_action_sources):
        fail(errors, f"stale ledger action source has no interactive controls: {path}")
    for duplicate in sorted(
        key for key, count in Counter(ledger_action_sources).items() if count > 1
    ):
        fail(errors, f"interactive editor source has multiple ledger owners: {duplicate}")

    debt_entries = debt.get("entries")
    if not isinstance(debt_entries, list):
        fail(errors, "production debt entries must be an array")
        debt_entries = []
    debt_ids = [
        entry.get("id")
        for entry in debt_entries
        if isinstance(entry, dict) and isinstance(entry.get("id"), str)
    ]
    debt_id_set = set(debt_ids)
    ledger_debt_set = set(ledger_debt_ids)
    for debt_id in sorted(debt_id_set - ledger_debt_set):
        fail(errors, f"production debt is not assigned to a ledger feature: {debt_id}")
    for debt_id in sorted(ledger_debt_set - debt_id_set):
        fail(errors, f"stale production debt id in feature ledger: {debt_id}")
    for duplicate in sorted(
        key for key, count in Counter(ledger_debt_ids).items() if count > 1
    ):
        fail(errors, f"production debt has multiple ledger owners: {duplicate}")

    if release:
        for feature in features:
            feature_id = feature.get("id", "<unknown>")
            if feature.get("status") not in {"complete", "retired"}:
                fail(
                    errors,
                    f"V1 release blocked by incomplete feature {feature_id}: "
                    f"{feature.get('status')}",
                )
            acceptance = feature.get("acceptance", {})
            failed = sorted(key for key, value in acceptance.items() if value is False)
            if failed:
                fail(
                    errors,
                    f"V1 release blocked by failed acceptance in {feature_id}: {failed}",
                )
        if debt_entries:
            fail(
                errors,
                f"V1 release blocked by {len(debt_entries)} production-debt entries",
            )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--release",
        action="store_true",
        help="enforce the final V1 Go/No-Go gate",
    )
    args = parser.parse_args()

    errors = validate(release=args.release)
    if errors:
        print("Feature ledger verification failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    ledger = json.loads(LEDGER_PATH.read_text(encoding="utf-8"))
    status_counts = Counter(feature["status"] for feature in ledger["features"])
    registered_panels = collect_registered_panels([])
    action_sources = collect_action_sources([])
    print(
        "Feature ledger verified: "
        f"{len(ledger['features'])} features, "
        f"{len(registered_panels)} panels, "
        f"{len(action_sources)} interactive sources, "
        f"statuses={dict(sorted(status_counts.items()))}."
    )
    if args.release:
        print("V1 release Go/No-Go gate passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
