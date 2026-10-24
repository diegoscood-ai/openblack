import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True  # no __pycache__ beside the docs
sys.path.insert(0, str(Path(__file__).resolve().parent))
import progress_rows as rows  # noqa: E402

# Recounts the rows of progress files and rewrites their Progress line (the rule is in progress_rows.py).
for path in sys.argv[1:]:
    text = open(path, encoding="utf-8").read()
    counts = rows.empty()
    for line in text.splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        parsed = rows.parse(cells[1]) if len(cells) >= 3 else None
        if parsed:
            rows.add(counts, *parsed)
    line = (f"**Progress: {counts['done']}/{rows.counted(counts)} done, {counts['chosen']} chosen, "
            f"{counts['partial']} partial, {counts['inferred']} inferred — {rows.score(counts)}%**")
    text = re.sub(r"\*\*Progress: .*\*\*", line, text, count=1)
    open(path, "w", encoding="utf-8", newline="\n").write(text)
    print(path, "rows", rows.counted(counts), counts, line)
