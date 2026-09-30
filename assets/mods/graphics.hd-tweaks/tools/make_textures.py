"""HD villager and animal textures for the mod graphics.hd-tweaks.

Every texture used by a person or animal mesh (MSH_P_* and MSH_A_* in AllMeshes.h) is decoded, brought back to its real resolution (some packs
store 256x256 art doubled with nearest pixels, which the upscaler would keep as blocks), upscaled x4 with Real-ESRGAN
(realesrgan-x4plus: keeps the painted detail; the anime model flattens it) and written as textures/<id hex>.png.
textures.cfg records the FNV-1a hash of each source DDS: the engine only uses an HD texture while the pack still has
that same texture. An image already in <out dir> whose hash is still the pack's is kept (no new upscale).

usage: python make_textures.py <AllMeshes.g3d> <AllMeshes.h> <out dir> [--tools <openblack bin>] [--esrgan <dir>]
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageChops, ImageStat

DEV = r"C:\Users\diewgarc\dev"


def fnv1a(data):
    h = 0x811C9DC5
    for b in data:
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def run(args):
    return subprocess.run(args, capture_output=True, text=True, check=False).stdout


def mesh_skins(tools, pack, header, tmp):
    names = {}
    for line in open(header, encoding="latin-1"):
        m = re.match(r"\s*(MSH_[PA]_\w+)\s*=\s*(\d+)", line)
        if m:
            names[int(m[2])] = m[1]
    skins = set()
    for index in sorted(names):
        path = os.path.join(tmp, f"{index}.l3d")
        run([os.path.join(tools, "packtool.exe"), "-m", str(index), "-e", path, pack])
        for s in re.findall(r"skinID: 0x([0-9A-F]+)", run([os.path.join(tools, "l3dtool.exe"), "read", "-P", path])):
            if s != "FFFFFFFF":
                skins.add(int(s, 16))
    return sorted(skins)


def native_size(image):
    """Halve while the image is (almost) a nearest-pixel doubling of its half."""
    while image.size[0] > 64:
        half = image.resize((image.size[0] // 2, image.size[1] // 2), Image.BOX)
        back = half.resize(image.size, Image.NEAREST)
        if sum(ImageStat.Stat(ImageChops.difference(image, back)).mean) / 3 > 1.5:
            break
        image = half
    return image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("pack")
    parser.add_argument("header")
    parser.add_argument("out")
    parser.add_argument("--tools", default=os.path.join(DEV, r"openblack\cmake-build-presets\ninja-multi-vcpkg\bin\Release"))
    parser.add_argument("--esrgan", default=os.path.join(DEV, r"tools\realesrgan"))
    args = parser.parse_args()

    os.makedirs(os.path.join(args.out, "textures"), exist_ok=True)
    tmp = tempfile.mkdtemp()
    try:
        skins = mesh_skins(args.tools, args.pack, args.header, tmp)
        # the images already made, kept while their source texture is the same
        made = {}
        cfg_path = os.path.join(args.out, "textures.cfg")
        if os.path.exists(cfg_path):
            for line in open(cfg_path, encoding="utf-8"):
                m = re.match(r"\s*([0-9a-f]+)\s*=\s*([0-9a-f]{8})", line)
                if m:
                    made[int(m[1], 16)] = m[2]
        entries = []
        for skin in skins:
            dds = os.path.join(tmp, f"{skin:x}.dds")
            run([os.path.join(args.tools, "packtool.exe"), "-t", f"{skin:x}", "-e", dds, args.pack])
            if not os.path.exists(dds):
                print(f"texture {skin:#x}: not in the pack", file=sys.stderr)
                continue
            data = open(dds, "rb").read()
            # the pack's DDS data after the 4-byte magic and the 124-byte header (G3DTexture::ddsData in openblack)
            source_hash = fnv1a(data[128:])
            image_path = os.path.join(args.out, "textures", f"{skin:x}.png")
            if made.get(skin) == f"{source_hash:08x}" and os.path.exists(image_path):
                entries.append(f"{skin:x} = {source_hash:08x}")
                print(f"texture {skin:#x}: kept")
                continue
            rgba = Image.open(dds).convert("RGBA")
            size = rgba.size[0]
            rgb = native_size(rgba.convert("RGB"))
            src = os.path.join(tmp, f"{skin:x}_in.png")
            dst = os.path.join(tmp, f"{skin:x}_x4.png")
            rgb.save(src)
            run([os.path.join(args.esrgan, "realesrgan-ncnn-vulkan.exe"), "-i", src, "-o", dst, "-n", "realesrgan-x4plus",
                 "-m", os.path.join(args.esrgan, "models")])
            hd = Image.open(dst).convert("RGB")
            alpha = rgba.getchannel("A")
            if alpha.getextrema() != (255, 255):
                hd.putalpha(alpha.resize(hd.size, Image.LANCZOS))
            hd.save(image_path, optimize=True)
            entries.append(f"{skin:x} = {source_hash:08x}")
            print(f"texture {skin:#x}: {size} px in the pack, {rgb.size[0]} px native -> {hd.size[0]} px")
        with open(os.path.join(args.out, "textures.cfg"), "w", encoding="utf-8") as cfg:
            cfg.write("# HD textures of the villagers and animals: <texture id (hex)> = <FNV-1a of the pack's DDS data>. The engine\n"
                      "# only uses textures/<id>.png while the pack's texture still has that hash.\n"
                      "# Made by tools/make_textures.py (Real-ESRGAN x4plus).\n")
            cfg.write("\n".join(entries) + "\n")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
