#!/usr/bin/env python3
"""
Batch fetch images for multiple herbs from Wikimedia Commons.

Usage:
  python batch_fetch_images.py [--download] [--limit 5]
"""
import json
import subprocess
import sys
from pathlib import Path
from typing import Dict, List

ROOT = Path(__file__).resolve().parents[1]
MARKDOWN_DIR = ROOT / "markdown"

# Mapping of herb IDs to scientific names for searching
HERB_QUERIES = {
    "basil": "Ocimum basilicum",
    "bay_leaf": "Laurus nobilis bay leaf",
    "belladonna": "Atropa belladonna",
    "castor_bean": "Ricinus communis",
    "chervil": "Anthriscus cerefolium",
    "chives": "Allium schoenoprasum",
    "cilantro": "Coriandrum sativum cilantro",
    "comfrey": "Symphytum officinale",
    "culantro": "Eryngium foetidum",
    "datura": "Datura stramonium",
    "dill": "Anethum graveolens",
    "foxglove": "Digitalis purpurea",
    "kaffir_lime_leaf": "Citrus hystrix kaffir",
    "lemon_balm": "Melissa officinalis",
    "lemongrass": "Cymbopogon citratus",
    "lily_of_the_valley": "Convallaria majalis",
    "marjoram": "Origanum majorana",
    "mint": "Mentha spicata peppermint",
    "oleander": "Nerium oleander",
    "oregano": "Origanum vulgare",
    "parsley": "Petroselinum crispum",
    "poison_hemlock": "Conium maculatum",
    "rosemary": "Rosmarinus officinalis",
    "rue": "Ruta graveolens",
    "sage": "Salvia officinalis",
    "shiso": "Perilla frutescens shiso",
    "tansy": "Tanacetum vulgare",
    "tarragon": "Artemisia dracunculus",
    "thyme": "Thymus vulgaris",
    "vietnamese_coriander": "Persicaria odorata",
    "water_hemlock": "Cicuta maculata",
    "wormwood": "Artemisia absinthium"
}


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Batch fetch images for herbs")
    parser.add_argument("--download", action="store_true", help="Download images")
    parser.add_argument("--limit", type=int, default=5, help="Images per herb")
    parser.add_argument("--herbs", nargs="+", help="Specific herbs to process")
    args = parser.parse_args()
    
    # Determine which herbs to process
    if args.herbs:
        herbs_to_process = {k: v for k, v in HERB_QUERIES.items() if k in args.herbs}
    else:
        # Process all herbs
        herbs_to_process = HERB_QUERIES
    
    print(f"Processing {len(herbs_to_process)} herbs...")
    print("=" * 60)
    
    success_count = 0
    failed = []
    
    for herb_id, query in herbs_to_process.items():
        print(f"\n[{herb_id}] Fetching images...")
        
        cmd = [
            sys.executable,
            str(ROOT / "scripts" / "image_fetcher.py"),
            "--id", herb_id,
            "--query", query,
            "--limit", str(args.limit)
        ]
        
        if args.download:
            cmd.append("--download")
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            if result.returncode == 0:
                print(f"[{herb_id}] ✓ Success")
                success_count += 1
            else:
                print(f"[{herb_id}] ✗ Failed: {result.stderr}")
                failed.append(herb_id)
        except subprocess.TimeoutExpired:
            print(f"[{herb_id}] ✗ Timeout")
            failed.append(herb_id)
        except Exception as e:
            print(f"[{herb_id}] ✗ Error: {e}")
            failed.append(herb_id)
    
    print("\n" + "=" * 60)
    print(f"Summary: {success_count}/{len(herbs_to_process)} succeeded")
    if failed:
        print(f"Failed herbs: {', '.join(failed)}")
    
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())