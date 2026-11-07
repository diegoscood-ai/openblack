"""Counts the C arrays, the new / delete expressions and the direct file reads in the game's code, for the refactor
table's rows.

The sources under src/ are read with their comments, string literals and character literals taken out. A C array is a
declaration that starts a statement (or a member) and gives a name one or more [bounds]; function parameters are not
read (one written as an array is a pointer). A new or delete is the keyword, but not operator new / delete or a deleted
function (= delete). A direct file read is a std::ifstream or one of the C runtime's fopen functions. Third-party
headers under src/ are not ours and are left out.

    python -I docs/tools/cpp_counts.py <repository root>

It lists each one (file, line, what) and prints the three counts last.
"""

import pathlib
import re
import sys

THIRD_PARTY = ("src/Debug/imgui_memory_editor.h",)
QUALIFIERS = r"(?:(?:static|const|constexpr|inline|thread_local|mutable|volatile|unsigned|signed|struct|alignas\([^)]*\))\s+)*"
TYPE = r"[A-Za-z_][\w:]*(?:<[^;{}()]*?>)?(?:\s+const)?"
ARRAY = re.compile(r"(?:^|(?<=[;{}]))\s*" + QUALIFIERS + TYPE + r"(?:\s*[*&]+\s*|\s+)([A-Za-z_]\w*)\s*(?:\[[^\]\[;]*\]\s*)+(?==|;|\{)",
                   re.M)
NOT_TYPES = {"return", "else", "case", "goto", "delete", "new", "throw", "co_return", "using", "typedef"}
NEW = re.compile(r"(?<![\w.])new\b")
DELETE = re.compile(r"(?<![\w.])delete\b")
READS = re.compile(r"(?<![\w.:])(?:std::)?(?:ifstream|_?w?fopen(?:_s)?)\b")


def strip(text: str) -> str:
    """The text with its comments and literals blanked, newlines kept so that line numbers stay"""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        elif c == "R" and text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_")):
            k = text.find("(", i)
            delim = ")" + text[i + 2 : k] + '"'
            j = text.find(delim, k)
            j = n if j < 0 else j + len(delim)
            out.append('""' + "\n" * text.count("\n", i, j))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(c + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    arrays, news, reads = [], [], []
    for path in sorted((root / "src").rglob("*")):
        if path.suffix not in (".h", ".hpp", ".cpp", ".inl"):
            continue
        rel = path.relative_to(root).as_posix()
        if rel in THIRD_PARTY:
            continue
        text = strip(path.read_text(encoding="utf-8", errors="replace"))
        line_of = lambda pos: text.count("\n", 0, pos) + 1
        for m in ARRAY.finditer(text):
            first = m.group(0).split()[0] if m.group(0).split() else ""
            # a structured binding (auto [a, b]) names no array
            if first in NOT_TYPES or m.group(1) == "auto":
                continue
            arrays.append((rel, line_of(m.start(1)), " ".join(m.group(0).split())))
        for m in NEW.finditer(text):
            if text[max(0, m.start() - 9) : m.start()].rstrip().endswith("operator"):
                continue
            news.append((rel, line_of(m.start()), "new"))
        for m in DELETE.finditer(text):
            before = text[max(0, m.start() - 9) : m.start()].rstrip()
            if before.endswith("=") or before.endswith("operator"):
                continue
            news.append((rel, line_of(m.start()), "delete"))
        for m in READS.finditer(text):
            reads.append((rel, line_of(m.start()), m.group(0)))
    for rel, line, what in arrays:
        print(f"array\t{rel}:{line}\t{what}")
    for rel, line, what in news:
        print(f"{what}\t{rel}:{line}")
    for rel, line, what in reads:
        print(f"read\t{rel}:{line}\t{what}")
    print(f"{len(arrays)} C arrays, {len(news)} new / delete, {len(reads)} direct file reads")
    return 0


if __name__ == "__main__":
    sys.exit(main())
