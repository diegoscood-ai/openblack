"""The tilled soil drawn under the crop fields (world.foliage, [field] soil): brown earth in furrows along the image's x,
fading out irregularly at the edges so it blends into the land like the footprints of the buildings.
usage: gen_field_soil.py <out.png>
"""
import math
import random
import sys

from PIL import Image

SIZE = 256
FURROWS = 14    # ridges across the image (the field is about 10 units wide: one every ~0.7 units)
EDGE = 0.16     # share of each side that fades out
OPACITY = 0.88  # in the middle: a little of the land still shows through


def noise(seed):
    """Smooth value noise on a 16 x 16 lattice, 0..1"""
    rng = random.Random(seed)
    lattice = [[rng.random() for _ in range(17)] for _ in range(17)]

    def at(x, y):
        gx, gy = x * 16, y * 16
        ix, iy = int(gx), int(gy)
        fx, fy = gx - ix, gy - iy
        fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        a, b = lattice[iy][ix], lattice[iy][ix + 1]
        c, d = lattice[iy + 1][ix], lattice[iy + 1][ix + 1]
        return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy

    return at


def main():
    coarse = noise(1)
    fine = noise(2)
    edge_noise = noise(3)
    rng = random.Random(4)
    image = Image.new('RGBA', (SIZE, SIZE))
    pixels = image.load()
    for y in range(SIZE):
        for x in range(SIZE):
            u, v = (x + 0.5) / SIZE, (y + 0.5) / SIZE
            # furrows: a ridge profile across v, wobbling a little along u
            phase = v * FURROWS + 0.15 * math.sin(u * 9.0 + v * 3.0)
            ridge = 0.5 + 0.5 * math.cos(phase * 2 * math.pi)
            shade = 0.72 + 0.28 * ridge + 0.18 * (coarse(u, v) - 0.5) + 0.12 * (fine(u * 0.9, v * 0.9) - 0.5)
            shade += 0.06 * (rng.random() - 0.5)
            r, g, b = 100 * shade, 72 * shade, 46 * shade
            # soft irregular edge
            border = min(u, v, 1 - u, 1 - v) / EDGE + 0.6 * (edge_noise(u, v) - 0.5)
            alpha = OPACITY * max(0.0, min(1.0, border)) ** 1.5
            pixels[x, y] = (int(min(255, r)), int(min(255, g)), int(min(255, b)), int(255 * alpha))
    image.save(sys.argv[1])
    print(sys.argv[1])


if __name__ == '__main__':
    main()
