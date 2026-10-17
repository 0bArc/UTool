#!/usr/bin/env python3
"""Render Nexus-style mod banners (1300x372 + 1400x400) from catalog metadata."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Pillow required: pip install pillow", file=sys.stderr)
    sys.exit(2)

SIZES = ((1300, 372, "banner_1300x372.png"), (1400, 400, "header_1400x400.png"))
BLACK = (8, 8, 10, 255)
WHITE = (255, 255, 255, 255)
GRAY = (209, 213, 219, 255)  # gray-300 - readable on black
DIM = (156, 163, 175, 255)  # gray-400
ACCENT = (255, 255, 255, 255)


def find_font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = [
        Path(r"C:\Windows\Fonts\segoeui.ttf"),
        Path(r"C:\Windows\Fonts\arial.ttf"),
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
        Path("/System/Library/Fonts/Supplemental/Arial.ttf"),
    ]
    for path in candidates:
        if path.is_file():
            return ImageFont.truetype(str(path), size=size)
    return ImageFont.load_default()


def find_font_bold(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = [
        Path(r"C:\Windows\Fonts\segoeuib.ttf"),
        Path(r"C:\Windows\Fonts\arialbd.ttf"),
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"),
        Path("/System/Library/Fonts/Supplemental/Arial Bold.ttf"),
    ]
    for path in candidates:
        if path.is_file():
            return ImageFont.truetype(str(path), size=size)
    return find_font(size)


def parse_mod_lua(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8", errors="replace")
    out: dict[str, str] = {}
    for key in ("name", "description", "author", "id"):
        m = re.search(rf'{key}\s*=\s*"([^"]*)"', text)
        if m:
            out[key] = m.group(1)
    return out


def clean_tagline(text: str) -> str:
    # No em/en dashes - they look bad and break some Nexus fonts.
    text = text.replace("\u2014", " - ").replace("\u2013", " - ")
    text = re.sub(r"\s+-\s+", " - ", text)
    text = re.sub(r"\s{2,}", " ", text).strip()
    return text


def wrap_text(draw: ImageDraw.ImageDraw, text: str, font, max_width: int) -> list[str]:
    words = text.split()
    if not words:
        return []
    lines: list[str] = []
    cur = words[0]
    for word in words[1:]:
        trial = f"{cur} {word}"
        if draw.textlength(trial, font=font) <= max_width:
            cur = trial
        else:
            lines.append(cur)
            cur = word
    lines.append(cur)
    return lines[:2]


def render_banner(
    *,
    title: str,
    tagline: str,
    category: str,
    author: str,
    out_dir: Path,
) -> list[Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    tagline = clean_tagline(tagline)
    written: list[Path] = []
    for w, h, filename in SIZES:
        # Solid dark canvas only. No grid / no alpha wash (RGBA wash became white).
        img = Image.new("RGB", (w, h), (8, 8, 10))
        draw = ImageDraw.Draw(img)

        pad_x = int(w * 0.055)
        pad_y = int(h * 0.16)
        eyebrow_font = find_font(max(15, h // 20))
        title_font = find_font_bold(max(38, h // 7))
        body_font = find_font(max(17, h // 17))
        meta_font = find_font(max(14, h // 22))

        eyebrow = f"ICARUS  /  {category.upper()}" if category else "ICARUS"
        draw.text((pad_x, pad_y), eyebrow, font=eyebrow_font, fill=(156, 163, 175))
        author_text = author or "xynv4"
        aw = draw.textlength(author_text, font=meta_font)
        draw.text((w - pad_x - aw, pad_y), author_text, font=meta_font, fill=(156, 163, 175))

        title_y = pad_y + int(h * 0.22)
        draw.text((pad_x, title_y), title, font=title_font, fill=(255, 255, 255))

        if tagline:
            body_y = title_y + int(h * 0.28)
            for i, line in enumerate(wrap_text(draw, tagline, body_font, w - 2 * pad_x)):
                draw.text(
                    (pad_x, body_y + i * int(h * 0.12)),
                    line,
                    font=body_font,
                    fill=(209, 213, 219),
                )

        rule_y = h - int(h * 0.08)
        draw.rectangle([pad_x, rule_y, pad_x + int(w * 0.08), rule_y + 2], fill=(255, 255, 255))

        path = out_dir / filename
        img.save(path, "PNG", optimize=True)
        written.append(path)
    return written


def load_catalog(mods_dir: Path) -> list[dict]:
    cfg_path = mods_dir / "utool.json"
    if not cfg_path.is_file():
        return []
    data = json.loads(cfg_path.read_text(encoding="utf-8"))
    return list(data.get("catalog") or [])


def resolve_entries(mods_dir: Path, want_id: str | None, all_ship: bool) -> list[tuple[dict, Path, dict]]:
    catalog = load_catalog(mods_dir)
    out = []
    for entry in catalog:
        if want_id and entry.get("id") != want_id:
            continue
        if all_ship and not entry.get("ship", True):
            continue
        if want_id is None and not all_ship and not entry.get("ship", True):
            continue
        root = mods_dir / entry["path"]
        lua = root / "mod.lua"
        if not lua.is_file():
            print(f"skip {entry.get('id')}: missing mod.lua", file=sys.stderr)
            continue
        meta = parse_mod_lua(lua)
        out.append((entry, root, meta))
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description="Render utool mod Nexus banners")
    parser.add_argument("--mods-dir", default="examples", type=Path)
    parser.add_argument("--id", help="Catalog id to render")
    parser.add_argument("--all", action="store_true", help="Render all ship:true catalog entries")
    args = parser.parse_args()

    mods_dir = args.mods_dir.resolve()
    if not args.id and not args.all:
        print("Specify --id <catalog-id> or --all", file=sys.stderr)
        return 1

    entries = resolve_entries(mods_dir, args.id, args.all)
    if not entries:
        print("No matching catalog entries", file=sys.stderr)
        return 1

    for entry, root, meta in entries:
        title = entry.get("bannerTitle") or meta.get("name") or entry["id"]
        tagline = entry.get("bannerTagline") or meta.get("description") or ""
        category = entry.get("category") or ""
        author = meta.get("author") or "xynv4"
        paths = render_banner(
            title=title,
            tagline=tagline,
            category=category,
            author=author,
            out_dir=root / "media",
        )
        for p in paths:
            print(p)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
