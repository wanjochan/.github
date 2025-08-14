#!/usr/bin/env python3
"""
Herb data collector for Herbiverse.

Features:
- Fetch raw pages from authoritative sources
- Manage basic reference metadata
- Store raw snapshots in `../data/{id}.raw.json`
- Validate sources using `source_validator.SourceValidator`

CLI examples:
  python herb_collector.py from-url --id rosemary --url https://powo.science.kew.org/taxon/urn:lsid:ipni.org:names:452841-1 --org "Kew Gardens" --title "POWO: Rosmarinus officinalis"
"""
from __future__ import annotations

import argparse
import json
import logging
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Optional
from urllib.request import Request, urlopen

# Local imports
from source_validator import SourceValidator, ValidationResult  # type: ignore

ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "data"
REFS_DIR = ROOT / "references"
CONFIG_PATH = ROOT / "config.json"


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


logger = get_logger("herb_collector")


def ensure_dirs() -> None:
    for d in (DATA_DIR, REFS_DIR):
        d.mkdir(parents=True, exist_ok=True)


def slugify(text: str) -> str:
    import re
    text = text.lower().strip()
    text = re.sub(r"[^a-z0-9\-\s]", "", text)
    text = re.sub(r"\s+", "-", text)
    text = re.sub(r"-+", "-", text)
    return text


def http_get(url: str, timeout: int = 20) -> str:
    req = Request(url, headers={"User-Agent": "HerbiverseBot/1.0"})
    with urlopen(req, timeout=timeout) as resp:
        charset = resp.headers.get_content_charset() or "utf-8"
        return resp.read().decode(charset, errors="replace")


def save_json(path: Path, data: Dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)


def add_reference(refs_path: Path, ref: Dict) -> None:
    refs: Dict[str, Dict] = {}
    if refs_path.exists():
        try:
            with open(refs_path, "r", encoding="utf-8") as f:
                refs = json.load(f)
        except Exception:
            refs = {}
    refs[str(len(refs) + 1)] = ref
    save_json(refs_path, refs)


def collect_from_url(herb_id: str, url: str, org: Optional[str], title: Optional[str]) -> None:
    ensure_dirs()

    logger.info("Fetching: %s", url)
    try:
        html = http_get(url)
    except Exception as e:
        logger.error("HTTP GET failed: %s", e)
        raise SystemExit(2)

    fetched_at = datetime.now(timezone.utc).isoformat()

    raw_record = {
        "id": herb_id,
        "source_url": url,
        "org": org,
        "title": title,
        "fetched_at": fetched_at,
        "raw_html": html,
    }

    raw_path = DATA_DIR / f"{herb_id}.raw.json"
    save_json(raw_path, raw_record)
    logger.info("Saved raw snapshot: %s", raw_path)

    # Reference management
    ref_entry = {
        "title": title or "",
        "org": org or "",
        "year": datetime.now().year,
        "url": url,
    }

    # Validate reference
    validator = SourceValidator(CONFIG_PATH)
    res: ValidationResult = validator.validate_reference(ref_entry)
    if res.is_valid:
        logger.info("Reference trusted")
    else:
        logger.warning("Reference NOT trusted: %s", res.reason or "unknown reason")

    refs_path = REFS_DIR / f"{herb_id}.refs.json"
    add_reference(refs_path, ref_entry)
    logger.info("Updated references: %s", refs_path)


def main() -> None:
    parser = argparse.ArgumentParser(description="Herbiverse collector")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_url = sub.add_parser("from-url", help="Collect raw data from a URL")
    p_url.add_argument("--id", required=True, help="Herb identifier (lowercase, hyphenated)")
    p_url.add_argument("--url", required=True, help="Source URL")
    p_url.add_argument("--org", required=False, help="Source organization name")
    p_url.add_argument("--title", required=False, help="Reference title")

    args = parser.parse_args()

    if args.cmd == "from-url":
        herb_id = slugify(args.id)
        if herb_id != args.id:
            logger.info("Normalized id to: %s", herb_id)
        collect_from_url(herb_id, args.url, args.org, args.title)


if __name__ == "__main__":
    main()
