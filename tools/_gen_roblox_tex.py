# Genera texture materiale e stud, stile Roblox: grigio da moltiplicare col colore.
import math
import struct
import zlib
from pathlib import Path

ROOT = Path(r"C:\Users\User\Documents\RobloxEngine\assets\textures")
MAT = ROOT / "materials"
N = 256


def save_png(path: Path, rgb: bytearray, n: int) -> None:
    raw = bytearray()
    stride = n * 3
    for y in range(n):
        raw.append(0)
        raw.extend(rgb[y * stride:(y + 1) * stride])

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", n, n, 8, 2, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    path.write_bytes(png)


def hn(x: int, y: int) -> float:
    n = (x * 374761393 + y * 668265263) & 0xFFFFFFFF
    n = (n ^ (n >> 13)) * 1274126177 & 0xFFFFFFFF
    return (n & 255) / 255.0


def vnoise(x: float, y: float) -> float:
    x %= N
    y %= N
    x0 = int(math.floor(x))
    y0 = int(math.floor(y))
    fx = x - x0
    fy = y - y0
    fx = fx * fx * (3 - 2 * fx)
    fy = fy * fy * (3 - 2 * fy)

    def h(ix, iy):
        return hn(ix % N, iy % N)

    a = h(x0, y0) * (1 - fx) + h(x0 + 1, y0) * fx
    b = h(x0, y0 + 1) * (1 - fx) + h(x0 + 1, y0 + 1) * fx
    return a * (1 - fy) + b * fy


def put(buf, x, y, g, tint=(1.0, 0.985, 0.96)):
    g = max(0, min(255, int(g)))
    i = (y * N + x) * 3
    buf[i] = max(0, min(255, int(g * tint[0])))
    buf[i + 1] = max(0, min(255, int(g * tint[1])))
    buf[i + 2] = max(0, min(255, int(g * tint[2])))


def blank():
    return bytearray(N * N * 3)


def brick():
    b = blank()
    bw, bh, m = 64, 32, 5
    for y in range(N):
        row = y // bh
        yy = y % bh
        shift = bw // 2 if (row & 1) else 0
        for x in range(N):
            xx = (x + shift) % N
            cx = xx % bw
            mortar = yy < m or cx < m
            if mortar:
                g = 72 + int(hn(x // 3, y // 3) * 10)
            else:
                bid = (xx // bw) * 3 + row * 11
                face = 214 + hn(bid, row) * 22
                edge = min(cx - m, bw - cx, yy - m, bh - yy)
                if edge < 4:
                    face *= 0.82 + edge * 0.04
                g = face
            put(b, x, y, g)
    return b


def wood():
    b = blank()
    ph = N // 4
    for y in range(N):
        py = y % ph
        plank = y // ph
        gap = py < 4
        for x in range(N):
            if gap:
                g = 58
            else:
                # venature lunghe, non puntini
                line = 22 if (x + plank * 3) % 9 == 0 else 0
                slow = math.sin(x * 0.11 + plank * 2.0) * 10
                g = 206 + slow - line
                if py < 7:
                    g *= 0.88
            put(b, x, y, g, (1.0, 0.97, 0.90))
    return b


def grass():
    b = blank()
    for y in range(N):
        for x in range(N):
            col = int(x + math.sin(y * 0.04) * 4) % 6
            if col == 0:
                g = 214
            elif col == 1:
                g = 156
            else:
                g = 186 + (hn(x // 3, y // 8) - 0.5) * 16
            put(b, x, y, g, (0.90, 1.0, 0.82))
    return b


def concrete():
    b = blank()
    for y in range(N):
        for x in range(N):
            blot = vnoise(x * 0.55, y * 0.55)
            fine = hn(x, y)
            pit = vnoise(x * 2.2, y * 2.2)
            g = 188 + (blot - 0.5) * 36 + (fine - 0.5) * 22
            if pit > 0.78:
                g *= 0.78
            put(b, x, y, g, (1, 1, 1))
    return b


def metal():
    b = blank()
    for y in range(N):
        for x in range(N):
            brush = 0.5 + 0.5 * math.sin(y * 1.7 + math.sin(x * 0.05) * 3.0)
            fine = hn(x, y // 2)
            g = 176 + brush * 48 + (fine - 0.5) * 14
            put(b, x, y, g, (0.96, 0.98, 1.0))
    return b


def fabric():
    b = blank()
    for y in range(N):
        for x in range(N):
            wx = (x % 8) < 4
            wy = (y % 8) < 4
            thread = 214 if wx == wy else 168
            thread += int((hn(x, y) - 0.5) * 12)
            put(b, x, y, thread, (1, 0.98, 0.96))
    return b


def marble():
    b = blank()
    for y in range(N):
        for x in range(N):
            v = math.sin((x + y) * 0.045 + math.sin(y * 0.03) * 4.0)
            vein = abs(v)
            soft = vnoise(x * 0.4, y * 0.4)
            g = 228 - vein * 70 + (soft - 0.5) * 12
            put(b, x, y, g, (1, 1, 1))
    return b


def sand():
    b = blank()
    for y in range(N):
        for x in range(N):
            grain = hn(x, y)
            dune = vnoise(x * 0.35, y * 0.35)
            g = 196 + (grain - 0.5) * 40 + (dune - 0.5) * 22
            put(b, x, y, g, (1.0, 0.97, 0.88))
    return b


def slate():
    b = blank()
    for y in range(N):
        layer = y // 28
        seam = (y % 28) < 2
        for x in range(N):
            if seam:
                g = 70
            else:
                crack = 1.0 if abs(math.sin(x * 0.2 + layer)) > 0.97 else 0.0
                g = 150 + hn(x // 2, layer) * 40
                if crack:
                    g = 90
            put(b, x, y, g, (0.96, 0.98, 1.0))
    return b


def diamondplate():
    b = blank()
    cell = 32
    for y in range(N):
        for x in range(N):
            cx = (x % cell) - cell / 2
            cy = (y % cell) - cell / 2
            # rombo: |x| + |y|
            d = abs(cx) + abs(cy)
            if d < 9:
                # luce in alto a sinistra
                light = (9 - d) / 9
                shade = 150 + light * 90
                if cx + cy < 0:
                    shade += 18
                g = shade
            else:
                g = 132 + hn(x // 2, y // 2) * 8
            put(b, x, y, g, (0.97, 0.98, 1.0))
    return b


def corroded():
    b = blank()
    for y in range(N):
        for x in range(N):
            brush = 0.5 + 0.5 * math.sin(y * 1.3)
            rust = vnoise(x * 0.8, y * 0.8)
            g = 170 + brush * 30
            if rust > 0.62:
                g = 110 + (rust - 0.62) * 80
            put(b, x, y, g, (1.0, 0.92, 0.82))
    return b


def granite():
    b = blank()
    for y in range(N):
        for x in range(N):
            a = hn(x, y)
            blob = vnoise(x * 1.6, y * 1.6)
            if a > 0.72:
                g = 230
            elif blob > 0.6:
                g = 120
            else:
                g = 175
            put(b, x, y, g, (1, 0.99, 0.98))
    return b


def asphalt():
    b = blank()
    for y in range(N):
        for x in range(N):
            stone = hn(x, y)
            base = 78 + vnoise(x * 0.5, y * 0.5) * 18
            if stone > 0.86:
                g = 150 + (stone - 0.86) * 200
            else:
                g = base
            put(b, x, y, g, (1, 1, 1))
    return b


def studs():
    # 4x4 stud in 256 px: uno stud = 1 unita' con kStudTile = 4
    n = 256
    cell = n // 4
    b = bytearray(n * n * 3)
    for y in range(n):
        for x in range(n):
            cx = (x % cell) + 0.5
            cy = (y % cell) + 0.5
            # stud quadrato stondato, centrato, luce da sopra-sinistra
            dx = cx - cell * 0.5
            dy = cy - cell * 0.5
            ax = abs(dx)
            ay = abs(dy)
            # raggio stondato
            inset = 10
            half = cell * 0.5 - inset
            qx = max(ax - half + 8, 0)
            qy = max(ay - half + 8, 0)
            r = math.sqrt(qx * qx + qy * qy)
            inside = ax < half and ay < half or r < 8
            if not inside:
                g = 148
            else:
                # bevel: bordo piu' scuro in basso a destra, chiaro in alto a sinistra
                edge = min(half - ax, half - ay)
                bevel = max(0.0, 1.0 - edge / 7.0)
                light = (-dx - dy) / cell
                g = 214 + light * 36 - bevel * 28
                if dx > 0 and dy > 0 and bevel > 0.2:
                    g -= 18
            g = max(0, min(255, int(g)))
            i = (y * n + x) * 3
            b[i] = b[i + 1] = b[i + 2] = g
    save_png(ROOT / "stud.png", b, n)


def main():
    mats = {
        "brick": brick,
        "wood": wood,
        "grass": grass,
        "concrete": concrete,
        "metal": metal,
        "fabric": fabric,
        "marble": marble,
        "sand": sand,
        "slate": slate,
        "diamondplate": diamondplate,
        "corrodedmetal": corroded,
        "granite": granite,
        "asphalt": asphalt,
    }
    for name, fn in mats.items():
        save_png(MAT / f"{name}.png", fn(), N)
        print(name)
    studs()
    print("stud")


if __name__ == "__main__":
    main()
