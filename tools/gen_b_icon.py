from PIL import Image, ImageDraw, ImageFont
import os

out_dir = r"c:\Users\User\Documents\RobloxEngine\assets\icon"
os.makedirs(out_dir, exist_ok=True)

def make_icon(size):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    margin = size // 12
    d.rounded_rectangle(
        [margin + size // 40, margin + size // 40, size - margin, size - margin],
        radius=size // 8,
        fill=(20, 40, 80, 180),
    )
    d.rounded_rectangle(
        [margin, margin, size - margin - size // 40, size - margin - size // 40],
        radius=size // 8,
        fill=(45, 130, 230, 255),
    )
    d.rounded_rectangle(
        [margin + size // 10, margin + size // 10, size - margin - size // 8, margin + size // 3],
        radius=size // 16,
        fill=(90, 170, 255, 220),
    )
    try:
        font = ImageFont.truetype(r"C:\Windows\Fonts\arialbd.ttf", size=int(size * 0.55))
    except Exception:
        font = ImageFont.load_default()
    text = "B"
    bbox = d.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    x = (size - tw) // 2 - bbox[0]
    y = (size - th) // 2 - bbox[1] - size // 40
    d.text((x + size // 50, y + size // 50), text, font=font, fill=(15, 40, 90, 255))
    d.text((x, y), text, font=font, fill=(255, 255, 255, 255))
    return img

png = make_icon(256)
png_path = os.path.join(out_dir, "b_icon.png")
png.save(png_path, "PNG")
print("Wrote", png_path)

sizes = [16, 32, 48, 64, 128, 256]
icons = [make_icon(s) for s in sizes]
ico_path = os.path.join(out_dir, "app.ico")
icons[-1].save(ico_path, format="ICO", sizes=[(s, s) for s in sizes])
print("Wrote", ico_path)
