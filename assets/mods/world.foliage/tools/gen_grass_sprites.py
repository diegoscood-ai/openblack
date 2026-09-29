"""Procedural foliage sprites for the world.foliage mod.

Grass blades are pure grey (the game tints grey texels with the ground colour under each plant); flower heads keep
their colours (saturated, so they are not tinted). Drawn at 4x and downsampled for smooth edges.
usage: gen_grass_sprites.py <output dir>
"""
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw

SS = 4  # supersampling


def blade(draw, base_x, base_y, height, width, bend, shade, rng):
    """A tapered, curved blade from (base_x, base_y) upwards; bend = horizontal offset of the tip"""
    steps = 14
    left, right = [], []
    for i in range(steps + 1):
        t = i / steps
        # quadratic bend, more at the top
        x = base_x + bend * t * t
        y = base_y - height * t
        # direction for the width offset
        dx = 2 * bend * t / height
        n = math.hypot(1.0, dx)
        nx, ny = 1.0 / n, dx / n
        w = width * (1.0 - t) ** 0.8 * 0.5
        left.append((x - nx * w, y - ny * w))
        right.append((x + nx * w, y + ny * w))
    polygon = left + right[::-1]
    # grey gradient: darker at the base, lighter to the tip, drawn as bands
    bands = 6
    for b in range(bands):
        t0, t1 = b / bands, (b + 1) / bands
        i0, i1 = int(t0 * steps), int(math.ceil(t1 * steps))
        part = left[i0:i1 + 1] + right[i0:i1 + 1][::-1]
        g = shade * (0.55 + 0.6 * (t0 + t1) / 2)
        g = max(0, min(255, int(g)))
        draw.polygon(part, fill=(g, g, g, 255))
    # a darker midrib on wide blades
    if width > 6 * SS:
        mid = [((l[0] + r[0]) / 2, (l[1] + r[1]) / 2) for l, r in zip(left, right)]
        g = max(0, int(shade * 0.55))
        draw.line(mid[:-2], fill=(g, g, g, 255), width=max(1, SS))


def tuft(size, blades, height_range, width_range, spread, seed):
    rng = random.Random(seed)
    w, h = size
    image = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    order = []
    for _ in range(blades):
        base_x = w * SS / 2 + rng.gauss(0, spread * w * SS / 2)
        height = rng.uniform(*height_range) * h * SS
        width = rng.uniform(*width_range) * SS
        lean = (base_x - w * SS / 2) / (w * SS / 2)
        bend = (lean * 0.5 + rng.uniform(-0.45, 0.45)) * height * 0.55
        shade = rng.uniform(150, 215)
        order.append((height, base_x, width, bend, shade))
    # tall blades behind, short in front
    for height, base_x, width, bend, shade in sorted(order, reverse=True):
        blade(draw, base_x, h * SS - 1, height, width, bend, shade, rng)
    return image


def flower_head(draw, cx, cy, radius, petal, centre, rng, petals=5):
    angle0 = rng.uniform(0, math.pi)
    for i in range(petals):
        a = angle0 + i * 2 * math.pi / petals
        px, py = cx + math.cos(a) * radius * 0.6, cy + math.sin(a) * radius * 0.45
        r = radius * 0.55
        draw.ellipse((px - r, py - r * 0.8, px + r, py + r * 0.8), fill=petal)
    r = radius * 0.35
    draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=centre)


def flowers(size, count, petal, centre, seed):
    rng = random.Random(seed)
    w, h = size
    image = tuft(size, 9, (0.35, 0.7), (4, 7), 0.5, seed + 1000)
    draw = ImageDraw.Draw(image)
    for _ in range(count):
        base_x = w * SS / 2 + rng.gauss(0, 0.25 * w * SS)
        top = h * SS * rng.uniform(0.15, 0.4)
        tip_x = base_x + rng.uniform(-0.12, 0.12) * w * SS
        g = int(rng.uniform(120, 160))
        draw.line([(base_x, h * SS - 1), ((base_x + tip_x) / 2, (h * SS + top) / 2), (tip_x, top)],
                  fill=(g, g, g, 255), width=int(2.2 * SS))
        flower_head(draw, tip_x, top, rng.uniform(9, 13) * SS, petal, centre, rng)
    return image


def save(image, path):
    image = image.resize((image.width // SS, image.height // SS), Image.LANCZOS)
    image.save(path)
    print(path, image.size)


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    # short dense grass
    for i in range(4):
        save(tuft((256, 160), 38, (0.35, 0.95), (7, 12), 0.55, 10 + i), out / f'gen_grass_{i + 1}.png')
    # longer, thinner grass
    for i in range(2):
        save(tuft((256, 300), 26, (0.45, 0.97), (6, 10), 0.4, 30 + i), out / f'gen_long_grass_{i + 1}.png')
    # flowers: white (slightly blue so they keep their colour), yellow, blue
    save(flowers((192, 256), 3, (205, 222, 255, 255), (250, 205, 60, 255), 50), out / 'gen_flowers_white.png')
    save(flowers((192, 256), 3, (255, 215, 40, 255), (200, 120, 20, 255), 51), out / 'gen_flowers_yellow.png')
    save(flowers((192, 256), 3, (120, 170, 255, 255), (250, 240, 200, 255), 52), out / 'gen_flowers_blue.png')


if __name__ == '__main__':
    main()
