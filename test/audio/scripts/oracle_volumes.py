"""Records a volume scenario's sound map, alignment and sky type results again on the game's own executable, run
natively at the game's FPU control word (24-bit precision) through the exe oracle, in place of the ones
emu_soundmap.py recorded under Unicorn, whose x87 keeps every intermediate at full precision.

Real code, as emu_soundmap.py sets it up: GSoundMap::Update on the scenario's synthetic island, fn_005E2240
(alignment) and LH3DSky's SetTime / Time2SkyType. Stubbed: GGame::GetCamera (the scenario's camera) and
GDebug::SetMessage. The inputs and the bank steps are kept as they are.

Usage (Python 3, the oracle's folder in BW1_ORACLE, its exe the frozen v1.42 runblack.exe):
    python oracle_volumes.py ../scenarios/volumes_1.json ../scenarios/volumes_2.json
"""
import hashlib
import json
import os
import struct
import sys

sys.path.insert(0, os.environ.get("BW1_ORACLE", "oracle"))
import oracle  # noqa: E402

SOUND_MAP_UPDATE = 0x71D6F0
ALIGNMENT = 0x5E2240
SKY_SET_TIME = 0x86A2C0
GET_CAMERA = 0x54C180
DEBUG_SET_MESSAGE = 0x511DA0

G_GAME = 0xD0195C
G_GLOBAL_AUDIO = 0xCD3B20
CAMERA_POSITION = 0xEA1DB8
INDEX_BLOCK = 0xE9C964
PTR_BLOCKS = 0xE9C564
SKY_TYPE = 0xFA26BC
SKY_TIMES = 0xFA2694  # dayFull, duskEnd, duskStart, nightFull
SOUND_INFO = 0xD9A908
CELL_BYTES = 17 * 17 * 8
BATCH = 10


def u32(value):
    return struct.pack("<I", value & 0xFFFFFFFF)


def f32_bits(value):
    return struct.unpack("<I", struct.pack("<f", value))[0]


def bits_f32(bits):
    return struct.unpack("<f", struct.pack("<I", bits))[0]


def run_batches(o, requests, read):
    results = []
    for start in range(0, len(requests), BATCH):
        for response in o.batch(requests[start:start + BATCH]):
            if not response.get("ok"):
                raise oracle.OracleError(o.describe(response), response)
            results.append(read(response["read_bytes"]))
    return results


def island_allocations(scenario):
    items = [("game", {"size": 0x260000}), ("camera", {"size": 0x200}), ("soundMap", {"size": 0x110})]
    for index, cells in enumerate(scenario["blocks"]):
        items.append((f"block{index}", {"size": CELL_BYTES, "hex": cells}))
    return items


def run_sound_map(o, scenario):
    # The scratch buffers are laid out the same way at every request: the first one gives their addresses
    allocations = island_allocations(scenario)
    first = o.request({"alloc": [dict(spec, name=name) for name, spec in allocations]}, check=True)
    buffers = {name: int(address, 16) for name, address in first["buffers"].items()}
    o.stub(GET_CAMERA, "u32", buffers["camera"], pop=0)
    fixed = [(INDEX_BLOCK, bytes.fromhex(scenario["lookup"]))]
    for index in range(len(scenario["blocks"])):
        fixed.append((PTR_BLOCKS + (index + 1) * 4, u32(buffers[f"block{index}"])))
    fixed.append((SOUND_INFO, b"".join(u32(value) for value in scenario["soundInfo"])))
    game = buffers["game"]
    fixed += [(G_GAME, u32(game)), (game + 0x59B8 + 0x28, u32(8)), (game + 0x59B8 + 0x2C, u32(0x2000)),
              (game + 0x59B8 + 0x30, u32(0x2000)), (game + 0x2502C0, u32(buffers["camera"]))]
    requests = []
    for query in scenario["queries"]:
        weather = query["weather"]
        writes = fixed + [
            (CAMERA_POSITION, u32(query["x"]) + u32(query["y"]) + u32(query["z"])),
            (buffers["camera"] + 0x80,
             bytes([0, weather[0] & 0xFF, weather[1] & 0xFF, 0, weather[2] & 0xFF, weather[3] & 0xFF, 0, 0])),
            (SKY_TYPE, u32(query["sky"])),
        ]
        requests.append(oracle.Oracle.make_request(
            f"0x{SOUND_MAP_UPDATE:08X}", "thiscall", (), "void", this=buffers["soundMap"], alloc=allocations,
            write=writes, read=[(buffers["soundMap"] + 0xB4, 14 * 4), (buffers["soundMap"] + 0x108, 4)]))
    return run_batches(o, requests, lambda raw: {"volumes": list(struct.unpack("<14I", raw[0])),
                                                 "heightAboveLand": struct.unpack("<I", raw[1])[0]})


def run_alignment(o, scenario):
    requests = []
    for bits in scenario["alignments"]:
        # the players' update hands the audio (alignment + 1) / 2, in float steps
        argument = f32_bits((bits_f32(bits) + 1.0) * 0.5)
        requests.append(oracle.Oracle.make_request(
            f"0x{ALIGNMENT:08X}", "stdcall", ({"type": "f32", "bits": f"0x{argument:08X}"},), "void",
            alloc=[("audio", {"size": 0x300})], write=[(G_GLOBAL_AUDIO, u32(0x20000000))],
            read=[(0x20000000 + 0x190, 4)]))
    first = o.request({"alloc": [{"name": "audio", "size": 0x300}]}, check=True)
    if int(first["buffers"]["audio"], 16) != 0x20000000:
        raise oracle.OracleError("the oracle's scratch arena is not at 0x20000000")
    return run_batches(o, requests, lambda raw: struct.unpack("<I", raw[0])[0])


def run_sky(o, scenario):
    requests = []
    for case in scenario["sky"]:
        times = b"".join(u32(value) for value in case["times"])
        requests.append(oracle.Oracle.make_request(
            f"0x{SKY_SET_TIME:08X}", "cdecl", ({"type": "f32", "bits": f"0x{case['time']:08X}"},), "void",
            write=[(SKY_TIMES, times)], read=[(SKY_TYPE, 4)]))
    return run_batches(o, requests, lambda raw: struct.unpack("<I", raw[0])[0])


def main():
    for path in sys.argv[1:]:
        with open(path, encoding="utf-8") as f:
            scenario = json.load(f)
        with oracle.Oracle() as o:
            o.stub(DEBUG_SET_MESSAGE, "u32", 0, pop=0)
            sound_map = run_sound_map(o, scenario)
            o.reset()
            alignments = run_alignment(o, scenario)
            sky = run_sky(o, scenario)
            with open(o.info["exe"], "rb") as exe:
                sha1 = hashlib.sha1(exe.read()).hexdigest()
            control_word = o.info["fpu_cw"]
        expected = scenario["expected"]
        changed = sum(1 for new, old in zip(sound_map, expected["soundMap"]) if new["volumes"] != old["volumes"])
        print(f"{path}: {changed} of {len(sound_map)} sound map queries, "
              f"{sum(a != b for a, b in zip(alignments, expected['alignments']))} alignments and "
              f"{sum(a != b for a, b in zip(sky, expected['sky']))} sky types recorded differently")
        expected["soundMap"] = sound_map
        expected["alignments"] = alignments
        expected["sky"] = sky
        scenario["recordedOn"] = {"soundMap, alignments, sky": "runblack.exe through the oracle (oracle_volumes.py)",
                                  "bankSteps": "Unicorn (emu_soundmap.py)", "exeSha1": sha1, "fpuControlWord": control_word}
        with open(path, "w", newline="\n", encoding="utf-8") as f:
            json.dump(scenario, f, separators=(",", ":"))
            f.write("\n")


if __name__ == "__main__":
    main()
