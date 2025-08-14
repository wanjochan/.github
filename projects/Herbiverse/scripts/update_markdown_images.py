#!/usr/bin/env python3
"""
Update markdown files to use local images if available, otherwise keep online URLs.

Usage:
  python update_markdown_images.py [--herb HERB_ID]
"""
import json
import re
from pathlib import Path
from typing import Dict, List, Optional

ROOT = Path(__file__).resolve().parents[1]
MARKDOWN_DIR = ROOT / "markdown"
IMAGES_DIR = ROOT / "images"


def load_image_metadata(herb_id: str) -> Optional[List[Dict]]:
    """Load metadata.json for a herb if it exists."""
    meta_path = IMAGES_DIR / herb_id / "metadata.json"
    if not meta_path.exists():
        return None
    
    with open(meta_path, "r", encoding="utf-8") as f:
        return json.load(f)


def get_local_images(herb_id: str) -> List[Path]:
    """Get list of downloaded local images for a herb."""
    herb_dir = IMAGES_DIR / herb_id
    if not herb_dir.exists():
        return []
    
    images = []
    for ext in [".jpg", ".jpeg", ".png", ".gif", ".webp"]:
        images.extend(herb_dir.glob(f"*{ext}"))
    
    # Sort by filename to maintain order
    return sorted(images)


def format_image_markdown(
    image_path: Optional[Path], 
    metadata: Optional[Dict], 
    index: int,
    use_local: bool = True
) -> str:
    """Format an image reference for markdown."""
    if use_local and image_path:
        # Use relative path from markdown to images
        rel_path = f"../images/{image_path.parent.name}/{image_path.name}"
        alt_text = f"Image {index}"
        
        if metadata:
            # Extract clean alt text from metadata
            license_info = metadata.get("license", "Unknown")
            source = metadata.get("source", "Unknown")
            alt_text = f"{image_path.parent.name.replace('_', ' ').title()} - {source} ({license_info})"
        
        return f"![{alt_text}]({rel_path})"
    
    elif metadata:
        # Use online URL from metadata
        url = metadata.get("image_url", "")
        license_info = metadata.get("license", "Unknown")
        source = metadata.get("source", "Unknown")
        alt_text = f"{metadata.get('title', 'Image').replace('File:', '').replace('.jpg', '')} - {source} ({license_info})"
        
        return f"![{alt_text}]({url})"
    
    return ""


def update_herb_markdown(herb_id: str, dry_run: bool = False) -> bool:
    """Update a single herb markdown file with image references."""
    md_path = MARKDOWN_DIR / f"{herb_id}.md"
    if not md_path.exists():
        print(f"Warning: {md_path} not found")
        return False
    
    # Load current content
    with open(md_path, "r", encoding="utf-8") as f:
        content = f.read()
    
    # Check if images already exist in markdown
    existing_images = re.findall(r'!\[.*?\]\(.*?\)', content)
    if existing_images:
        print(f"[{herb_id}] Already has {len(existing_images)} images, skipping")
        return False
    
    # Get local images and metadata
    local_images = get_local_images(herb_id)
    metadata = load_image_metadata(herb_id)
    
    if not local_images and not metadata:
        print(f"[{herb_id}] No images available")
        return False
    
    # Prepare image markdown
    image_lines = []
    
    if local_images:
        # Use local images
        print(f"[{herb_id}] Using {len(local_images)} local images")
        for i, img_path in enumerate(local_images, 1):
            # Try to match with metadata by index
            meta_item = metadata[i-1] if metadata and i <= len(metadata) else None
            image_md = format_image_markdown(img_path, meta_item, i, use_local=True)
            if image_md:
                image_lines.append(image_md)
    
    elif metadata:
        # Use online URLs from metadata
        print(f"[{herb_id}] Using {len(metadata)} online images from metadata")
        for i, meta_item in enumerate(metadata, 1):
            image_md = format_image_markdown(None, meta_item, i, use_local=False)
            if image_md:
                image_lines.append(image_md)
    
    if not image_lines:
        return False
    
    # Insert images after the header
    lines = content.split('\n')
    insert_index = 0
    
    # Find the first non-header, non-empty line
    for i, line in enumerate(lines):
        if line.strip() and not line.startswith('#'):
            insert_index = i
            break
    
    # Insert images
    new_lines = lines[:insert_index] + [''] + image_lines + [''] + lines[insert_index:]
    new_content = '\n'.join(new_lines)
    
    if dry_run:
        print(f"[{herb_id}] Would add {len(image_lines)} images")
        return True
    
    # Write updated content
    with open(md_path, "w", encoding="utf-8") as f:
        f.write(new_content)
    
    print(f"[{herb_id}] ✓ Added {len(image_lines)} images")
    return True


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Update markdown with image references")
    parser.add_argument("--herb", help="Specific herb to update")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes without writing")
    args = parser.parse_args()
    
    if args.herb:
        success = update_herb_markdown(args.herb, dry_run=args.dry_run)
        return 0 if success else 1
    
    # Process all herbs with available images
    herbs = set()
    
    # Get herbs with local images
    if IMAGES_DIR.exists():
        for herb_dir in IMAGES_DIR.iterdir():
            if herb_dir.is_dir():
                herbs.add(herb_dir.name)
    
    print(f"Found {len(herbs)} herbs with images")
    
    updated = 0
    for herb_id in sorted(herbs):
        if update_herb_markdown(herb_id, dry_run=args.dry_run):
            updated += 1
    
    print(f"\nUpdated {updated}/{len(herbs)} markdown files")
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())