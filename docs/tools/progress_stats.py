"""Progress counts for the dashboard page.

Reads the same sources as the viewer's live dashboard and writes what GitHub needs to show it without the viewer:

* the feature progress: every file listed in wiki/project/progress/INDEX.md, its rows counted by the progress tools'
  rule (the second column is the status: done, partial, todo or n/a). A file that is not there is taken from its INDEX
  row instead;
* the port to raffclar's target build: the table of wiki/project/port-audit.md;
* the refactor: the "Progress counts" table of wiki/project/refactor-conventions.md;
* the fork's open issues: with --issues <file>, a JSON-lines export of them (one object per issue: n, l labels, ids
  row ids), turned into wiki/project/issues.json (area label -> issue numbers, row id -> issue number, and which labels
  each dashboard bar links to). Without it the committed issues.json is used as it is.

It rewrites the static section of wiki/project/dashboard.md (between the dashboard:static markers) and the issue
links of port-audit.md's open rows. Run it after any of the sources change:

    python -I docs/tools/progress_stats.py                     (--json also prints the counts)
    python -I docs/tools/progress_stats.py --issues <file.jsonl>  (also refreshes issues.json)
    python -I docs/tools/progress_stats.py --raffclar <clone>   (also recounts raffclar.json from a clone of his tree)

Paths are relative to docs/, the folder above this script, so it runs from any directory.
"""

from __future__ import annotations

import datetime
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROJECT = ROOT / "wiki" / "project"
PROGRESS = PROJECT / "progress"
INDEX = PROGRESS / "INDEX.md"
PORT = PROJECT / "port-audit.md"
REFACTOR = PROJECT / "refactor-conventions.md"
DASHBOARD = PROJECT / "dashboard.md"
ISSUES_OUT = PROJECT / "issues.json"
RAFFCLAR_OUT = PROJECT / "raffclar.json"
RAFFCLAR_REF, UPSTREAM_REF, TARGET = "refs/remotes/raffclar/master", "refs/remotes/origin/master", "0222f9f5"
COMPARE = "https://github.com/raffclar/openblack/compare/{target}...master"

FORK = "https://github.com/diegoscood-ai/openblack"
STATUSES = ("done", "partial", "todo", "n/a")
START, END = "<!-- dashboard:static -->", "<!-- /dashboard:static -->"

# Which of the fork's area labels each progress domain and each port area links to.
DOMAIN_LABELS = {
    "animal": ["area:animals"], "audio": ["area:audio"], "building": ["area:buildings"], "camera": ["area:camera"],
    "creature": ["area:creature"], "debug": ["area:editor"], "engine": ["area:engine"], "gesture": ["area:magic"],
    "hand": ["area:hand"], "interface": ["area:interface"], "miracles": ["area:magic"], "nature": ["area:trees"],
    "ocean": ["area:rendering"], "physics": ["area:physics", "area:particles"], "rendering": ["area:rendering"],
    "resources": ["area:engine"], "scripts": ["area:script"], "sky": ["area:rendering"], "story": ["area:script"],
    "temple": ["area:buildings"], "terrain": ["area:rendering"], "town": ["area:town"], "video": ["area:video"],
    "villager": ["area:villagers"], "weather": ["area:weather"], "worship": ["area:villagers"],
}
AREA_LABELS = {
    "Creature, magic and camera": ["area:creature", "area:magic", "area:camera"],
    "Renderer and interface": ["area:rendering", "area:interface", "area:editor", "area:video", "area:weather"],
    "Villagers, scripts, hand and audio": ["area:villagers", "area:script", "area:hand", "area:audio",
                                           "area:particles", "area:town", "area:buildings"],
    "Testbed, physics and animals": ["area:testbed", "area:physics", "area:animals", "area:trees"],
    "Coordination": ["area:engine"],
}
# "What is left" that is all accepted exceptions, or only partly so.
ACCEPTED = re.compile(r"^(required by|all of them out of scope)", re.I)
PARTLY = re.compile(r"on purpose|out of scope|required by", re.I)


def cells(line: str) -> list[str]:
    return [c.strip() for c in line.strip().strip("|").split("|")]


def slug(text: str) -> str:
    """GitHub's heading anchor (github-slugger), for a heading seen once on its page."""
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text).lower()
    return re.sub(r"[^\w\- ]", "", text).replace(" ", "-")


def score(c: dict) -> int:
    total = c["done"] + c["partial"] + c["todo"]
    return round(100 * (c["done"] + 0.5 * c["partial"]) / total) if total else 0


# ---------- feature progress ----------


def count_file(path: Path) -> dict:
    """Rows by status, the progress tools' rule; values of a Status column outside the four go to 'other'."""
    c = dict.fromkeys(STATUSES, 0)
    c["other"] = 0
    status_col = None
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.startswith("|"):
            status_col = None
            continue
        row = cells(line)
        low = [x.lower() for x in row]
        if "status" in low:
            status_col = low.index("status")
            continue
        if re.fullmatch(r":?-+:?", row[0]):
            continue
        if len(row) >= 3 and row[1] in c and row[1] != "other":
            c[row[1]] += 1
        elif status_col is not None and status_col < len(row) and row[status_col]:
            c["other"] += 1
    return c


def features() -> dict:
    domains, order, current = {}, [], None
    for line in INDEX.read_text(encoding="utf-8").splitlines():
        m = re.match(r"## \[([^\]]+)\]\([^)]*\)(.*)", line)
        if m:
            current = m.group(1)
            order.append(current)
            domains[current] = {"name": current, "anchor": slug(line[3:]), "files": 0, "fromIndex": 0,
                                **dict.fromkeys(STATUSES, 0), "other": 0}
            continue
        m = re.match(r"\| \[([^\]]*)\]\(([^)]+\.md)\) \|", line)
        if not (m and current):
            continue
        row = cells(line)
        path = PROGRESS / m.group(2)
        d = domains[current]
        d["files"] += 1
        if path.is_file():
            c = count_file(path)
        else:
            d["fromIndex"] += 1
            c = dict(zip(STATUSES, (int(x or 0) for x in row[2:6])), other=0)
        for k in (*STATUSES, "other"):
            d[k] += c[k]
    overall = {k: sum(d[k] for d in domains.values()) for k in (*STATUSES, "other", "files", "fromIndex")}
    for d in (*domains.values(), overall):
        d["score"] = score(d)
        d["rows"] = sum(d[k] for k in STATUSES)
    ranked = sorted((domains[n] for n in order), key=lambda d: (-d["score"], order.index(d["name"])))
    return {"overall": overall, "domains": [d for d in ranked if d["rows"]]}


# ---------- port audit ----------


def port() -> dict:
    text = PORT.read_text(encoding="utf-8")
    areas, total = [], None
    for line in text.splitlines():
        m = re.match(r"\| (\*\*)?([^|*]+?)(\*\*)? \| (\*\*)?(\d+)(\*\*)? \| (\*\*)?(\d+)(\*\*)? \| (\*\*)?(\d+)(\*\*)? \|", line)
        if not m:
            continue
        entry = {"area": m.group(2), "rows": int(m.group(5)), "closed": int(m.group(8)), "open": int(m.group(11))}
        if entry["area"] == "All":
            total = entry
        else:
            entry["anchor"] = slug(entry["area"])
            areas.append(entry)
    # the open-row list, per area heading, to check the table against it
    listed, area = {}, None
    for line in text.split("## Open rows", 1)[-1].splitlines():
        if line.startswith("### "):
            area = line[4:].strip()
            listed[area] = []
        elif area and (m := re.match(r"- \*\*([^*]+)\*\*", line)):
            listed[area].append(m.group(1))
    problems = []
    if total is None:
        total = {"area": "All", "rows": sum(a["rows"] for a in areas), "closed": sum(a["closed"] for a in areas),
                 "open": sum(a["open"] for a in areas)}
    for a in areas:
        if a["rows"] != a["closed"] + a["open"]:
            problems.append(f"{a['area']}: {a['rows']} rows is not {a['closed']} closed + {a['open']} open")
        if len(listed.get(a["area"], [])) != a["open"]:
            problems.append(f"{a['area']}: the table says {a['open']} open, the list has {len(listed.get(a['area'], []))}")
    for k in ("rows", "closed", "open"):
        if total[k] != sum(a[k] for a in areas):
            problems.append(f"the All row's {k} is not the sum of the areas")
    return {"areas": areas, "total": total, "openIds": listed, "problems": problems}


# ---------- refactor ----------


def exception(pct: float | None, left: str) -> str:
    """'all' when what is left stays on purpose, 'part' when some of it does, '' otherwise."""
    if pct is None or pct >= 100:
        return ""
    return "all" if ACCEPTED.search(left) else "part" if PARTLY.search(left) else ""


def refactor() -> dict:
    text = REFACTOR.read_text(encoding="utf-8")
    section = text.split("### Progress counts", 1)[-1]
    metrics, header = [], None
    for line in section.splitlines():
        if line.startswith("#"):
            break
        if not line.startswith("|"):
            continue
        row = cells(line)
        if header is None:
            header = [h.lower() for h in row]
            continue
        if re.fullmatch(r":?-+:?", row[0]):
            continue
        get = lambda name: row[header.index(name)] if name in header and header.index(name) < len(row) else ""
        done = get("done")
        m = re.search(r"(\d+(?:\.\d+)?)\s*%", done)
        left = get("what is left")
        pct = float(m.group(1)) if m else None
        metrics.append({"what": row[0], "before": get("before"), "now": get("now"), "done": done, "pct": pct,
                        "left": left, "exception": exception(pct, left)})
    return {"metrics": metrics}


# ---------- unification with raffclar's tree ----------


def raffclar(repo: str) -> dict:
    """Commit counts of his master against upstream openblack and the port target, from a local clone."""
    git = lambda *args: subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True, check=True).stdout.strip()
    base = git("merge-base", RAFFCLAR_REF, UPSTREAM_REF)
    count = lambda rng, *extra: int(git("rev-list", "--count", *extra, rng))
    head, head_date = git("log", "-1", "--format=%H %cI", RAFFCLAR_REF).split()
    return {"target": TARGET, "base": base[:8], "head": head[:8], "headDate": head_date[:10],
            "total": count(f"{base}..{RAFFCLAR_REF}"), "inTarget": count(f"{base}..{TARGET}"),
            "after": count(f"{TARGET}..{RAFFCLAR_REF}"), "afterMerges": count(f"{TARGET}..{RAFFCLAR_REF}", "--merges"),
            "computed": datetime.date.today().isoformat(), "compare": COMPARE.format(target=TARGET)}


def load_raffclar() -> dict | None:
    return json.loads(RAFFCLAR_OUT.read_text(encoding="utf-8")) if RAFFCLAR_OUT.is_file() else None


UPLIFT = PROJECT / "raffclar_uplift.tsv"
PORTED_RX = re.compile(r"Ported from raffclar/openblack PR #(\d+)")


def imported() -> dict | None:
    """How much of the gain his newer lanes bring is in our tree: the uplift table's rows of the pull requests that a
    commit of ours names as "Ported from raffclar/openblack PR #N", over the table's total."""
    if not UPLIFT.is_file():
        return None
    gain = {}
    for line in UPLIFT.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#") or line.startswith("pr\t"):
            continue
        pr, _lane, up = line.split("\t")[:3]
        try:
            gain[pr] = gain.get(pr, 0.0) + float(up)
        except ValueError:
            pass
    r = subprocess.run(["git", "-C", str(ROOT), "log", "--format=%B"], capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    ported = sorted(set(PORTED_RX.findall(r.stdout)), key=int) if r.returncode == 0 else []
    total = sum(gain.values())
    done = sum(gain.get(pr, 0.0) for pr in ported)
    return {"total": round(total, 1), "done": round(done, 1), "prs": ported, "prsWithGain": sum(1 for v in gain.values() if v > 0)}


# ---------- issues ----------


def issues(source: Path | None) -> dict:
    if source is None:
        if ISSUES_OUT.is_file():
            return json.loads(ISSUES_OUT.read_text(encoding="utf-8"))
        source = Path()
    labels, rows = {}, {}
    if source.is_file():
        for line in source.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            item = json.loads(line)
            for label in item.get("l", []):
                labels.setdefault(label, []).append(item["n"])
            for rid in item.get("ids", []):
                rows.setdefault(rid, item["n"])
    for v in labels.values():
        v.sort(reverse=True)
    return {"fork": FORK, "labels": dict(sorted(labels.items())), "rows": dict(sorted(rows.items())),
            "domains": DOMAIN_LABELS, "areas": AREA_LABELS}


def issues_query(label_list: list[str]) -> str:
    return f"{FORK}/issues?q=is:issue+is:open+label:{','.join(label_list)}"


def issue_count(iss: dict, label_list: list[str]) -> int:
    return len({n for lab in label_list for n in iss["labels"].get(lab, [])})


# ---------- static Markdown ----------


def text_bar(done: float, partial: float = 0, width: int = 20) -> str:
    d = round(done * width)
    p = min(width - d, round((done + partial) * width) - d)
    return "█" * d + "▒" * p + "░" * (width - d - p)


def issues_cell(iss: dict, label_list: list[str] | None) -> str:
    if not label_list:
        return ""
    n = issue_count(iss, label_list)
    return f"[{n} open]({issues_query(label_list)})" if n else ""


def static_section(f: dict, p: dict, r: dict, iss: dict, rf: dict | None) -> str:
    o = f["overall"]
    active = o["done"] + o["partial"] + o["todo"]
    out = [START, "", "## Static copy", "", "*Written by `tools/progress_stats.py`; the viewer draws the same counts live.*", "",
           "### Features", "",
           f"**[Overall](progress/INDEX.md): {o['score']}%**: {o['done']} done, {o['partial']} partial, {o['todo']} todo"
           f" ({o['n/a']} n/a left out) across {o['rows']} rows in {o['files']} files.", "",
           f"`{text_bar(o['done'] / active, o['partial'] / active, 40)}`", "",
           "█ done · ▒ partial · ░ todo", "",
           "| Domain | Progress | Score | Done / partial / todo | Issues |", "|---|---|---:|---|---|"]
    for d in f["domains"]:
        act = d["done"] + d["partial"] + d["todo"]
        bar = text_bar(d["done"] / act, d["partial"] / act) if act else text_bar(0)
        out.append(f"| [{d['name']}](progress/INDEX.md#{d['anchor']}) | `{bar}` | {d['score']}% | "
                   f"{d['done']} / {d['partial']} / {d['todo']} | {issues_cell(iss, DOMAIN_LABELS.get(d['name']))} |")
    t = p["total"]
    out += ["", "### Unification with raffclar's tree", "",
            "Full unification means the port target closed and his newer commits audited.", ""]
    im = imported()
    if im and im["total"]:
        out += [f"**raffclar's newer work brought in: {im['done']:g} of {im['total']:g} rows of gain: "
                f"{round(100 * im['done'] / im['total'])}%**", "",
                f"`{text_bar(im['done'] / im['total'], 0, 40)}`", "",
                f"{len(im['prs'])} of his {im['prsWithGain']} pull requests with a gain are ported, lane by lane "
                "(commits marked \"Ported from raffclar/openblack PR #N\", unverified against v1.42 until the audit). "
                "The gain of each is in [raffclar_uplift.tsv](raffclar_uplift.tsv); importing all of it would take "
                "the features score from 44.8% to about 49.7%.", ""]
    if rf:
        out += [f"**raffclar's commits inside our port target: {rf['inTarget']} of {rf['total']}: "
                f"{round(100 * rf['inTarget'] / rf['total'])}%**", "",
                f"`{text_bar(rf['inTarget'] / rf['total'], 0, 40)}`", "",
                f"Counted from upstream openblack's `{rf['base']}` to his master `{rf['head']}` of {rf['headDate']} "
                f"(computed {rf['computed']}); the target is `{rf['target']}`.", ""]
    out += [f"**Port rows of the target closed: [{t['closed']} of {t['rows']} rows closed](port-audit.md), {t['open']} open: "
            f"{round(100 * t['closed'] / t['rows'])}%**", "",
            f"`{text_bar(t['closed'] / t['rows'], 0, 40)}`", "",
            "| Area | Progress | Closed | Open | Issues |", "|---|---|---:|---:|---|"]
    for a in p["areas"]:
        out.append(f"| [{a['area']}](port-audit.md#{a['anchor']}) | `{text_bar(a['closed'] / a['rows'])}` | "
                   f"{a['closed']} of {a['rows']} | {a['open']} | {issues_cell(iss, AREA_LABELS.get(a['area']))} |")
    if rf:
        out += ["", f"[{rf['after']} newer commits on his master after the target ({rf['afterMerges']} merges), "
                    f"not audited yet]({rf['compare']})."]
    out += ["", "### Refactor", "",
            "| Metric | Progress | Done | Before → now | What is left |", "|---|---|---:|---|---|"]
    for m in r["metrics"]:
        bar = f"`{text_bar(m['pct'] / 100)}`" if m["pct"] is not None else "(a count, no goal)"
        left = {"all": "**Accepted exception.** ", "part": "**Partly an accepted exception.** "}.get(m["exception"], "") + m["left"]
        out.append(f"| [{m['what']}](refactor-conventions.md#progress-counts) | {bar} | {m['done']} | "
                   f"{m['before']} → {m['now']} | {left} |")
    out += ["", END]
    return "\n".join(out)



README = PROJECT.parent.parent / "README.md"  # docs/README.md: what GitHub shows when the docs folder is opened


def readme(static: str) -> str:
    """docs/README.md for GitHub: how to read the wiki, then the dashboard's static copy with its links re-rooted."""
    import posixpath

    def reroot(m):
        target = m.group(2)
        if re.match(r"[a-z]+:|#", target):
            return m.group(0)
        return f"{m.group(1)}({posixpath.normpath(posixpath.join('wiki/project', target))})"

    body = re.sub(r"(\[[^\]]*\])\(([^)]+)\)", reroot, static.replace(START, "").replace(END, ""))
    body = body.replace("## Static copy", "## Progress", 1)
    return ("# openblack wiki\n\n"
            "How the original Black & White (v1.42) behaves, with addresses and formulas, and what openblack does.\n\n"
            "- **Read it:** open `docs/index.html` through `docs/open_wiki.bat` (Windows), or browse "
            "[the pages](SUMMARY.md) here.\n"
            "- **Start:** the [progress dashboard](wiki/project/dashboard.md) (live bars in the viewer) and "
            "[Reading this wiki](wiki/project/reading-the-wiki.md).\n\n"
            "*This page is written by `tools/progress_stats.py`.*\n" + body.rstrip() + "\n")

def write_if_changed(path: Path, text: str) -> bool:
    old = path.read_text(encoding="utf-8") if path.is_file() else None
    if old == text:
        return False
    path.write_text(text, encoding="utf-8", newline="\n")
    return True


def link_open_rows(iss: dict) -> bool:
    """Give each open row of port-audit.md that has an issue its "#N" link (and drop stale ones)."""
    text = PORT.read_text(encoding="utf-8")
    head, sep, rest = text.partition("## Open rows")
    lines = []
    for line in rest.split("\n"):
        m = re.match(r"(- \*\*([^*]+)\*\*.*?)( \(\[#\d+\]\([^)]*\)\))?$", line)
        if m:
            line = m.group(1)
            n = iss["rows"].get(m.group(2))
            if n:
                line += f" ([#{n}]({FORK}/issues/{n}))"
        lines.append(line)
    return write_if_changed(PORT, head + sep + "\n".join(lines))


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    source = None
    if "--issues" in sys.argv[1:]:
        at = sys.argv.index("--issues")
        if at + 1 >= len(sys.argv) or not Path(sys.argv[at + 1]).is_file():
            print("--issues needs the path of the issues export (JSON lines)", file=sys.stderr)
            return 2
        source = Path(sys.argv[at + 1])
    iss = issues(source)
    changed = []
    if write_if_changed(ISSUES_OUT, json.dumps(iss, indent=1) + "\n"):
        changed.append(ISSUES_OUT)
    if link_open_rows(iss):
        changed.append(PORT)
    if "--raffclar" in sys.argv[1:]:
        at = sys.argv.index("--raffclar")
        if at + 1 >= len(sys.argv):
            print("--raffclar needs the path of a clone with the raffclar and origin remotes", file=sys.stderr)
            return 2
        if write_if_changed(RAFFCLAR_OUT, json.dumps(raffclar(sys.argv[at + 1]), indent=1) + "\n"):
            changed.append(RAFFCLAR_OUT)
    rf = load_raffclar()
    f, p, r = features(), port(), refactor()
    text = DASHBOARD.read_text(encoding="utf-8")
    if START not in text or END not in text:
        print(f"{DASHBOARD} has no {START} ... {END} section", file=sys.stderr)
        return 1
    before, _, after = text.partition(START)
    _, _, after = after.partition(END)
    static = static_section(f, p, r, iss, rf)
    if write_if_changed(DASHBOARD, before + static + after):
        changed.append(DASHBOARD)
    if write_if_changed(README, readme(static)):
        changed.append(README)
    for problem in p["problems"]:
        print("port-audit.md:", problem, file=sys.stderr)
    if "--json" in sys.argv[1:]:
        print(json.dumps({"features": f, "port": p, "refactor": r, "raffclar": rf}, indent=1, ensure_ascii=False))
    else:
        o, t = f["overall"], p["total"]
        print(f"features {o['score']}%: {o['done']} done, {o['partial']} partial, {o['todo']} todo, {o['n/a']} n/a, "
              f"{o['other']} other, {o['files']} files ({o['fromIndex']} from INDEX); "
              f"port {t['closed']}/{t['rows']} closed; refactor {len(r['metrics'])} metrics")
        for path in changed:
            print("wrote", path.relative_to(ROOT))
    return 1 if p["problems"] else 0


if __name__ == "__main__":
    sys.exit(main())
