#!/usr/bin/env python3
"""
Add local image links to markdown files while preserving existing online images.
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
    for ext in [".jpg", ".JPG", ".jpeg", ".png", ".gif", ".webp"]:
        images.extend(herb_dir.glob(f"*{ext}"))
    
    # Sort by filename to maintain order
    return sorted(images)


def format_local_image_section(herb_id: str) -> str:
    """Create a section with local image gallery."""
    local_images = get_local_images(herb_id)
    metadata = load_image_metadata(herb_id)
    
    if not local_images:
        return ""
    
    lines = []
    lines.append("\n## Local Image Gallery\n")
    lines.append("*Downloaded high-resolution images for offline viewing:*\n")
    
    for i, img_path in enumerate(local_images, 1):
        # Use relative path from markdown to images
        rel_path = f"../images/{herb_id}/{img_path.name}"
        
        # Try to get metadata for this image
        meta_item = None
        if metadata and i <= len(metadata):
            meta_item = metadata[i-1]
        
        # Create alt text
        if meta_item:
            license_info = meta_item.get("license", "Unknown")
            alt_text = f"{herb_id.replace('_', ' ').title()} - Image {i} ({license_info})"
        else:
            alt_text = f"{herb_id.replace('_', ' ').title()} - Image {i}"
        
        # Add image with caption
        lines.append(f"![{alt_text}]({rel_path})")
        
        # Add caption with metadata if available
        if meta_item:
            license_info = meta_item.get("license", "")
            author = meta_item.get("author", "")
            if author:
                # Clean HTML from author
                author = re.sub(r'<[^>]+>', '', author).strip()
            
            if license_info or author:
                caption = f"*Image {i}: "
                if author:
                    caption += f"© {author}"
                if license_info:
                    caption += f" ({license_info})"
                caption += "*"
                lines.append(caption)
        
        lines.append("")  # Empty line between images
    
    return "\n".join(lines)


def update_herb_markdown(herb_id: str) -> bool:
    """Update a single herb markdown file with local image gallery."""
    md_path = MARKDOWN_DIR / f"{herb_id}.md"
    if not md_path.exists():
        print(f"Warning: {md_path} not found")
        return False
    
    # Load current content
    with open(md_path, "r", encoding="utf-8") as f:
        content = f.read()
    
    # Check if local gallery already exists
    if "## Local Image Gallery" in content:
        print(f"[{herb_id}] Already has local image gallery, skipping")
        return False
    
    # Get local images
    local_images = get_local_images(herb_id)
    if not local_images:
        print(f"[{herb_id}] No local images available")
        return False
    
    # Create local image section
    local_section = format_local_image_section(herb_id)
    if not local_section:
        return False
    
    # Add the local gallery section at the end of the file
    # But before references if they exist
    if "## References" in content or "## 参考" in content:
        # Find where references section starts
        ref_match = re.search(r'\n## (References|参考)', content)
        if ref_match:
            insert_pos = ref_match.start()
            new_content = content[:insert_pos] + local_section + content[insert_pos:]
        else:
            new_content = content + local_section
    else:
        # Just append at the end
        new_content = content.rstrip() + "\n" + local_section
    
    # Write updated content
    with open(md_path, "w", encoding="utf-8") as f:
        f.write(new_content)
    
    print(f"[{herb_id}] ✓ Added local image gallery with {len(local_images)} images")
    return True


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Add local image galleries to markdown files")
    parser.add_argument("--herb", help="Specific herb to update")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes without writing")
    args = parser.parse_args()
    
    if args.herb:
        success = update_herb_markdown(args.herb)
        return 0 if success else 1
    
    # Process all herbs with available local images
    herbs = []
    if IMAGES_DIR.exists():
        for herb_dir in IMAGES_DIR.iterdir():
            if herb_dir.is_dir():
                # Check if has actual images (not just metadata)
                images = get_local_images(herb_dir.name)
                if images:
                    herbs.append(herb_dir.name)
    
    print(f"Found {len(herbs)} herbs with local images")
    print("=" * 60)
    
    updated = 0
    for herb_id in sorted(herbs):
        if update_herb_markdown(herb_id):
            updated += 1
    
    print("=" * 60)
    print(f"Updated {updated}/{len(herbs)} markdown files")
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())