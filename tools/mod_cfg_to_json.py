#!/usr/bin/env python3
"""Converts the old mod data files to JSON (with comments, JSONC), keeping every rule and comment:

  foliage.cfg  -> foliage.json   (world.foliage and its modules: plants, [field], [field_stage ...], [flyer ...])
  textures.cfg -> textures.json  (graphics.hd-tweaks: texture id -> FNV-1a of the pack's DDS)

openblack reads the .json when there is one and the .cfg otherwise (3D/Foliage.cpp, Resources/HdTextures.cpp), turning
the JSON back into the same rules, so the result is identical; this tool checks that before it writes anything
(--check only checks).

usage: python tools/mod_cfg_to_json.py <file.cfg>... [--keep] [--check]
  By default the .cfg is renamed to .cfg.old once the .json is written (so nothing is lost); --keep leaves it.
"""

import json
import pathlib
import re
import sys

# keys whose value is a comma list (SplitList in Foliage.cpp): written as JSON arrays
LIST_KEYS = {"images", "texture", "terrain", "zone", "not_zone", "near", "over"}
NUMBER = re.compile(r"^-?\d+(\.\d+)?$")


def split_comment(line):
    for i, c in enumerate(line):
        if c in "#;":
            return line[:i], line[i + 1:].strip()
    return line, None


def parse_foliage(text):
    """[(None, comments)] header, then sections: {"name", "comments", "keys": [(key, value, comments, inline)]}"""
    header = []
    sections = []
    pending = []
    for raw in text.splitlines():
        body, comment = split_comment(raw)
        body = body.strip()
        if not body:
            if comment is not None:
                pending.append(comment)
            elif pending and not sections:
                pending.append("")  # keep blank lines inside the header comment
            continue
        if body.startswith("[") and body.endswith("]"):
            if not sections:
                header, pending = pending, []
            sections.append({"name": body[1:-1].strip(), "comments": pending, "keys": []})
            pending = []
            if comment is not None:
                sections[-1]["comments"].append(comment)
            continue
        if "=" not in body or not sections:
            raise ValueError(f"line not understood: {raw!r}")
        key, value = (part.strip() for part in body.split("=", 1))
        sections[-1]["keys"].append((key, value, pending, comment))
        pending = []
    while header and header[-1] == "":
        header.pop()
    return header, sections


def json_value(key, value):
    if key in LIST_KEYS:
        return [item.strip() for item in value.split(",") if item.strip()]
    if NUMBER.match(value):
        return float(value) if "." in value else int(value)
    return value


def cfg_value(value):
    """What 3D/Foliage.cpp makes of a JSON value (FoliageRulesFromJson): the same text as in the .cfg"""
    if isinstance(value, list):
        return ", ".join(value)
    if isinstance(value, bool):
        return "on" if value else "off"
    return str(value)


def comment_lines(comments, indent):
    return [f"{indent}//{' ' + c if c else ''}" for c in comments]


def write_foliage_json(header, sections):
    lines = comment_lines(header, "")
    lines.append("{")
    lines.append('  "schema": 1,')
    lines.append('  "rules": [')
    for s_index, section in enumerate(sections):
        lines += comment_lines(section["comments"], "    ")
        lines.append("    {")
        entries = [("section", section["name"], [], None)] + [(k, json_value(k, v), c, i) for k, v, c, i in section["keys"]]
        for k_index, (key, value, comments, inline) in enumerate(entries):
            lines += comment_lines(comments, "      ")
            comma = "," if k_index < len(entries) - 1 else ""
            text = f"      {json.dumps(key)}: {json.dumps(value, ensure_ascii=False)}{comma}"
            if inline:
                text += f"  // {inline}"
            lines.append(text)
        lines.append("    }" + ("," if s_index < len(sections) - 1 else ""))
    lines.append("  ]")
    lines.append("}")
    return "\n".join(lines) + "\n"


def strip_jsonc(text):
    """// comments out (outside strings), as nlohmann's ignore_comments does"""
    out = []
    for line in text.splitlines():
        in_string = False
        escaped = False
        cut = len(line)
        for i, c in enumerate(line):
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == '"':
                in_string = not in_string
            elif c == "/" and not in_string and line[i + 1:i + 2] == "/":
                cut = i
                break
        out.append(line[:cut])
    return "\n".join(out)


def check_foliage(original_text, json_text):
    _, sections = parse_foliage(original_text)
    data = json.loads(strip_jsonc(json_text))
    rules = data["rules"]
    assert len(rules) == len(sections), "a different number of sections"
    for section, rule in zip(sections, rules):
        assert rule["section"] == section["name"], (rule["section"], section["name"])
        got = [(k, cfg_value(v)) for k, v in rule.items() if k != "section"]
        want = [(k, ", ".join(i.strip() for i in v.split(",") if i.strip()) if k in LIST_KEYS else v)
                for k, v, _, _ in section["keys"]]
        assert got == want, (section["name"], got, want)


def parse_textures(text):
    header, entries = [], []
    pending = []
    for raw in text.splitlines():
        body, comment = split_comment(raw)
        body = body.strip()
        if not body:
            if comment is not None:
                pending.append(comment)
            continue
        key, value = (part.strip() for part in body.split("=", 1))
        if not entries:
            header, pending = pending, []
        entries.append((key, value))
    return header + pending, entries


def write_textures_json(header, entries):
    lines = comment_lines(header, "")
    lines.append("{")
    lines.append('  "schema": 1,')
    lines.append('  "textures": {')
    for index, (key, value) in enumerate(entries):
        lines.append(f'    "{key}": "{value}"' + ("," if index < len(entries) - 1 else ""))
    lines.append("  }")
    lines.append("}")
    return "\n".join(lines) + "\n"


def check_textures(original_text, json_text):
    _, entries = parse_textures(original_text)
    data = json.loads(strip_jsonc(json_text))["textures"]
    assert list(data.items()) == entries, "the textures differ"


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    keep = "--keep" in sys.argv
    check_only = "--check" in sys.argv
    if not args:
        print(__doc__)
        return 1
    for name in args:
        path = pathlib.Path(name)
        text = path.read_text(encoding="utf-8")
        if path.name == "foliage.cfg":
            out = write_foliage_json(*parse_foliage(text))
            check_foliage(text, out)
        elif path.name == "textures.cfg":
            out = write_textures_json(*parse_textures(text))
            check_textures(text, out)
        else:
            print(f"{path}: not a foliage.cfg or textures.cfg, left alone")
            continue
        target = path.with_suffix(".json")
        if check_only:
            print(f"{path}: converts exactly")
            continue
        target.write_text(out, encoding="utf-8")
        if not keep:
            path.rename(path.with_name(path.name + ".old"))
        print(f"{path} -> {target.name} (checked){'' if keep else ', the .cfg kept as ' + path.name + '.old'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
