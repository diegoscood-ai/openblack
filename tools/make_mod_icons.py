#!/usr/bin/env python3
"""Makes the icon.png of the mods that come with openblack (assets/mods/<id>/, mods/examples/...): a rounded tile in
the colour of the mod's category with a short label, 128x128. Original artwork (nothing taken from the game).

usage: python tools/make_mod_icons.py            (from the repository root; needs Pillow)
A mod that has an icon.png of its own made by hand is never overwritten unless --force is given.
"""

import json
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFont

SIZE = 128
# category -> (top colour, bottom colour)
COLOURS = {
    "Graphics": ((70, 110, 200), (30, 50, 110)),
    "World": ((80, 160, 70), (30, 80, 30)),
    "Water": ((40, 150, 190), (15, 60, 100)),
    "Game": ((200, 140, 40), (110, 60, 15)),
    "Test": ((170, 70, 170), (80, 25, 80)),
    "Examples": ((120, 120, 130), (50, 50, 60)),
}
# a short label per mod (2-4 letters); otherwise the first letters of its name
LABELS = {
    "graphics.msaa": "AA",
    "graphics.mipmaps": "MIP",
    "graphics.anisotropic": "ANI",
    "graphics.terrain-x2": "x2",
    "graphics.hd-tweaks": "HD",
    "water.living": "~",
    "world.ground-statics": "\u2193",
    "world.crops": "\u2740",
    "world.foliage": "\u2618",
    "world.foliage.beach": "\u2605",
    "world.foliage.butterflies": "\u2766",
    "test.miracle-dispensers": "\u2726",
    "game.skip-intro": "\u00bb",
    "examples": "{ }",
    "example.data-only": "{}",
    "example.lua-hello": "Lua",
    "example.lua-library": "Lib",
    "example.lua-consumer": "Use",
    "example.native-hello": "C",
    "example.native-library": "Lib",
    "example.native-consumer": "Use",
}
FONTS = ["C:/Windows/Fonts/seguisym.ttf", "C:/Windows/Fonts/segoeuib.ttf", "DejaVuSans-Bold.ttf"]


def font(size):
    for name in FONTS:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def english(value, fallback):
    if isinstance(value, dict):
        return value.get("en") or next(iter(value.values()), fallback)
    return value or fallback


def make(manifest_path, force):
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    folder = manifest_path.parent
    target = folder / "icon.png"
    if target.exists() and not force:
        return
    identifier = manifest["id"]
    category = english(manifest.get("category"), "Examples")
    top, bottom = COLOURS.get(category, COLOURS["Examples"])
    label = LABELS.get(identifier) or "".join(w[0] for w in english(manifest.get("name"), identifier).split()[:2]).upper()

    tile = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    gradient = Image.new("RGBA", (SIZE, SIZE))
    for y in range(SIZE):
        t = y / (SIZE - 1)
        gradient.paste(tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)) + (255,), (0, y, SIZE, y + 1))
    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).rounded_rectangle((4, 4, SIZE - 5, SIZE - 5), radius=22, fill=255)
    tile.paste(gradient, (0, 0), mask)
    draw = ImageDraw.Draw(tile)
    draw.rounded_rectangle((4, 4, SIZE - 5, SIZE - 5), radius=22, outline=(255, 255, 255, 90), width=3)
    size = 64 if len(label) <= 2 else 44
    text_font = font(size)
    box = draw.textbbox((0, 0), label, font=text_font)
    x = (SIZE - (box[2] - box[0])) / 2 - box[0]
    y = (SIZE - (box[3] - box[1])) / 2 - box[1]
    draw.text((x + 2, y + 3), label, font=text_font, fill=(0, 0, 0, 110))
    draw.text((x, y), label, font=text_font, fill=(255, 255, 255, 240))
    tile.save(target)
    print(f"{target}")


def main():
    force = "--force" in sys.argv
    root = pathlib.Path(__file__).resolve().parent.parent
    for path in sorted(root.glob("assets/mods/*/mod.json")) + sorted(root.glob("mods/examples/**/mod.json")):
        make(path, force)
    for path in sorted(root.glob("mods/examples/modpack.json")):
        make(path, force)


if __name__ == "__main__":
    main()
