"""Generate the app/tray icon raster files from the same geometry as icon.svg."""

from pathlib import Path

from PIL import Image, ImageDraw

OUT_DIR = Path(__file__).resolve().parent
SIZE = 256
SCALE = 4


def make_icon(size=SIZE):
    """Draw the padlock icon at 4x and downsample for smooth edges."""
    px = size * SCALE

    def s(value):
        return round(value * SCALE)

    image = Image.new("RGBA", (px, px), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    # Dark tile + blue square panel
    draw.rectangle([0, 0, px - 1, px - 1], fill="#0b1220")
    draw.rectangle([s(24), s(24), s(232), s(232)], fill="#3b82f6")

    # Shackle
    draw.arc([s(86), s(54), s(170), s(138)], 180, 360, fill="#ffffff", width=s(18))
    draw.line([s(86), s(96), s(86), s(116)], fill="#ffffff", width=s(18))
    draw.line([s(170), s(96), s(170), s(116)], fill="#ffffff", width=s(18))

    # Lock body
    draw.rectangle([s(68), s(108), s(188), s(204)], fill="#ffffff")

    # Keyhole
    draw.ellipse([s(115), s(133), s(141), s(159)], fill="#3b82f6")
    draw.rectangle([s(120), s(150), s(136), s(184)], fill="#3b82f6")

    return image.resize((size, size), Image.LANCZOS)


def main():
    icon = make_icon(SIZE)
    png_path = OUT_DIR / "icon.png"
    ico_path = OUT_DIR / "icon.ico"
    icon.save(png_path)
    icon.save(
        ico_path,
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )
    print(f"Wrote {png_path}")
    print(f"Wrote {ico_path}")


if __name__ == "__main__":
    main()
