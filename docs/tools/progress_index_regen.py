import os
import sys
from pathlib import Path

sys.dont_write_bytecode = True  # no __pycache__ beside the docs
sys.path.insert(0, str(Path(__file__).resolve().parent))
import progress_rows as rows  # noqa: E402

# Rebuilds progress/INDEX.md from the feature files' own rows (the rule is in progress_rows.py).
root = sys.argv[1]


def count(path):
    counts = rows.empty()
    title = None
    for line in open(path, encoding="utf-8").read().splitlines():
        if title is None and line.startswith("# "):
            title = line[2:].strip()
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        parsed = rows.parse(cells[1]) if len(cells) >= 3 else None
        if parsed:
            rows.add(counts, *parsed)
    return title, counts


def pct(c):
    return f"{rows.score(c)}%" if rows.counted(c) else ""


def partial_cell(c):
    """The partial rows, with the share their measured ones add up to and how many are not measured yet."""
    if not c["partial"]:
        return "0"
    left = c["partial"] - c["partialMeasured"]
    return f"{c['partial']} ({c['partialDone']:.1f} done" + (f", {left} not measured)" if left else ")")


domains = {}
for d in sorted(os.listdir(root)):
    p = os.path.join(root, d)
    if not os.path.isdir(p) or d.startswith("."):
        continue
    files = []
    for f in sorted(os.listdir(p)):
        if f.endswith(".md"):
            title, c = count(os.path.join(p, f))
            files.append((f, title, c))
    # one level of sub-folders (e.g. story/silver_scrolls), listed after the folder's own files
    for sub in sorted(os.listdir(p)):
        sp = os.path.join(p, sub)
        if os.path.isdir(sp) and not sub.startswith("."):
            for f in sorted(os.listdir(sp)):
                if f.endswith(".md"):
                    title, c = count(os.path.join(sp, f))
                    files.append((f"{sub}/{f}", title, c))
    if files:
        domains[d] = files

overall = rows.empty()
summary = []
for d, files in domains.items():
    c = rows.empty()
    for _, _, fc in files:
        rows.merge(c, fc)
    rows.merge(overall, c)
    summary.append((d, len(files), rows.counted(c), rows.score(c)))

nfiles = sum(len(f) for f in domains.values())
out = ["# Progress index", "",
       "Every domain and feature file, with its score: done and chosen count 1, a partial row its measured share "
       "(a partial row not measured yet counts 0), inferred and pending 0. Generated from the files' own rows; "
       "regenerate after editing.", "",
       f"**Overall: {overall['done']} done, {overall['chosen']} chosen, {partial_cell(overall)} partial, "
       f"{overall['inferred']} inferred, {overall['pending']} pending across {rows.counted(overall)} rows in {nfiles} "
       f"files — {rows.score(overall)}%**", "",
       "| Domain | Files | Rows | Score |", "|---|---|---|---|"]
for d, n, nrows, s in sorted(summary, key=lambda x: -x[3]):
    out.append(f"| [{d}]({d}/) | {n} | {nrows} | {s}% |")
for d, files in domains.items():
    c = rows.empty()
    for _, _, fc in files:
        rows.merge(c, fc)
    # a fixed anchor per domain, since the heading's own one changes with the score (#ocean--94)
    out += ["", f'<a id="{d}"></a>', "", f"## [{d}]({d}/) — {rows.score(c)}%", "",
            "| File | Rows | Done | Chosen | Partial | Inferred | Pending | Score |", "|---|---|---|---|---|---|---|---|"]
    for f, title, fc in files:
        out.append(f"| [{title}]({d}/{f}) | {rows.counted(fc)} | {fc['done']} | {fc['chosen']} | {partial_cell(fc)} | "
                   f"{fc['inferred']} | {fc['pending']} | {pct(fc)} |")
open(os.path.join(root, "INDEX.md"), "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
print("files", nfiles, {k: round(v, 2) if isinstance(v, float) else v for k, v in overall.items()}, rows.score(overall))
