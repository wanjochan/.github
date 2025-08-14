#!/usr/bin/env python3
"""
Process raw herb data into schema-compliant JSON and Markdown

- Reads raw snapshots from `../data/{id}.raw.json`
- Produces JSON in `../json/{id}.json` if valid; otherwise writes draft as `../json/{id}.draft.json`
- Renders Markdown using `../templates/template.md` into `../markdown/{id}.md`

CLI examples:
  python herb_processor.py --id rosemary
  python herb_processor.py --all
"""
from __future__ import annotations

import argparse
import json
import logging
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "data"
JSON_DIR = ROOT / "json"
MD_DIR = ROOT / "markdown"
TEMPLATE_PATH = ROOT / "templates" / "template.md"
SCHEMA_PATH = ROOT / "schema.json"


def get_logger(name: str) -> logging.Logger:
    logger = logging.getLogger(name)
    if not logger.handlers:
        handler = logging.StreamHandler()
        formatter = logging.Formatter(
            fmt="%(asctime)s | %(levelname)s | %(name)s | %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
        )
        handler.setFormatter(formatter)
        logger.addHandler(handler)
        logger.setLevel(logging.INFO)
    return logger


logger = get_logger("herb_processor")


def load_json(path: Path) -> Dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def save_json(path: Path, data: Dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)


def load_schema() -> Optional[Dict]:
    try:
        return load_json(SCHEMA_PATH)
    except Exception as e:
        logger.warning("Failed to read schema.json: %s", e)
        return None


def validate_against_schema(data: Dict, schema: Optional[Dict]) -> Tuple[bool, Optional[str]]:
    if schema is None:
        return True, None
    try:
        import jsonschema  # type: ignore
        jsonschema.validate(instance=data, schema=schema)
        return True, None
    except ModuleNotFoundError:
        logger.warning("jsonschema not installed; skipping validation")
        return True, None
    except Exception as e:
        return False, str(e)


def render_template(template_text: str, context: Dict[str, Any]) -> str:
    # Very small Mustache-like replacement for {{var}}
    def replace(match: re.Match[str]) -> str:
        key = match.group(1).strip()
        val = context.get(key)
        if val is None:
            return ""
        if isinstance(val, list):
            return ", ".join(str(v) for v in val)
        return str(val)

    return re.sub(r"\{\{\s*([^}]+?)\s*\}\}", replace, template_text)


def build_default_record(raw: Dict) -> Dict:
    # This is a conservative mapping. It expects later enrichment by editors.
    herb_id = raw.get("id") or "unknown-id"
    references = []
    # Try to load references file if present
    refs_path = ROOT / "references" / f"{herb_id}.refs.json"
    if refs_path.exists():
        try:
            refs_obj = load_json(refs_path)
            for _, r in sorted(refs_obj.items(), key=lambda x: int(x[0])):
                references.append({
                    "title": r.get("title", ""),
                    "org": r.get("org", ""),
                    "year": int(r.get("year", 0)) if str(r.get("year", "")).isdigit() else 0,
                    "url": r.get("url", ""),
                })
        except Exception:
            pass

    record = {
        "id": herb_id,
        "cn_name": raw.get("cn_name", ""),
        "en_name": raw.get("en_name", ""),
        "scientific_name": raw.get("scientific_name", ""),
        "category": raw.get("category", "edible"),
        "edible_parts": raw.get("edible_parts", []),
        "toxicity": raw.get("toxicity", {
            "status": "safe",
            "toxic_parts": [],
            "notes": "",
            "sources": []
        }),
        "cultivation": raw.get("cultivation", {
            "hardiness": "",
            "sun": "",
            "soil": "",
            "pH": "",
            "watering": "",
            "propagation": [],
            "companion": [],
            "pests": [],
        }),
        "varieties": raw.get("varieties", []),
        "chemistry": raw.get("chemistry", []),
        "culinary_uses": raw.get("culinary_uses", []),
        "storage": raw.get("storage", ""),
        "lookalikes": raw.get("lookalikes", []),
        "images": raw.get("images", []),
        "references": references or raw.get("references", []),
        "tags": raw.get("tags", []),
    }
    # Optional rich-text fields for Markdown rendering
    # These are not enforced by schema, but help produce long-form content
    for key in [
        "key_features",
        "difficulty",
        "morphology_content",
        "lookalike_comparison",
        "fertilizing",
        "varieties_content",
        "chemistry_content",
        "culinary_content",
        "storage_methods",
        "toxicology_content",
    ]:
        if key in raw and isinstance(raw.get(key), (str, list)):
            record[key] = raw.get(key)
    return record


def to_markdown_context(record: Dict) -> Dict[str, Any]:
    category_cn = {"edible": "可食用", "non_edible": "不可食用"}.get(record.get("category", "edible"), "")
    edible_parts = record.get("edible_parts") or []
    toxicity = record.get("toxicity") or {}

    def build_edible_or_toxic_parts() -> str:
        if toxicity.get("status") in ("toxic", "caution", "external_only"):
            parts = ", ".join(toxicity.get("toxic_parts", [])) or "未知"
            return f"毒部位: {parts}"
        parts = ", ".join(edible_parts) or "未知"
        return f"可食部位: {parts}"

    references_list = []
    for r in record.get("references", []):
        title = r.get("title", "")
        org = r.get("org", "")
        year = r.get("year", "")
        url = r.get("url", "")
        if url:
            references_list.append(f"- [{title}]({url}) — {org}, {year}")
        else:
            references_list.append(f"- {title} — {org}, {year}")

    ctx = {
        "cn_name": record.get("cn_name", ""),
        "en_name": record.get("en_name", ""),
        "scientific_name": record.get("scientific_name", ""),
        "category_cn": category_cn,
        "edible_or_toxic_parts": build_edible_or_toxic_parts(),
        "key_features": record.get("key_features", ""),
        "difficulty": record.get("difficulty", ""),
        "morphology_content": record.get("morphology_content", ""),
        "lookalike_comparison": record.get("lookalike_comparison", ""),
        "hardiness": record.get("cultivation", {}).get("hardiness", ""),
        "sun": record.get("cultivation", {}).get("sun", ""),
        "soil": record.get("cultivation", {}).get("soil", ""),
        "pH": record.get("cultivation", {}).get("pH", ""),
        "watering": record.get("cultivation", {}).get("watering", ""),
        "fertilizing": record.get("fertilizing", ""),
        "propagation": ", ".join(record.get("cultivation", {}).get("propagation", [])),
        "pests": ", ".join(record.get("cultivation", {}).get("pests", [])),
        "companion": ", ".join(record.get("cultivation", {}).get("companion", [])),
        "varieties_content": record.get("varieties_content")
            if record.get("varieties_content")
            else "\n".join(f"- {v.get('name','')}" for v in record.get("varieties", [])),
        "chemistry_content": record.get("chemistry_content")
            if record.get("chemistry_content")
            else "\n".join(
                f"- {c.get('compound','')} ({c.get('class','')}) — {c.get('role','')}" for c in record.get("chemistry", [])
            ),
        "culinary_content": record.get("culinary_content")
            if record.get("culinary_content")
            else "\n".join(f"- {u}" for u in record.get("culinary_uses", [])),
        "storage_methods": record.get("storage_methods", record.get("storage", "")),
        "toxicology_content": record.get("toxicology_content", toxicity.get("notes", "")),
        "references_list": "\n".join(references_list),
    }
    return ctx


def process_one(herb_id: str) -> None:
    raw_path = DATA_DIR / f"{herb_id}.raw.json"
    if not raw_path.exists():
        logger.error("Raw file not found: %s", raw_path)
        return

    try:
        raw = load_json(raw_path)
    except Exception as e:
        logger.error("Failed to load raw JSON: %s", e)
        return

    record = build_default_record(raw)
    schema = load_schema()

    is_valid, error = validate_against_schema(record, schema)
    if is_valid:
        out_path = JSON_DIR / f"{herb_id}.json"
        save_json(out_path, record)
        logger.info("Wrote schema-compliant JSON: %s", out_path)
    else:
        out_path = JSON_DIR / f"{herb_id}.draft.json"
        save_json(out_path, {"error": error, "record": record})
        logger.warning("Record did not pass schema validation: %s", error)
        logger.info("Wrote draft JSON for review: %s", out_path)

    # Markdown rendering
    try:
        template_text = TEMPLATE_PATH.read_text(encoding="utf-8")
    except Exception as e:
        logger.error("Failed to read template: %s", e)
        return

    context = to_markdown_context(record)
    md_text = render_template(template_text, context)

    md_out = MD_DIR / f"{herb_id}.md"
    md_out.parent.mkdir(parents=True, exist_ok=True)
    md_out.write_text(md_text, encoding="utf-8")
    logger.info("Wrote Markdown: %s", md_out)


def list_ids_from_raw() -> List[str]:
    ids: List[str] = []
    if not DATA_DIR.exists():
        return ids
    for p in sorted(DATA_DIR.glob("*.raw.json")):
        ids.append(p.stem.replace(".raw", ""))
    return ids


def main() -> None:
    parser = argparse.ArgumentParser(description="Herbiverse processor")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--id", type=str, help="Herb id to process")
    group.add_argument("--all", action="store_true", help="Process all raw records")
    args = parser.parse_args()

    if args.id:
        process_one(args.id)
    else:
        ids = list_ids_from_raw()
        if not ids:
            logger.warning("No raw files found in %s", DATA_DIR)
            return
        for herb_id in ids:
            process_one(herb_id)


if __name__ == "__main__":
    main()
