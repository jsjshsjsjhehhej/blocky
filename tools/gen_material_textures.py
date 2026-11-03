"""Generate seamless RGB albedo PNGs for PartMaterial (no background / alpha)."""
from __future__ import annotations

import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "textures", "materials")
OUT = os.path.normpath(OUT)
N = 256


def save(name: str, im: Image.Image) -> None:
    if im.mode != "RGB":
        im = im.convert("RGB")
    path = os.path.join(OUT, name + ".png")
    im.save(path, "PNG", optimize=True)
    print(name, im.size, os.path.getsize(path))


def clamp(v: float) -> int:
    return max(0, min(255, int(v)))


def noise2(x: float, y: float, seed: float = 0.0) -> float:
    n = math.sin(x * 12.9898 + y * 78.233 + seed * 45.164) * 43758.5453
    return n - math.floor(n)


def fbm(x: float, y: float, octaves: int = 5, seed: float = 0.0) -> float:
    amp, freq, v, n = 1.0, 1.0, 0.0, 0.0
    for i in range(octaves):
        v += amp * noise2(x * freq, y * freq, seed + i * 17)
        n += amp
        amp *= 0.5
        freq *= 2.0
    return v / n


def gen_wood() -> Image.Image:
    px = bytearray(N * N * 3)
    for j in range(N):
        for i in range(N):
            ang = (i / N) * 2 * math.pi
            cx = math.cos(ang) * 0.35 + (j / N) * 3.0
            rings = math.sin(cx * 18.0 + fbm(i / N * 4, j / N * 4, 3, 1) * 2.5)
            grain = fbm(i / N * 12, j / N * 2, 4, 7)
            t = 0.55 + 0.25 * rings + 0.15 * grain
            r = clamp(90 + t * 110)
            g = clamp(55 + t * 70)
            b = clamp(28 + t * 35)
            o = (j * N + i) * 3
            px[o : o + 3] = bytes((r, g, b))
    return Image.frombytes("RGB", (N, N), bytes(px))


def gen_concrete() -> Image.Image:
    px = bytearray(N * N * 3)
    for j in range(N):
        for i in range(N):
            n = fbm(i / N * 8, j / N * 8, 6, 3)
            speck = noise2(i * 0.7, j * 0.7, 9)
            t = 0.45 + 0.35 * n + 0.08 * speck
            c = clamp(120 + t * 90)
            o = (j * N + i) * 3
            px[o : o + 3] = bytes((c, c, clamp(c - 4)))
    return Image.frombytes("RGB", (N, N), bytes(px))


def gen_grass() -> Image.Image:
    px = bytearray(N * N * 3)
    for j in range(N):
        for i in range(N):
            n = fbm(i / N * 10, j / N * 10, 5, 11)
            blade = abs(math.sin((i + n * 8) * 0.35)) * 0.25
            t = 0.4 + 0.4 * n + blade
            r = clamp(30 + t * 50)
            g = clamp(90 + t * 110)
            b = clamp(20 + t * 40)
            o = (j * N + i) * 3
            px[o : o + 3] = bytes((r, g, b))
    return Image.frombytes("RGB", (N, N), bytes(px))


def gen_brick() -> Image.Image:
    im = Image.new("RGB", (N, N), (70, 70, 72))
    bw, bh = N // 4, N // 8
    mortar = 2
    for row in range(0, N, bh):
        off = (bw // 2) if ((row // bh) % 2) else 0
        for col in range(-bw, N + bw, bw):
            x0 = (col + off) % N
            for dx in range(mortar, bw - mortar):
                for dy in range(mortar, bh - mortar):
                    x = (x0 + dx) % N
                    y = (row + dy) % N
                    var = noise2(x * 0.2, y * 0.2, 5)
                    r = clamp(140 + var * 40 + ((x + y) % 7) * 2)
                    g = clamp(70 + var * 25)
                    b = clamp(55 + var * 20)
                    im.putpixel((x, y), (r, g, b))
    return im.filter(ImageFilter.SMOOTH)


def gen_fabric() -> Image.Image:
    px = bytearray(N * N * 3)
    for j in range(N):
        for i in range(N):
            weave = (math.sin(i * 0.8) * math.sin(j * 0.8)) * 0.5 + 0.5
            n = fbm(i / N * 6, j / N * 6, 3, 21)
            t = 0.5 * weave + 0.5 * n
            c = clamp(150 + t * 70 - 20)
            o = (j * N + i) * 3
            px[o : o + 3] = bytes((c, clamp(c - 5), clamp(c + 5)))
    return Image.frombytes("RGB", (N, N), bytes(px))


def gen_marble() -> Image.Image:
    px = bytearray(N * N * 3)
    for j in range(N):
        for i in range(N):
            x, y = i / N, j / N
            v = math.sin((x * 6 + fbm(x * 3, y * 3, 4, 2) * 4) * math.pi)
            vein = abs(v)
            t = 0.85 - 0.55 * (1.0 - vein) ** 3
            c = clamp(200 + t * 55)
            o = (j * N + i) * 3
            px[o : o + 3] = bytes((c, clamp(c - 2), clamp(c - 4)))
    return Image.frombytes("RGB", (N, N), bytes(px))


def gen_sand() -> Image.Image:
    px = bytearray(N * N * 3)
    for j in range(N):
        for i in range(N):
            n = fbm(i / N * 14, j / N * 14, 5, 33)
            speck = noise2(i * 1.3, j * 1.1, 4)
            t = 0.55 + 0.3 * n + 0.1 * speck
            r = clamp(190 + t * 50)
            g = clamp(160 + t * 40)
            b = clamp(100 + t * 30)
            o = (j * N + i) * 3
            px[o : o + 3] = bytes((r, g, b))
    return Image.frombytes("RGB", (N, N), bytes(px))


def gen_slate() -> Image.Image:
    px = bytearray(N * N * 3)
    for j in range(N):
        for i in range(N):
            layers = math.sin(j / N * 40 * math.pi + fbm(i / N * 2, j / N * 2, 3, 8) * 3)
            n = fbm(i / N * 7, j / N * 7, 4, 12)
            t = 0.4 + 0.25 * layers + 0.25 * n
            c = clamp(55 + t * 70)
            o = (j * N + i) * 3
            px[o : o + 3] = bytes((c, clamp(c + 4), clamp(c + 8)))
    return Image.frombytes("RGB", (N, N), bytes(px))


def gen_diamondplate() -> Image.Image:
    im = Image.new("RGB", (N, N), (145, 148, 152))
    cell = N // 8
    for by in range(0, N, cell):
        for bx in range(0, N, cell):
            cx, cy = bx + cell // 2, by + cell // 2
            for j in range(by, by + cell):
                for i in range(bx, bx + cell):
                    dx, dy = i - cx, j - cy
                    m = abs(dx) + abs(dy)
                    r = cell * 0.38
                    if m < r:
                        h = 1.0 - m / r
                        shade = 20 * (dx + dy) / cell
                        c = clamp(160 + h * 50 + shade)
                        im.putpixel((i % N, j % N), (c, c, clamp(c + 2)))
                    elif m < r + 2:
                        im.putpixel((i % N, j % N), (110, 110, 110))
    return im


def gen_corrodedmetal() -> Image.Image:
    px = bytearray(N * N * 3)
    for j in range(N):
        for i in range(N):
            n = fbm(i / N * 9, j / N * 9, 6, 44)
            rust = fbm(i / N * 5, j / N * 5, 4, 55)
            metal = 0.45 + 0.35 * n
            if rust > 0.55:
                t = (rust - 0.55) / 0.45
                r = clamp(90 + t * 100 + metal * 40)
                g = clamp(45 + t * 40 + metal * 20)
                b = clamp(25 + metal * 20)
            else:
                c = clamp(70 + metal * 90)
                r, g, b = c, clamp(c + 5), clamp(c + 10)
            o = (j * N + i) * 3
            px[o : o + 3] = bytes((r, g, b))
    return Image.frombytes("RGB", (N, N), bytes(px))


def gen_granite() -> Image.Image:
    px = bytearray(N * N * 3)
    for j in range(N):
        for i in range(N):
            n = fbm(i / N * 12, j / N * 12, 5, 66)
            speck = 1.0 if noise2(i * 2.1, j * 2.3, 3) > 0.92 else 0.0
            base = 90 + n * 80
            if speck:
                base = 220
            c = clamp(base)
            o = (j * N + i) * 3
            px[o : o + 3] = bytes((c, clamp(c - 5), clamp(c - 10)))
    return Image.frombytes("RGB", (N, N), bytes(px))


def gen_asphalt() -> Image.Image:
    px = bytearray(N * N * 3)
    for j in range(N):
        for i in range(N):
            n = fbm(i / N * 16, j / N * 16, 6, 77)
            speck = noise2(i * 3.0, j * 3.0, 2)
            t = 0.25 + 0.35 * n + 0.08 * speck
            c = clamp(35 + t * 55)
            o = (j * N + i) * 3
            px[o : o + 3] = bytes((c, c, clamp(c + 2)))
    return Image.frombytes("RGB", (N, N), bytes(px))


def gen_metal() -> Image.Image:
    px = bytearray(N * N * 3)
    for j in range(N):
        for i in range(N):
            brush = fbm(i / N * 20, j / N * 2, 4, 88)
            n = fbm(i / N * 4, j / N * 4, 3, 89)
            t = 0.55 + 0.3 * brush + 0.1 * n
            c = clamp(140 + t * 70)
            o = (j * N + i) * 3
            px[o : o + 3] = bytes((c, c, clamp(c + 4)))
    return Image.frombytes("RGB", (N, N), bytes(px))


def main() -> None:
    os.makedirs(OUT, exist_ok=True)
    gens = {
        "wood": gen_wood,
        "concrete": gen_concrete,
        "grass": gen_grass,
        "brick": gen_brick,
        "fabric": gen_fabric,
        "marble": gen_marble,
        "sand": gen_sand,
        "slate": gen_slate,
        "diamondplate": gen_diamondplate,
        "corrodedmetal": gen_corrodedmetal,
        "granite": gen_granite,
        "asphalt": gen_asphalt,
        "metal": gen_metal,
    }
    for name, fn in gens.items():
        save(name, fn())
    jpg = os.path.join(OUT, "wood.jpg")
    if os.path.exists(jpg):
        os.remove(jpg)
        print("removed wood.jpg")
    print("done ->", OUT)


if __name__ == "__main__":
    main()
