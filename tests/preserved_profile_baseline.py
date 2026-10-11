"""Preserve the original watch catalog while validating its Reader extension."""
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]


def baseline_bytes(name):
    raw = (ROOT / name).read_bytes()
    if name != "productivity-manifest.json":
        return raw
    catalog = json.loads(raw)
    reader = json.loads((ROOT / "Apps/ebook_reader.json").read_text())
    expected = [{
        "id": "ebook-reader", "version": reader["version"],
        "classification": "e-ink", "target": "x4",
        "source_path": "reader/ReaderApp.cpp",
        "manifest_path": "Apps/ebook_reader.json",
        "build_script": "scripts/build_reader.py",
        "migration_record": "reader/MIGRATION.md",
        "upstream_lock": "reader/upstream.json",
    }]
    if catalog.pop("e_ink_apps", None) != expected:
        raise AssertionError("Reader catalog and built manifest must agree")
    # The historical SHA still covers every legacy row and its order. Only the
    # separately checked Reader extension is projected out of that old fixture.
    return (json.dumps(catalog, indent=2) + "\n").encode()
