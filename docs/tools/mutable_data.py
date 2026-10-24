"""Counts the mutable globals and statics of the game's code from its compiled objects.

The "Mutable globals and file- or function-scope statics" row of wiki/project/refactor-conventions.md#progress-counts
comes from this script. It reads the symbol tables of a Release build's object files with MSVC's dumpbin and keeps the
symbols in the .data, .bss and .tls$ sections, then leaves out what is const, EnTT's own statics (the locator's slots,
type_id and type_index), the compiler's own (guards, string literals, RTTI) and third-party code (stb, ImGui and its
memory editor). A symbol that several object files share (an inline variable) counts once. Code built only off Windows
is not in these objects and is read by hand.

    python -I docs/tools/mutable_data.py <dumpbin.exe> <objects folder>...

An objects folder is a target's own, e.g. <build>/src/CMakeFiles/openblack_lib.dir/Release and
<build>/src/CMakeFiles/openblack.dir/Release. It prints one line per variable (the object file it was first seen in,
then its demangled declaration) and the count last.
"""

import pathlib
import re
import subprocess
import sys

SYMBOL = re.compile(r"^[0-9A-F]+ [0-9A-F]+ (SECT[0-9A-F]+) +\w+(?: \(\))? +(External|Static) +\| (\S+)(?: \((.*)\))?$")
MUTABLE_SECTIONS = (".data", ".bss", ".tls$")
LIBRARY_STATICS = re.compile(r"entt::locator<|entt::type_id<|entt::type_index<|entt::internal::|__cdecl std::|_Mbstatet|"
                             r"stbi|_OptionsStorage|_Avx2")
THIRD_PARTY = re.compile(r"^(Im|imgui)|ImGui|MemoryEditor|cxxopts::")
# const right before the variable's own name, or before the function it is a static of
CONST_OBJECT = re.compile(r"\bconst (`|[A-Za-z_][\w:]*$)")


def mutable_symbols(dumpbin: str, obj: pathlib.Path):
    """The mutable data symbols of one object file, demangled"""
    text = subprocess.run([dumpbin, "/nologo", "/symbols", str(obj)], capture_output=True, text=True,
                          errors="replace").stdout
    sections = {}
    for line in text.splitlines():
        match = SYMBOL.match(line)
        if not match:
            continue
        section, _, name, demangled = match.groups()
        if name.startswith("."):
            sections[section] = name
            continue
        if sections.get(section) not in MUTABLE_SECTIONS:
            continue
        if name.startswith(("??_", "__", "$")) or "TSS" in name:
            continue
        declaration = demangled or name
        if LIBRARY_STATICS.search(declaration) or THIRD_PARTY.search(declaration):
            continue
        # the declared type is what comes before the enclosing function's quoted name, if any
        declared = declaration.split("`")[0]
        if re.search(r"\bconst\b", declared) and not declared.rstrip().endswith("*"):
            continue
        if CONST_OBJECT.search(declaration):
            continue
        # a function-scope static's guard ($S1, $S2...) is the compiler's
        if re.search(r"::\$S\d+$", declaration):
            continue
        yield declaration


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    dumpbin, folders = sys.argv[1], sys.argv[2:]
    seen = {}
    for folder in folders:
        root = pathlib.Path(folder)
        for obj in sorted(root.rglob("*.obj")):
            for declaration in mutable_symbols(dumpbin, obj):
                seen.setdefault(declaration, obj.relative_to(root).as_posix())
    for declaration, obj in sorted(seen.items(), key=lambda item: (item[1], item[0])):
        print(f"{obj}\t{declaration}")
    print(f"{len(seen)} mutable globals and statics")
    return 0


if __name__ == "__main__":
    sys.exit(main())
