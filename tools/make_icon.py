#!/usr/bin/env python3
"""Genera app.ico multi-risoluzione da PNG sorgente."""
from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    Image = None


def png_chunk(tag: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(tag + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)


def write_png_rgba(path: Path, w: int, h: int, rgba: bytes) -> None:
    raw = b""
    stride = w * 4
    for y in range(h):
        raw += b"\x00" + rgba[y * stride : (y + 1) * stride]
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    data = png_chunk(b"IHDR", ihdr) + png_chunk(b"IDAT", zlib.compress(raw, 9)) + png_chunk(b"IEND", b"")
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + data)


def resize_nearest(src: Image.Image, size: int) -> Image.Image:
    return src.resize((size, size), Image.Resampling.LANCZOS)


def build_ico_from_pil(src: Path, out: Path, sizes=(16, 32, 48, 64, 128, 256)) -> None:
    img = Image.open(src).convert("RGBA")
    images = [resize_nearest(img, s) for s in sizes]
    out.parent.mkdir(parents=True, exist_ok=True)
    images[0].save(
        out,
        format="ICO",
        sizes=[(im.width, im.height) for im in images],
        append_images=images[1:],
    )


def build_ico_fallback(src: Path, out: Path) -> None:
    """Fallback minimale se Pillow non e' installato."""
    if Image is None:
        raise SystemExit("Installa Pillow: pip install pillow")
    build_ico_from_pil(src, out)


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    src = root / "assets" / "icon" / "r_icon.png"
    out = root / "assets" / "icon" / "app.ico"
    if len(sys.argv) > 1:
        src = Path(sys.argv[1])
    if not src.exists():
        print(f"Sorgente mancante: {src}", file=sys.stderr)
        return 1
    build_ico_from_pil(src, out)
    print(f"Creato {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
