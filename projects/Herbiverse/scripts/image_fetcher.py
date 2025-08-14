#!/usr/bin/env python3
"""
Image fetcher for Herbiverse using Wikimedia Commons API.

- Searches images with acceptable licenses from config.json
- Optionally downloads images to `../images/{id}/`
- Saves a metadata JSON alongside downloads

CLI examples:
  python image_fetcher.py --id rosemary --query "Rosmarinus officinalis" --limit 5 --download
"""
from __future__ import annotations

import argparse
import json
import logging
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple
from urllib.parse import urlencode
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
IMAGES_DIR = ROOT / "images"
CONFIG_PATH = ROOT / "config.json"

COMMONS_API = "https://commons.wikimedia.org/w/api.php"


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


logger = get_logger("image_fetcher")


def load_config() -> Dict[str, Any]:
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}


def http_get_json(url: str, params: Dict[str, Any]) -> Dict:
    qs = urlencode(params)
    req = Request(f"{url}?{qs}", headers={"User-Agent": "HerbiverseBot/1.0"})
    with urlopen(req, timeout=30) as resp:
        return json.loads(resp.read().decode("utf-8", errors="replace"))


def search_commons(query: str, limit: int = 10) -> List[Dict]:
    # Step 1: search for files (namespace 6)
    res = http_get_json(COMMONS_API, {
        "action": "query",
        "format": "json",
        "list": "search",
        "srsearch": query,
        "srnamespace": 6,
        "srlimit": limit,
    })
    pages = res.get("query", {}).get("search", [])
    ids = [p.get("pageid") for p in pages if p.get("pageid")]
    if not ids:
        return []

    # Step 2: fetch imageinfo for those pages
    res2 = http_get_json(COMMONS_API, {
        "action": "query",
        "format": "json",
        "pageids": "|".join(str(i) for i in ids),
        "prop": "imageinfo",
        "iiprop": "url|extmetadata|size",
    })
    result: List[Dict] = []
    pages2 = res2.get("query", {}).get("pages", {})
    for _, page in pages2.items():
        title = page.get("title")
        infos = page.get("imageinfo") or []
        if not infos:
            continue
        info = infos[0]
        ext = info.get("extmetadata") or {}
        result.append({
            "title": title,
            "image_url": info.get("url"),
            "width": info.get("width"),
            "height": info.get("height"),
            "license": (ext.get("LicenseShortName", {}) or {}).get("value"),
            "author": (ext.get("Artist", {}) or {}).get("value"),
            "credit": (ext.get("Credit", {}) or {}).get("value"),
            "source": "Wikimedia Commons",
            "page_url": f"https://commons.wikimedia.org/wiki/{title}" if title else "",
        })
    return result


def license_ok(license_name: Optional[str], allowed: List[str]) -> bool:
    if not license_name:
        return False
    name = license_name.lower()
    return any(a.lower() in name for a in allowed)


def filter_by_config(images: List[Dict], config: Dict[str, Any]) -> List[Dict]:
    req = config.get("imageRequirements", {})
    allowed = req.get("licenses", [])
    min_res = req.get("minResolution", "800x600")
    try:
        min_w, min_h = [int(x) for x in min_res.lower().split("x")]
    except Exception:
        min_w, min_h = 800, 600

    filtered = []
    for img in images:
        if not license_ok(img.get("license"), allowed):
            continue
        w = int(img.get("width") or 0)
        h = int(img.get("height") or 0)
        if w >= min_w and h >= min_h:
            filtered.append(img)
    return filtered


def download_image(url: str, out_path: Path) -> None:
    req = Request(url, headers={"User-Agent": "HerbiverseBot/1.0"})
    with urlopen(req, timeout=60) as resp:
        data = resp.read()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(data)


def save_metadata(herb_id: str, images: List[Dict]) -> Path:
    meta_path = IMAGES_DIR / herb_id / "metadata.json"
    meta_path.parent.mkdir(parents=True, exist_ok=True)
    with open(meta_path, "w", encoding="utf-8") as f:
        json.dump(images, f, ensure_ascii=False, indent=2)
    return meta_path


def main() -> None:
    parser = argparse.ArgumentParser(description="Fetch images for a herb from Wikimedia Commons")
    parser.add_argument("--id", required=True, help="Herb identifier")
    parser.add_argument("--query", required=True, help="Search query")
    parser.add_argument("--limit", type=int, default=10, help="Search limit")
    parser.add_argument("--download", action="store_true", help="Download images as files")
    args = parser.parse_args()

    config = load_config()
    logger.info("Searching Wikimedia Commons: query=\"%s\"", args.query)

    candidates = search_commons(args.query, limit=args.limit)
    if not candidates:
        logger.warning("No images found")
        return

    filtered = filter_by_config(candidates, config)
    logger.info("Found %d candidates; %d passed filters", len(candidates), len(filtered))

    if args.download:
        for i, img in enumerate(filtered, start=1):
            url = img.get("image_url")
            if not url:
                continue
            ext = Path(url).suffix or ".jpg"
            out = IMAGES_DIR / args.id / f"{i:02d}{ext}"
            try:
                download_image(url, out)
                logger.info("Downloaded: %s", out)
            except Exception as e:
                logger.error("Failed to download %s: %s", url, e)

    meta_path = save_metadata(args.id, filtered)
    logger.info("Saved metadata: %s", meta_path)


if __name__ == "__main__":
    main()
