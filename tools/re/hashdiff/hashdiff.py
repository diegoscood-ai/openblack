"""Compares two state-hash entities files (OPENBLACK_STATE_HASH_ENTITIES, see README.md): the first turn that differs,
the entities only one run has, and per entity the values that changed (old -> new), by turn then entity.

usage: python hashdiff.py <base>_entities.txt <run>_entities.txt [--part p,p] [--max N]
       python hashdiff.py --self-test
Exit 0 = identical (for the parts compared), 1 = they differ.
"""
import sys

# the names of what the built-in parts fold per entity, in order
NAMES = {
    "transform": ["pos.x", "pos.y", "pos.z"] + [f"rot{c}{r}" for c in range(3) for r in range(3)]
    + ["scale.x", "scale.y", "scale.z"],
    "creature": ["owner", "species", "leashable", "inDevScript", "alignment", "fatness", "strength", "size", "phase",
                 "hasHome", "home.x", "home.y", "home.z"],
}


def read(path, parts=None):
    """{(segment, turn): {(part, key): [values]}}; a turn going back starts a new segment (a cycle run's land load)"""
    out, segment, last, turn = {}, 0, None, None
    for line in open(path, encoding="utf-8", errors="replace"):
        f = line.split()
        if not f:
            continue
        if f[0] == "turn":
            turn = int(f[1])
            if last is not None and turn <= last:
                segment += 1
            last = turn
            out.setdefault((segment, turn), {})
            continue
        if len(f) < 3 or turn is None or (parts and f[1] not in parts):
            continue
        out[(segment, turn)].setdefault((f[1], f[2]), []).extend(f[3:])
    return out


def name(part, i):
    names = NAMES.get(part, [])
    return names[i] if i < len(names) else f"#{i}"


def compare(a, b, max_entities=20):
    lines, first = [], None
    for t in sorted(set(a) & set(b)):
        ea, eb = a[t], b[t]
        only_a = sorted(set(ea) - set(eb))
        only_b = sorted(set(eb) - set(ea))
        changed = sorted(k for k in set(ea) & set(eb) if ea[k] != eb[k])
        if not (only_a or only_b or changed):
            continue
        if first is None:
            first = t
            lines.append(f"first differing turn: segment {t[0]} turn {t[1]}")
        for part, key in only_a[:max_entities]:
            lines.append(f"  turn {t[1]} {part} {key}: only in base")
        for part, key in only_b[:max_entities]:
            lines.append(f"  turn {t[1]} {part} {key}: only in run")
        for part, key in changed[:max_entities]:
            va, vb = ea[(part, key)], eb[(part, key)]
            fields = [f"{name(part, i)} {x} -> {y}" for i, (x, y) in enumerate(zip(va, vb)) if x != y]
            if len(va) != len(vb):
                fields.append(f"{len(va)} values -> {len(vb)}")
            lines.append(f"  turn {t[1]} {part} {key}: " + ", ".join(fields))
        hidden = max(0, len(only_a) - max_entities) + max(0, len(only_b) - max_entities) + max(0, len(changed) - max_entities)
        if hidden:
            lines.append(f"  ... and {hidden} more at this turn")
        if len(lines) > 200:
            lines.append("(stopped after 200 lines)")
            break
    missing = sorted(set(a) ^ set(b))
    if missing:
        lines.append(f"turns only in one file: {len(missing)} (first {missing[0]})")
    return first, lines


def self_test():
    import os
    import tempfile
    base = "turn 1\n1 transform e5 f3f800000/1 f00000000/0\n1 creature e9 u0\nturn 2\n2 transform e5 f3f800000/1 f00000000/0\n"
    run = "turn 1\n1 transform e5 f3f800000/1 f00000000/0\n1 creature e9 u0\nturn 2\n2 transform e5 f40000000/2 f00000000/0\n2 transform e6 u1\n"
    with tempfile.TemporaryDirectory(dir=os.path.dirname(os.path.abspath(__file__))) as d:
        pa, pb = os.path.join(d, "a.txt"), os.path.join(d, "b.txt")
        open(pa, "w").write(base)
        open(pb, "w").write(run)
        first, lines = compare(read(pa), read(pb))
        assert first == (0, 2), first
        assert any("e5: pos.x f3f800000/1 -> f40000000/2" in x for x in lines), lines
        assert any("e6: only in run" in x for x in lines), lines
        assert compare(read(pa), read(pa))[0] is None
        assert compare(read(pa, {"creature"}), read(pb, {"creature"}))[0] is None
        # a cycle run: turn 1 again is a new segment
        open(pb, "w").write(base + "turn 1\n1 creature e9 u1\n")
        first, _ = compare(read(pa), read(pb))
        assert first is None  # the extra segment exists only in the run: reported as missing turns, not a value
    print("self-test passed")


def main(args):
    if args == ["--self-test"]:
        self_test()
        return 0
    parts, max_entities = None, 20
    if "--part" in args:
        i = args.index("--part")
        parts = set(args[i + 1].split(","))
        del args[i:i + 2]
    if "--max" in args:
        i = args.index("--max")
        max_entities = int(args[i + 1])
        del args[i:i + 2]
    if len(args) != 2 or any(x.startswith("-") for x in args):
        sys.exit(__doc__)
    a, b = read(args[0], parts), read(args[1], parts)
    first, lines = compare(a, b, max_entities)
    print(f"{len(a)} turns in base, {len(b)} in run" + (f", parts {sorted(parts)}" if parts else ""))
    print("\n".join(lines) if lines else "identical")
    return 0 if first is None and set(a) == set(b) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
