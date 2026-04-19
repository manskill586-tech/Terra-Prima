#!/usr/bin/env python3
"""Validate UTF-8 encoding and UI asset references."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

CHECK_EXTS = {".gd", ".tscn", ".json"}
CHECK_DIRS = [ROOT / "scripts" / "ui", ROOT / "scenes" / "ui", ROOT / "assets"]


def iter_files() -> list[Path]:
    files: list[Path] = []
    for base in CHECK_DIRS:
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if path.is_file() and path.suffix.lower() in CHECK_EXTS:
                files.append(path)
    return sorted(files)


def validate_utf8(paths: list[Path]) -> list[str]:
    errors: list[str] = []
    for path in paths:
        try:
            path.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            errors.append(f"UTF-8 decode failed: {path.relative_to(ROOT)} at byte {exc.start} ({exc.reason})")
    return errors


def validate_regions() -> list[str]:
    errors: list[str] = []
    region_path = ROOT / "assets" / "ui_regions.json"
    if not region_path.exists():
        return ["Missing assets/ui_regions.json"]

    try:
        data = json.loads(region_path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        return [f"Failed to parse assets/ui_regions.json: {exc}"]

    if not isinstance(data, dict):
        return ["assets/ui_regions.json root must be object"]

    for key, entry in data.items():
        if not isinstance(entry, dict):
            errors.append(f"{key}: entry must be object")
            continue

        source_path = entry.get("source_path")
        rect = entry.get("rect")

        if not isinstance(source_path, str) or not source_path.startswith("res://"):
            errors.append(f"{key}: source_path must be res:// string")
            continue

        if any(ord(ch) > 127 for ch in source_path):
            errors.append(f"{key}: source_path must be ASCII only ({source_path})")

        local_path = ROOT / source_path.replace("res://", "")
        if not local_path.exists():
            errors.append(f"{key}: missing source file {source_path}")

        if not isinstance(rect, list) or len(rect) != 4:
            errors.append(f"{key}: rect must be [x,y,w,h]")
            continue

        if not all(isinstance(v, (int, float)) for v in rect):
            errors.append(f"{key}: rect entries must be numeric")

        if rect[2] <= 0 or rect[3] <= 0:
            errors.append(f"{key}: rect width/height must be > 0")

    return errors


def main() -> int:
    files = iter_files()
    errors = validate_utf8(files)
    errors.extend(validate_regions())

    if errors:
        print("UI validation failed:")
        for err in errors:
            print(f"- {err}")
        return 1

    print(f"UI validation passed ({len(files)} files checked).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
