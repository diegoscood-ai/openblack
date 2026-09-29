"""Turns the green parts of foliage sprites grey (the world.foliage mod tints grey texels with the ground colour);
other colours (petals, cattail heads, seed heads) are kept. The grey is scaled so its mean is about 0.62, like the
generated gen_* sprites.
usage: mono_sprites.py <dir> <name.png>...   ->   <dir>/mono_<name>.png
"""
import colorsys
import sys
from pathlib import Path

from PIL import Image


def is_foliage(r, g, b):
    h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
    return s > 0.12 and 0.09 <= h <= 0.47  # hue 32..170 degrees: yellow-green to green


def is_pale_leaf(r, g, b):
    """Highlights and edges of the leaves: little saturation but not the near-white of a petal"""
    h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
    return s <= 0.12 and v < 0.85


def convert(path, out):
    image = Image.open(path).convert('RGBA')
    pixels = image.load()
    greys = []
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = pixels[x, y]
            if a > 0 and (is_foliage(r, g, b) or is_pale_leaf(r, g, b)):
                greys.append((x, y, 0.299 * r + 0.587 * g + 0.114 * b))
    if not greys:
        return
    mean = sum(v for _, _, v in greys) / len(greys)
    scale = 0.62 * 255 / max(mean, 1)
    grey_set = set()
    for x, y, v in greys:
        g = max(0, min(255, int(v * scale)))
        pixels[x, y] = (g, g, g, pixels[x, y][3])
        grey_set.add((x, y))
    # white or grey petals would be tinted too (the game tints texels with saturation < 0.2): a slight cream
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = pixels[x, y]
            if a > 0 and (x, y) not in grey_set:
                h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
                if s < 0.22 and v >= 0.85:
                    r2, g2, b2 = colorsys.hsv_to_rgb(0.13, 0.24, max(v, 0.3))
                    pixels[x, y] = (int(r2 * 255), int(g2 * 255), int(b2 * 255), a)
    image.save(out)
    print(out, f'{len(greys)} texels grey')


def main():
    directory = Path(sys.argv[1])
    for name in sys.argv[2:]:
        convert(directory / name, directory / f'mono_{name}')


if __name__ == '__main__':
    main()
