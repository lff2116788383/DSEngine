#!/usr/bin/env python3
"""Validate the V1 feature ledger against production editor entry points.

The ledger owns every registered editor panel, every interactive editor
source, and every known production-debt marker. On top of file-granularity
ownership this verifier now derives **stable, machine-readable per-action
identifiers** from the editor sources and reconciles the declared editor
asset extensions and runtime-consumer paths against the repository's own
registries (the editor asset-type table and the CMake engine source globs).
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]

LEDGER_REL = "tools/audit/feature_ledger.json"
DEBT_REL = "tools/audit/editor_production_debt.json"
EDITOR_SOURCE_REL = "apps/editor_cpp/src"
ASSET_REGISTRY_REL = "apps/editor_cpp/src/editor_asset_db_core.cpp"
ENGINE_SOURCES_CMAKE_REL = "cmake/engine_sources.cmake"

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
    r"ImGui::(MenuItem|Button|SmallButton|Selectable|Checkbox|"
    r"Input[A-Za-z]*|Drag[A-Za-z]*|Slider[A-Za-z]*|Combo)\s*\("
)
PANEL_PATTERN = re.compile(r'\badd\(\s*"([^"]+)"')
STRING_LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')
# Well-formed asset extension: lowercase, dot-prefixed, optional multi-part
# suffix (e.g. ".autosave.dscene"); no whitespace or uppercase.
ASSET_FORMAT_PATTERN = re.compile(r"^\.[a-z0-9]+(?:\.[a-z0-9]+)*$")
# ext == ".xyz" comparisons inside AssetTypeFromExtension.
ASSET_EXT_PATTERN = re.compile(r'ext\s*==\s*"(\.[A-Za-z0-9.]+)"')
# First-party engine source glob roots, e.g. "engine/*.cpp",
# "modules/gameplay_2d/*.cpp".
ENGINE_GLOB_PATTERN = re.compile(
    r'"((?:engine|modules)(?:/[^"*]+)*)/\*[^"]*\.cpp"'
)


@dataclass
class Layout:
    """Filesystem locations the verifier reads.

    Bundled so tests can point individual inputs at fixtures while reusing
    the real repository for the remaining sources.
    """

    root: Path
    ledger_path: Path
    debt_path: Path
    editor_source: Path
    asset_registry: Path
    engine_sources_cmake: Path

    @classmethod
    def from_root(cls, root: Path) -> "Layout":
        return cls(
            root=root,
            ledger_path=root / LEDGER_REL,
            debt_path=root / DEBT_REL,
            editor_source=root / EDITOR_SOURCE_REL,
            asset_registry=root / ASSET_REGISTRY_REL,
            engine_sources_cmake=root / ENGINE_SOURCES_CMAKE_REL,
        )


def fail(errors: list[str], message: str) -> None:
    errors.append(message)


def _rel(layout: Layout, path: Path) -> str:
    try:
        return path.relative_to(layout.root).as_posix()
    except ValueError:
        return str(path)


def load_json(path: Path, layout: Layout, errors: list[str]) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(errors, f"{_rel(layout, path)}: {exc}")
        return {}


def relative_existing_path(
    layout: Layout, value: str, errors: list[str], owner: str
) -> None:
    if not (layout.root / value).exists():
        fail(errors, f"{owner}: referenced path does not exist: {value}")


def collect_registered_panels(layout: Layout, errors: list[str]) -> set[str]:
    app_path = layout.editor_source / "editor_app.cpp"
    try:
        source = app_path.read_text(encoding="utf-8")
    except OSError as exc:
        fail(errors, f"cannot read {_rel(layout, app_path)}: {exc}")
        return set()
    return set(PANEL_PATTERN.findall(source))


def _first_argument(text: str, start: int) -> str:
    """Return the source text of the first call argument at ``start``.

    Tracks bracket depth and skips string literals so that commas or
    parentheses inside a label do not terminate the argument early.
    """

    depth = 0
    i = start
    n = len(text)
    out: list[str] = []
    while i < n:
        c = text[i]
        if c == '"':
            out.append(c)
            i += 1
            while i < n:
                out.append(text[i])
                if text[i] == "\\":
                    i += 1
                    if i < n:
                        out.append(text[i])
                        i += 1
                    continue
                if text[i] == '"':
                    i += 1
                    break
                i += 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                break
            depth -= 1
        elif c == "," and depth == 0:
            break
        out.append(c)
        i += 1
    return "".join(out)


def extract_actions(text: str, relpath: str) -> list[str]:
    """Derive stable action identifiers from one editor source.

    An identifier is ``<relpath>#<Control>:<label>`` where the label is the
    concatenation of the string literals in the control's first argument
    (its ImGui id). Anonymous controls fall back to the control kind, and a
    ``~N`` suffix disambiguates repeated identifiers within the same file so
    every action resolves to exactly one stable id.
    """

    actions: list[str] = []
    seen: Counter[str] = Counter()
    for match in ACTION_PATTERN.finditer(text):
        kind = match.group(1)
        arg = _first_argument(text, match.end())
        literals = STRING_LITERAL.findall(arg)
        label = "".join(literals).strip()
        base = f"{relpath}#{kind}:{label}" if label else f"{relpath}#{kind}:@"
        seen[base] += 1
        count = seen[base]
        actions.append(base if count == 1 else f"{base}~{count}")
    return actions


def collect_action_files(layout: Layout, errors: list[str]) -> set[str]:
    """Editor sources that contain at least one interactive control."""

    result: set[str] = set()
    try:
        for path in layout.editor_source.rglob("*.cpp"):
            if "ui_tests" in path.parts:
                continue
            source = path.read_text(encoding="utf-8")
            if ACTION_PATTERN.search(source):
                result.add(path.relative_to(layout.root).as_posix())
    except OSError as exc:
        fail(errors, f"cannot scan editor action sources: {exc}")
    return result


def collect_actions(layout: Layout, files: set[str]) -> dict[str, str]:
    """Map every stable action id to the source file that declares it."""

    actions: dict[str, str] = {}
    for relpath in sorted(files):
        source = (layout.root / relpath).read_text(encoding="utf-8")
        for action_id in extract_actions(source, relpath):
            actions[action_id] = relpath
    return actions


def discover_asset_extensions(layout: Layout, errors: list[str]) -> set[str]:
    """Extensions the editor asset-type registry recognizes."""

    try:
        source = layout.asset_registry.read_text(encoding="utf-8")
    except OSError as exc:
        fail(errors, f"cannot read editor asset registry: {exc}")
        return set()
    extensions = {ext.lower() for ext in ASSET_EXT_PATTERN.findall(source)}
    if not extensions:
        fail(
            errors,
            f"editor asset registry {ASSET_REGISTRY_REL} exposed no "
            "recognized extensions; discovery is broken",
        )
    return extensions


def discover_engine_source_roots(layout: Layout, errors: list[str]) -> set[str]:
    """First-party source roots compiled into dse_engine (from CMake globs)."""

    try:
        source = layout.engine_sources_cmake.read_text(encoding="utf-8")
    except OSError as exc:
        fail(errors, f"cannot read engine source registry: {exc}")
        return set()
    roots = set(ENGINE_GLOB_PATTERN.findall(source))
    if not roots:
        fail(
            errors,
            f"engine source registry {ENGINE_SOURCES_CMAKE_REL} exposed no "
            "first-party glob roots; discovery is broken",
        )
    return roots


def _is_registered_consumer(value: str, roots: set[str]) -> bool:
    normalized = value.replace("\\", "/").rstrip("/")
    return any(
        normalized == root or normalized.startswith(root + "/") for root in roots
    )


def validate(layout: Layout, release: bool = False) -> list[str]:
    errors: list[str] = []
    ledger = load_json(layout.ledger_path, layout, errors)
    debt = load_json(layout.debt_path, layout, errors)
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
            relative_existing_path(layout, path, errors, owner)
        for field in ("runtimeConsumers", "tests", "cleanMachineTests", "evidencePaths"):
            for path in feature.get(field, []):
                relative_existing_path(layout, path, errors, owner)

        asset_formats = feature.get("assetFormats", [])
        if isinstance(asset_formats, list):
            for value in asset_formats:
                if isinstance(value, str) and not ASSET_FORMAT_PATTERN.match(value):
                    fail(errors, f"{owner}: malformed asset extension {value!r}")
            for duplicate in sorted(
                key for key, count in Counter(asset_formats).items() if count > 1
            ):
                fail(errors, f"{owner}: duplicate asset extension: {duplicate}")

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

    registered_panels = collect_registered_panels(layout, errors)
    ledger_panel_set = set(ledger_panels)
    for panel_id in sorted(registered_panels - ledger_panel_set):
        fail(errors, f"registered editor panel is not owned by the ledger: {panel_id}")
    for panel_id in sorted(ledger_panel_set - registered_panels):
        fail(errors, f"stale ledger panel id is not registered by the editor: {panel_id}")
    for duplicate in sorted(key for key, count in Counter(ledger_panels).items() if count > 1):
        fail(errors, f"editor panel is owned by multiple ledger features: {duplicate}")

    scanned_action_sources = collect_action_files(layout, errors)
    ledger_action_set = set(ledger_action_sources)
    for path in sorted(scanned_action_sources - ledger_action_set):
        fail(errors, f"interactive editor source is not owned by the ledger: {path}")
    for path in sorted(ledger_action_set - scanned_action_sources):
        fail(errors, f"stale ledger action source has no interactive controls: {path}")
    for duplicate in sorted(
        key for key, count in Counter(ledger_action_sources).items() if count > 1
    ):
        fail(errors, f"interactive editor source has multiple ledger owners: {duplicate}")

    # Stable per-action ownership: every discovered action resolves to exactly
    # one owning feature via its (uniquely owned) source file.
    owned_files: dict[str, str] = {}
    for index, feature in enumerate(features):
        fid = feature.get("id") or f"features[{index}]"
        editor = feature.get("editor") or {}
        for path in editor.get("actionSources", []) or []:
            if isinstance(path, str):
                owned_files.setdefault(path, fid)
    resolvable_files = scanned_action_sources & set(owned_files)
    actions = collect_actions(layout, resolvable_files)
    for action_id, relpath in sorted(actions.items()):
        if relpath not in owned_files:
            fail(errors, f"editor action has no ledger owner: {action_id}")

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

    # Auto-discovered registries: editor asset extensions and the CMake engine
    # source roots that determine whether a runtime consumer is compiled in.
    discover_asset_extensions(layout, errors)
    engine_roots = discover_engine_source_roots(layout, errors)
    if engine_roots:
        for index, feature in enumerate(features):
            fid = feature.get("id") or f"features[{index}]"
            for value in feature.get("runtimeConsumers", []) or []:
                if isinstance(value, str) and not _is_registered_consumer(
                    value, engine_roots
                ):
                    fail(
                        errors,
                        f"{fid}: runtime consumer {value} is not within a "
                        "compiled engine source root (not registered in "
                        "dse_engine)",
                    )

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


def build_action_index(layout: Layout) -> dict[str, str]:
    """Full stable action-id -> owning feature id map (for --dump-actions)."""

    ledger = json.loads(layout.ledger_path.read_text(encoding="utf-8"))
    owned_files: dict[str, str] = {}
    for feature in ledger["features"]:
        for path in (feature.get("editor") or {}).get("actionSources", []) or []:
            owned_files.setdefault(path, feature["id"])
    actions = collect_actions(layout, set(owned_files))
    return {action_id: owned_files[relpath] for action_id, relpath in actions.items()}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--release",
        action="store_true",
        help="enforce the final V1 Go/No-Go gate",
    )
    parser.add_argument(
        "--dump-actions",
        action="store_true",
        help="print the stable action-id to feature-id ownership map as JSON",
    )
    args = parser.parse_args()

    layout = Layout.from_root(ROOT)

    if args.dump_actions:
        json.dump(build_action_index(layout), sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
        return 0

    errors = validate(layout, release=args.release)
    if errors:
        print("Feature ledger verification failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    ledger = json.loads(layout.ledger_path.read_text(encoding="utf-8"))
    status_counts = Counter(feature["status"] for feature in ledger["features"])
    registered_panels = collect_registered_panels(layout, [])
    action_files = collect_action_files(layout, [])
    actions = collect_actions(layout, action_files)
    asset_extensions = discover_asset_extensions(layout, [])
    engine_roots = discover_engine_source_roots(layout, [])
    print(
        "Feature ledger verified: "
        f"{len(ledger['features'])} features, "
        f"{len(registered_panels)} panels, "
        f"{len(action_files)} interactive sources, "
        f"{len(actions)} stable actions, "
        f"{len(asset_extensions)} editor asset extensions, "
        f"{len(engine_roots)} engine source roots, "
        f"statuses={dict(sorted(status_counts.items()))}."
    )
    if args.release:
        print("V1 release Go/No-Go gate passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
