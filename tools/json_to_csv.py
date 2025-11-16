#!/usr/bin/env python3
"""Convert sdroot JSON assets back into CSV files expected by the firmware."""
from __future__ import annotations

import csv
import json
from pathlib import Path
from typing import Iterable, Sequence

REPO_ROOT = Path(__file__).resolve().parents[1]
SDROOT = REPO_ROOT / "sdroot"


def _write_csv(path: Path, headers: Sequence[str], rows: Iterable[Sequence[str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter=";")
        writer.writerow(headers)
        for row in rows:
            writer.writerow(row)


def convert_calendar() -> None:
    source = SDROOT / "calendar.json"
    target = SDROOT / "calendar.csv"
    data = json.loads(source.read_text(encoding="utf-8"))
    entries = data.get("entries", [])

    headers = (
        "year",
        "month",
        "day",
        "tts_sentence",
        "tts_interval_min",
        "theme_box_id",
        "light_show_id",
        "color_range_id",
        "note",
    )

    def rows() -> Iterable[Sequence[str]]:
        for entry in entries:
            date = entry.get("date", {})
            tts = entry.get("tts", {})
            audio = entry.get("audio", {})
            lights = entry.get("lights", {})
            yield (
                str(date.get("year", "")),
                str(date.get("month", "")),
                str(date.get("day", "")),
                tts.get("sentence", ""),
                str(tts.get("interval_min", "")),
                str(audio.get("theme_box_id", "")),
                str(lights.get("pattern_id", "")),
                str(lights.get("color_id", "")),
                entry.get("note", ""),
            )

    _write_csv(target, headers, rows())


def convert_theme_boxes() -> None:
    source = SDROOT / "theme_boxes.json"
    target = SDROOT / "theme_boxes.csv"
    data = json.loads(source.read_text(encoding="utf-8"))
    boxes = data.get("theme_boxes", [])

    def format_entry(value: int) -> str:
        try:
            return f"{int(value):03d}"
        except (TypeError, ValueError):
            return ""

    headers = ("theme_box_id", "entries")

    def rows() -> Iterable[Sequence[str]]:
        for box in boxes:
            entries = [format_entry(v) for v in box.get("entries", []) if format_entry(v)]
            yield (
                str(box.get("id", "")),
                ",".join(entries),
            )

    _write_csv(target, headers, rows())


def convert_light_patterns() -> None:
    source = SDROOT / "light_patterns.json"
    target = SDROOT / "light_patterns.csv"
    data = json.loads(source.read_text(encoding="utf-8"))
    patterns = data.get("patterns", [])

    headers = (
        "pattern_id",
        "color_cycle_sec",
        "bright_cycle_sec",
        "fade_width",
        "min_brightness",
        "gradient_speed",
        "center_x",
        "center_y",
        "radius",
        "window_width",
        "radius_osc",
        "x_amp",
        "y_amp",
        "x_cycle_sec",
        "y_cycle_sec",
        "note",
    )

    numeric_fields = headers[1:-1]

    def fmt(value) -> str:
        if isinstance(value, float) and value.is_integer():
            return f"{value:.1f}"
        return str(value)

    def rows() -> Iterable[Sequence[str]]:
        for pattern in patterns:
            params = pattern.get("params", {})
            values = [fmt(params.get(field, "")) for field in numeric_fields]
            yield (pattern.get("id", ""), *values, pattern.get("label", ""))

    _write_csv(target, headers, rows())



def main() -> None:
    convert_calendar()
    convert_theme_boxes()
    convert_light_patterns()
    print("CSV assets regenerated under sdroot/")


if __name__ == "__main__":
    main()
