"""Progress counts for the dashboard page.

Reads the same sources as the viewer's live dashboard and writes what GitHub needs to show it without the viewer:

* the feature progress: every file listed in wiki/project/progress/INDEX.md, its rows counted by the progress tools'
  rule (progress_rows.py: the second column is the status, done, chosen, partial NN%, inferred or pending; the older
  "todo" counts as pending and "n/a" is counted apart). A file that is not there is taken from its INDEX row instead;
* raffclar's work in our tree: wiki/project/raffclar-status.tsv, one row per pull request merged into his current
  master (and one for his commits outside any pull request), each in, partial, pending or n/a;
* the refactor: the "Progress counts" table of wiki/project/refactor-conventions.md, whose "Test files" row it
  rewrites from the test sources under test/ (files, test cases and executables);
* the milestones: the table of wiki/project/milestones.md;
* the fork's open issues: with --issues <file>, a JSON-lines export of them (one object per issue: n, l labels, ids
  row ids), turned into wiki/project/issues.json (area label -> issue numbers, row id -> issue number, and which labels
  each dashboard bar links to). Without it the committed issues.json is used as it is.

It rewrites the static section of wiki/project/dashboard.md (between the dashboard:static markers), the bars it
links to (one small SVG file each in wiki/project/bars, rewritten only when its numbers change), the generated part of
wiki/project/raffclar-status.md, the progress block of the repository's README.md (between its progress:start and
progress:end markers) and the issue links of port-audit.md's open rows (the port audit is kept as history). Run it
after any of the sources change:

    python -I docs/tools/progress_stats.py                     (--json also prints the counts)
    python -I docs/tools/progress_stats.py --issues <file.jsonl>  (also refreshes issues.json)
    python -I docs/tools/progress_stats.py --raffclar <clone>   (also reads his master from a clone of his tree:
                                                                 raffclar.json, and new pull requests into the table)

Paths are relative to docs/, the folder above this script, so it runs from any directory.
"""

from __future__ import annotations

import datetime
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True  # no __pycache__ beside the docs
sys.path.insert(0, str(Path(__file__).resolve().parent))
import progress_rows as rows  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
PROJECT = ROOT / "wiki" / "project"
PROGRESS = PROJECT / "progress"
INDEX = PROGRESS / "INDEX.md"
PORT = PROJECT / "port-audit.md"
REFACTOR = PROJECT / "refactor-conventions.md"
DASHBOARD = PROJECT / "dashboard.md"
ISSUES_OUT = PROJECT / "issues.json"
RAFFCLAR_OUT = PROJECT / "raffclar.json"
RAFFCLAR_REF, UPSTREAM_REF = "refs/remotes/raffclar/master", "refs/remotes/origin/master"

FORK = "https://github.com/diegoscood-ai/openblack"
STATUSES = rows.STATUSES
START, END = "<!-- dashboard:static -->", "<!-- /dashboard:static -->"

# Each progress domain has a label of its own on the fork, area:<domain> (its folder's name), which its issue link and
# count use (domain_labels, below). These are the older, coarser area labels a domain also links to while an open issue
# carries one of them without any domain's own label, that is until that issue is sorted. Each port area links to the
# labels of AREA_LABELS.
DOMAIN_LABELS = {
    "animal": ["area:animals"], "audio": ["area:audio"], "building": ["area:buildings"], "camera": ["area:camera"],
    "creature": ["area:creature"], "debug": ["area:editor"], "engine": ["area:engine"], "gesture": ["area:magic"],
    "hand": ["area:hand"], "interface": ["area:interface"], "miracles": ["area:magic"], "nature": ["area:trees"],
    "ocean": ["area:rendering"], "physics": ["area:physics", "area:particles"], "rendering": ["area:rendering"],
    "resources": ["area:engine"], "scripts": ["area:script"], "sky": ["area:rendering"], "story": ["area:script"],
    "temple": ["area:buildings"], "terrain": ["area:rendering"], "town": ["area:town"], "video": ["area:video"],
    "villager": ["area:villagers"], "weather": ["area:weather"], "worship": ["area:villagers"],
}
# The readable name each progress domain (a folder of progress/) is shown under; the viewer reads it from issues.json,
# as it reads the families (FAMILIES, below).
DOMAIN_NAMES = {
    "animal": "Animals", "audio": "Sound and music", "building": "Buildings", "camera": "Camera",
    "creature": "Creature", "debug": "Debug tools", "easter_eggs": "Easter eggs", "engine": "Engine and files",
    "gesture": "Gestures", "hand": "The hand", "interface": "Interface", "miracles": "Miracles",
    "multiplayer": "Multiplayer", "nature": "Nature", "ocean": "Ocean", "pc_integration": "PC integration",
    "physics": "Physics", "rendering": "Rendering", "resources": "Food, wood and resources", "rival_gods": "Rival gods",
    "scripts": "Scripts", "sky": "Sky", "story": "Story", "temple": "Temple", "terrain": "Terrain", "town": "Towns",
    "vanilla_bugs": "Vanilla bugs", "video": "Video", "villager": "Villagers", "weather": "Weather",
    "worship": "Worship and belief",
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


def display_name(domain: str) -> str:
    """A domain's readable name; a folder not in DOMAIN_NAMES gets its own name with spaces."""
    return DOMAIN_NAMES.get(domain) or domain.replace("_", " ").capitalize()


def score(c: dict) -> int:
    return rows.score(c)


# ---------- feature progress ----------


BUGS_RX = re.compile(r"\*\*Bugs: (\d+) \(soft-locks and lost progress: (\d+)\)\*\*")
COUNTED = (*rows.KEYS, "other", "bugs", "softlocks")


def count_file(path: Path) -> dict:
    """Rows by status, the progress tools' rule (progress_rows.py); values of a Status column that are no status go to
    'other'. A bug log's "**Bugs: N (soft-locks and lost progress: S)**" line (vanilla_bugs_count.py writes it) gives
    bugs and softlocks."""
    c = rows.empty()
    c["other"] = 0
    text = path.read_text(encoding="utf-8")
    m = BUGS_RX.search(text)
    c["bugs"], c["softlocks"] = (int(m.group(1)), int(m.group(2))) if m else (0, 0)
    status_col = None
    for line in text.splitlines():
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
        parsed = rows.parse(row[1]) if len(row) >= 3 else None
        if parsed:
            rows.add(c, *parsed)
        elif status_col is not None and status_col < len(row) and row[status_col]:
            c["other"] += 1
    return c


def index_row(row: list[str]) -> dict:
    """A file's counts from its INDEX row (| File | Rows | Done | Chosen | Partial | Inferred | Pending | Score |), the
    partial cell "N (S done, M not measured)" as progress_index_regen.py writes it."""
    c = rows.empty()
    c.update(other=0, bugs=0, softlocks=0)

    def num(x: str) -> int:
        return int(re.match(r"\d*", x).group(0) or 0)

    c["done"], c["chosen"], c["inferred"], c["pending"] = num(row[2]), num(row[3]), num(row[5]), num(row[6])
    c["partial"] = num(row[4])
    m = re.search(r"\(([\d.]+) done(?:, (\d+) not measured)?\)", row[4])
    if m:
        c["partialDone"] = float(m.group(1))
        c["partialMeasured"] = c["partial"] - int(m.group(2) or 0)
    return c


def features() -> dict:
    domains, order, current = {}, [], None
    for line in INDEX.read_text(encoding="utf-8").splitlines():
        m = re.match(r"## \[([^\]]+)\]\([^)]*\)(.*)", line)
        if m:
            current = m.group(1)
            order.append(current)
            # the fixed <a id="<domain>"> that progress_index_regen.py writes above each heading
            domains[current] = {"name": current, "title": display_name(current), "anchor": current, "files": 0,
                                "fromIndex": 0, **dict.fromkeys(COUNTED, 0)}
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
            c = index_row(row)
        for k in COUNTED:
            d[k] += c[k]
    overall = {k: sum(d[k] for d in domains.values()) for k in (*COUNTED, "files", "fromIndex")}
    for d in (*domains.values(), overall):
        d["score"] = score(d)
        d["rows"] = rows.counted(d)
    ranked = sorted((domains[n] for n in order), key=lambda d: (-d["score"], order.index(d["name"])))
    # a domain with no feature rows is left out; one that logs the original's bugs gets its own line
    logs = [d for d in ranked if not d["rows"] and d["bugs"]]
    return {"overall": overall, "domains": [d for d in ranked if d["rows"]], "bugLogs": logs}


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
        metrics.append({"what": row[0], "short": get("short name"), "before": get("before"), "now": get("now"),
                        "done": done, "pct": pct, "left": left, "exception": exception(pct, left)})
    return {"metrics": metrics}


TESTS = ROOT.parent / "test"
TEST_MACRO = re.compile(r"^\s*(?:TEST|TEST_F|TEST_P|TYPED_TEST|TYPED_TEST_P)\(", re.M)
TEST_EXE = re.compile(r"^(?:openblack_setup_test_group|add_executable)\(", re.M)


def test_counts() -> dict | None:
    """The test sources under test/: files with a test, test cases written (gtest macros) and executables built."""
    if not TESTS.is_dir():
        return None
    files = cases = 0
    for path in sorted(TESTS.rglob("*")):
        if path.suffix in (".cpp", ".h") and path.is_file():
            n = len(TEST_MACRO.findall(path.read_text(encoding="utf-8", errors="replace")))
            files += n > 0
            cases += n
    cmake = TESTS / "CMakeLists.txt"
    exes = len(TEST_EXE.findall(cmake.read_text(encoding="utf-8"))) if cmake.is_file() else 0
    return {"files": files, "cases": cases, "executables": exes}


def link_test_row() -> bool:
    """Write the counted test sources into the "Test files" row of the refactor table. How many tests run and pass is
    not kept in the repository (each test run's log has it), so the row gives none."""
    t = test_counts()
    if not t:
        return False
    text = REFACTOR.read_text(encoding="utf-8")
    row = (f"| Test files | 151 | {t['files']} | — | {t['cases']} test cases in {t['files']} files under `test/`, built "
           f"into {t['executables']} executables; 46 tests gained a synthetic twin. Counted from the sources by "
           "`docs/tools/progress_stats.py`; how many pass is in each test run's log |")
    new, n = re.subn(r"(?m)^\| Test files \|.*\|$", lambda _: row, text, count=1)
    return bool(n) and write_if_changed(REFACTOR, new)

# ---------- raffclar's work in our tree ----------


STATUS_TSV = PROJECT / "raffclar-status.tsv"
STATUS_MD = PROJECT / "raffclar-status.md"
STATUS_START, STATUS_END = "<!-- raffclar-status:static -->", "<!-- /raffclar-status:static -->"
RAFF_STATUSES = ("in", "partial", "pending", "n/a")
RAFFCLAR_PR = "https://github.com/raffclar/openblack/pull/{n}"
# Why a pull request is not taken (n/a), from the start of its evidence; the first match names it. The viewer's
# NA_GROUPS is the same list.
NA_GROUPS = [
    ("his inspector and MCP adapter, parked by decision C-1", re.compile(r"inspector", re.I)),
    ("his own docs pages", re.compile(r"^docs only", re.I)),
    ("reverts", re.compile(r"revert", re.I)),
    ("formatting", re.compile(r"^formatting", re.I)),
    ("CI and build tooling", re.compile(r".")),
]


def raffclar_status() -> dict:
    """The rows of raffclar-status.tsv, their counts and the measure: (in + half of partial) / (all - n/a)."""
    rows = []
    if STATUS_TSV.is_file():
        for line in STATUS_TSV.read_text(encoding="utf-8").splitlines():
            if not line.strip() or line.startswith("#") or line.startswith("pr\t"):
                continue
            cols = (line.split("\t") + [""] * 5)[:5]
            rows.append(dict(zip(("pr", "title", "date", "status", "evidence"), cols)))
    counts = {k: sum(1 for r in rows if r["status"] == k) for k in RAFF_STATUSES}
    counts["other"] = len(rows) - sum(counts[k] for k in RAFF_STATUSES)
    counts["all"] = len(rows)
    counted = len(rows) - counts["n/a"]
    reasons = {}
    for r in rows:
        if r["status"] == "n/a":
            label = next(lab for lab, rx in NA_GROUPS if rx.search(r["evidence"]))
            reasons[label] = reasons.get(label, 0) + 1
    return {"rows": rows, "counts": counts, "counted": counted,
            "score": round(100 * (counts["in"] + 0.5 * counts["partial"]) / counted) if counted else 0,
            "naReasons": sorted(reasons.items(), key=lambda x: -x[1])}


def raffclar(repo: str) -> dict:
    """His master from a local clone with his master at refs/remotes/raffclar/master: the tip, the base on upstream
    openblack, and the pull requests merged since that base (number, date, title)."""
    git = lambda *args: subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True, check=True,
                                       encoding="utf-8", errors="replace").stdout.strip()
    base = git("merge-base", RAFFCLAR_REF, UPSTREAM_REF)
    head, head_date = git("log", "-1", "--format=%H %cI", RAFFCLAR_REF).split()
    prs = {}
    for rec in git("log", "--merges", "--format=%x1e%cs%x00%s%x00%b", f"{base}..{RAFFCLAR_REF}").split("\x1e"):
        if "\x00" not in rec:
            continue
        date, subj, body = rec.strip("\n").split("\x00", 2)
        m = re.match(r"Merge pull request #(\d+) from (\S+)", subj)
        if m:
            title = next((x.strip() for x in body.splitlines() if x.strip()), m.group(2))
            title = re.sub(r"^batch [a-z]+ \((.+)\)$", r"\1", re.sub(r"^Merge train \d+: ", "", title))
            prs[int(m.group(1))] = (date, title.replace("\t", " ")[:160])
    return {"base": base[:8], "head": head[:8], "headDate": head_date[:10], "computed": datetime.date.today().isoformat(),
            "pullRequests": len(prs), "prs": prs}


def add_new_prs(prs: dict) -> bool:
    """Append the pull requests of his master that the table does not have yet, as pending."""
    if not STATUS_TSV.is_file():
        return False
    text = STATUS_TSV.read_text(encoding="utf-8")
    known = {line.split("\t", 1)[0] for line in text.splitlines()}
    new = [f"{n}\t{t}\t{d}\tpending\tnew on his master; not classified yet" for n, (d, t) in sorted(prs.items())
           if str(n) not in known]
    if not new:
        return False
    lines = text.rstrip("\n").split("\n")
    # keep the direct-commit item last
    tail = [x for x in lines if x.startswith("direct\t")]
    body = [x for x in lines if not x.startswith("direct\t")]
    return write_if_changed(STATUS_TSV, "\n".join(body + new + tail) + "\n")


def load_raffclar() -> dict | None:
    return json.loads(RAFFCLAR_OUT.read_text(encoding="utf-8")) if RAFFCLAR_OUT.is_file() else None


def pr_cell(pr: str) -> str:
    return f"[#{pr}]({RAFFCLAR_PR.format(n=pr)})" if pr.isdigit() else "—"


def na_line(rs: dict) -> str:
    return "; ".join(f"{label} ({n})" for label, n in rs["naReasons"])


def status_section(rs: dict, rf: dict | None, bars: "Bars") -> str:
    """The generated part of raffclar-status.md: the measure, the counts and the list by status."""
    c = rs["counts"]
    tip = f" at `{rf['head']}` ({rf['headDate']})" if rf else ""
    out = [STATUS_START, "", raffclar_bar(rs, bars, prefix=""), "",
           f"**{rs['score']}%** of the pull requests on his master{tip} are in our tree: "
           f"(in + half of partial) / (all − n/a) = ({c['in']} + {c['partial']}/2) / ({c['all']} − {c['n/a']}).", "",
           "| Status | Pull requests | What it means |", "|---|---:|---|",
           f"| In | {c['in']} | Ported, or our tree has the same behaviour |",
           f"| Partial | {c['partial']} | Some of it is in our tree |",
           f"| Pending | {c['pending']} | Not in our tree yet, or not classified yet |",
           f"| n/a | {c['n/a']} | Not taken: {na_line(rs)} |",
           f"| **All** | **{c['all']}** | |"]
    if c["other"]:
        out.append(f"\n{c['other']} rows have another status; fix them in raffclar-status.tsv.")
    names = {"partial": "Partial", "pending": "Pending", "in": "In", "n/a": "Not taken (n/a)"}
    for st in ("partial", "pending", "in", "n/a"):
        rows = [r for r in rs["rows"] if r["status"] == st]
        out += ["", f'<a id="{bar_slug(names[st])}"></a>', "", f"### {names[st]} ({len(rows)})", "",
                "| PR | Title | Merged | Evidence |", "|---|---|---|---|"]
        esc = lambda s: s.replace("|", "\\|")
        out += [f"| {pr_cell(r['pr'])} | {esc(r['title'])} | {r['date']} | {esc(r['evidence'])} |" for r in rows]
    out += ["", STATUS_END]
    return "\n".join(out)


def write_status_page(rs: dict, rf: dict | None, bars: "Bars") -> bool:
    if not STATUS_MD.is_file():
        return False
    text = STATUS_MD.read_text(encoding="utf-8")
    if STATUS_START not in text or STATUS_END not in text:
        return False
    before, _, rest = text.partition(STATUS_START)
    _, _, after = rest.partition(STATUS_END)
    return write_if_changed(STATUS_MD, before + status_section(rs, rf, bars) + after)


# ---------- milestones ----------


MILESTONES = PROJECT / "milestones.md"


def milestones() -> list[dict]:
    """The rows of milestones.md's table, newest first: date, the milestone (a link into the page) and its commit."""
    if not MILESTONES.is_file():
        return []
    text = MILESTONES.read_text(encoding="utf-8")
    table = text.split("<!-- milestones:table -->", 1)[-1].split("<!-- /milestones:table -->", 1)[0]
    out = []
    for line in table.splitlines():
        row = cells(line) if line.startswith("|") else []
        if len(row) >= 3 and re.fullmatch(r"\d{4}-\d{2}-\d{2}", row[0]):
            out.append({"date": row[0], "what": row[1], "commit": row[2]})
    return sorted(out, key=lambda m: m["date"], reverse=True)


def milestone_lines(ms: list[dict], n: int) -> list[str]:
    """The latest n milestones as list lines; the links point into milestones.md."""
    fix = lambda s: re.sub(r"\]\(#", "](milestones.md#", s)
    return [f"- **{m['date']}** · {fix(m['what'])}" for m in ms[:n]]


# ---------- issues ----------


def own_label(domain: str) -> str:
    """A progress domain's own label on the fork."""
    return f"area:{domain}"


def domain_labels(labels: dict[str, list[int]]) -> dict[str, list[str]]:
    """The labels each progress domain's issue link and count use: its own label, and also each older label of
    DOMAIN_LABELS that an open issue carries without any domain's own label (the fallback until it is sorted)."""
    domains = sorted({*DOMAIN_NAMES, *DOMAIN_LABELS})
    sorted_issues = {n for d in domains for n in labels.get(own_label(d), [])}
    return {d: [own_label(d), *(lab for lab in DOMAIN_LABELS.get(d, [])
                                if lab != own_label(d) and set(labels.get(lab, [])) - sorted_issues)]
            for d in domains}


def issues(source: Path | None) -> dict:
    if source is None:
        if ISSUES_OUT.is_file():
            # the issue numbers stay as last exported; the bar-to-label table and the names come from this script
            last = json.loads(ISSUES_OUT.read_text(encoding="utf-8"))
            return {**last, "domains": domain_labels(last.get("labels", {})), "areas": AREA_LABELS,
                    "names": DOMAIN_NAMES, "families": FAMILIES}
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
            "domains": domain_labels(labels), "areas": AREA_LABELS, "names": DOMAIN_NAMES, "families": FAMILIES}


def issues_query(label_list: list[str]) -> str:
    return f"{FORK}/issues?q=is:issue+is:open+label:{','.join(label_list)}"


def issue_count(iss: dict, label_list: list[str]) -> int:
    return len({n for lab in label_list for n in iss["labels"].get(lab, [])})


# ---------- static Markdown ----------


# The domains grouped into families, in the order the dashboard shows them; a domain in none of them goes to "Other".
FAMILIES = [
    {"name": "World", "icon": "🌍",
     "domains": ["terrain", "sky", "ocean", "weather", "nature", "physics", "resources"]},
    {"name": "Beings", "icon": "🐾", "domains": ["creature", "villager", "animal", "town", "building", "worship"]},
    {"name": "Player and interface", "icon": "✋",
     "domains": ["hand", "gesture", "miracles", "camera", "interface", "temple", "multiplayer"]},
    {"name": "Story", "icon": "📜", "domains": ["story", "scripts", "rival_gods", "easter_eggs"]},
    {"name": "Engine", "icon": "⚙️", "domains": ["engine", "rendering", "audio", "video", "debug", "pc_integration"]},
]
OTHER_FAMILY = {"name": "Other", "icon": "📦", "domains": []}
# The section icons: the features, raffclar's work in our tree and the code clean-up.
ICON_FEATURES, ICON_UNIFY, ICON_REFACTOR = "🎮", "🔀", "🧹"

# Coloured squares, the text fallback of every bar (its alt text): done, chosen, the partial rows' share, inferred,
# pending, and what stays on purpose.
DONE, CHOSEN, PARTIAL, INFERRED, PENDING, KEPT = "🟩", "🟢", "🟨", "🟪", "⬜", "🟧"
LEGEND = (f"{DONE} done · {CHOSEN} chosen (done the way the user chose) · {PARTIAL} partial (the share done) · "
          f"{INFERRED} inferred (not checked against the original; counts 0) · {PENDING} pending")

# The bars are small SVG files in wiki/project/bars, which GitHub draws as images: a rounded bar with the done, partial
# and pending (or kept) parts in colour and the percentage inside. The colours read on light and dark pages alike:
# the pending part is a see-through grey, light on a light page and dark on a dark one.
BARS = PROJECT / "bars"
BAR_SMALL, BAR_BIG = (160, 18, 11), (360, 26, 14)  # width, height, font size
SEG_FILL = {"done": "#2da44e", "chosen": "#116329", "partial": "url(#p)", "inferred": "#a371f7", "kept": "url(#k)"}
PATTERNS = {
    "partial": '<pattern id="p" width="6" height="6" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">'
               '<rect width="6" height="6" fill="#d4a72c"/><rect width="3" height="6" fill="#eac54f"/></pattern>',
    "kept": '<pattern id="k" width="6" height="6" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">'
            '<rect width="6" height="6" fill="#e16f24"/><rect width="3" height="6" fill="#fb8f44"/></pattern>',
}


SQUARES = {"done": DONE, "chosen": CHOSEN, "partial": PARTIAL, "inferred": INFERRED, "kept": KEPT}


def text_bar(parts: list[tuple[str, float]], width: int = 10, rest: str = PENDING) -> str:
    """A row of 10 squares, each part rounded on the running total: the alt text of a bar."""
    out, total, used = "", 0.0, 0
    for kind, frac in parts:
        total += frac
        n = max(0, min(width - used, round(total * width) - used))
        out += SQUARES[kind] * n
        used += n
    return out + rest * (width - used)


def svg_bar(parts: list[tuple[str, float]], label: str, title: str, size: tuple[int, int, int]) -> str:
    """The SVG text of one bar; parts are (kind, fraction) in order, the rest of the bar is pending. Deterministic:
    the same numbers always give the same bytes."""
    w, h, font = size
    used = sorted({k for k, f in parts if f > 0 and k in PATTERNS})
    rects, x = [], 0.0
    for kind, frac in parts:
        if frac <= 0:
            continue
        seg = min(w - x, frac * w)
        rects.append(f'<rect x="{x:.1f}" width="{seg:.1f}" height="{h}" fill="{SEG_FILL[kind]}"/>')
        x += seg
    r = h / 2
    esc = lambda s: s.replace("&", "&amp;").replace("<", "&lt;").replace('"', "&quot;")
    return "\n".join([
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" role="img" '
        f'aria-label="{esc(title)}">',
        f"<title>{esc(title)}</title>",
        "<style>.t{fill:#8c959f;fill-opacity:.32}.e{fill:none;stroke:#8c959f;stroke-opacity:.7}"
        f".l{{font:700 {font}px -apple-system,'Segoe UI',Helvetica,Arial,sans-serif;fill:#fff;stroke:#1f2328;"
        "stroke-opacity:.65;stroke-width:3px;stroke-linejoin:round;paint-order:stroke}"
        "</style>",
        f'<defs><clipPath id="c"><rect width="{w}" height="{h}" rx="{r:g}"/></clipPath>'
        + "".join(PATTERNS[k] for k in used) + "</defs>",
        f'<g clip-path="url(#c)"><rect class="t" width="{w}" height="{h}"/>' + "".join(rects) + "</g>",
        f'<rect class="e" x=".5" y=".5" width="{w - 1}" height="{h - 1}" rx="{r - .5:g}"/>',
        f'<text class="l" x="{w / 2:g}" y="{h / 2 + font * .36:.1f}" text-anchor="middle">{esc(label)}</text>',
        "</svg>", ""])


class Bars:
    """The bars a run draws: each one a file of wiki/project/bars, linked from the page as an image."""

    def __init__(self):
        self.files: dict[str, str] = {}

    def image(self, name: str, parts: list[tuple[str, float]], label: str, title: str, big: bool = False,
              rest: str = PENDING, prefix: str = "") -> str:
        """The Markdown image of a bar (its alt text the squares and the label), and the file it needs. The link ends in
        a version query, a short hash of the file, so that a browser that keeps the old image fetches the new one."""
        svg = svg_bar(parts, label, title, BAR_BIG if big else BAR_SMALL)
        self.files[f"{name}.svg"] = svg
        version = hashlib.sha1(svg.encode("utf-8")).hexdigest()[:8]
        shown = [(k, f) for k, f in parts if not (k == "kept" and rest == KEPT)]
        return f"![{text_bar(shown, rest=rest)} {label}]({prefix}bars/{name}.svg?v={version})"

    def write(self) -> list[Path]:
        """Write the bars that changed and remove the ones no longer drawn; returns the files touched."""
        BARS.mkdir(exist_ok=True)
        touched = [BARS / n for n, text in sorted(self.files.items()) if write_if_changed(BARS / n, text)]
        for old in sorted(BARS.glob("*.svg")):
            if old.name not in self.files:
                old.unlink()
                touched.append(old)
        return touched


def bar_slug(text: str) -> str:
    """A bar's file name from a readable name: lower case, letters and digits joined by hyphens, cut after a whole
    word to at most 40 characters."""
    s = re.sub(r"[^a-z0-9]+", "-", text.lower().replace("'", "")).strip("-")
    return s if len(s) <= 40 else s[:41].rsplit("-", 1)[0]


def issues_cell(iss: dict, label_list: list[str] | None) -> str:
    if not label_list:
        return ""
    n = issue_count(iss, label_list)
    return f"[{n} open]({issues_query(label_list)})" if n else ""


def pct(n: float, d: float) -> int:
    return round(100 * n / d) if d else 0


def active(c: dict) -> int:
    return rows.counted(c)


def feature_parts(c: dict) -> list[tuple[str, float]]:
    a = active(c) or 1
    return [("done", c["done"] / a), ("chosen", c["chosen"] / a), ("partial", c["partialDone"] / a),
            ("inferred", c["inferred"] / a)]


def partial_text(c: dict) -> str:
    """The partial rows, the share they add up to and how many are not measured yet."""
    left = c["partial"] - c["partialMeasured"]
    return (f"{c['partial']} partial ({pct(c['partialDone'], active(c))}% of the rows' worth"
            + (f", {left} not measured yet)" if left else ")"))


def feature_title(c: dict) -> str:
    return (f"{c['score']}%: {c['done']} done, {c['chosen']} chosen, {partial_text(c)}, {c['inferred']} inferred, "
            f"{c['pending']} pending")


def partial_cell(c: dict) -> str:
    """A table's partial cell: the rows and, when some are measured, the share they add up to."""
    if not c["partialMeasured"]:
        return f"{c['partial']}"
    return f"{c['partial']} ({c['partialDone']:.1f} done)"


def families(f: dict) -> list[dict]:
    """The families with their domains (best score first) and their summed rows; empty families are left out."""
    known = {d for fam in FAMILIES for d in fam["domains"]}
    out = []
    for fam in [*FAMILIES, OTHER_FAMILY]:
        ds = [d for d in f["domains"] if (d["name"] in fam["domains"] if fam is not OTHER_FAMILY
                                          else d["name"] not in known)]
        if not ds:
            continue
        total = {k: sum(d[k] for d in ds) for k in rows.KEYS}
        total["score"] = score(total)
        out.append({**fam, "members": ds, "total": total, "slug": bar_slug(fam["name"])})
    return out


def goals_of(r: dict) -> dict:
    """The refactor's goals (measures with a percentage): how many are reached and how many have only what stays on
    purpose left."""
    goals = [m for m in r["metrics"] if m["pct"] is not None]
    return {"goals": goals, "n": len(goals), "reached": sum(1 for m in goals if m["pct"] >= 100),
            "kept": sum(1 for m in goals if m["exception"] == "all")}


def raffclar_bar(rs: dict, bars: Bars, prefix: str = "") -> str:
    c, n = rs["counts"], rs["counted"] or 1
    return bars.image("raffclar-all", [("done", c["in"] / n), ("partial", c["partial"] / n)], f"{rs['score']}%",
                      f"{rs['score']}%: {c['in']} in, {c['partial']} partial, {c['pending']} pending of {rs['counted']} "
                      f"pull requests ({c['n/a']} n/a)", big=True, prefix=prefix)


def cleanup_bar(r: dict, bars: Bars, prefix: str = "") -> str:
    g = goals_of(r)
    n = g["n"] or 1
    return bars.image("refactor-all", [("done", g["reached"] / n), ("kept", g["kept"] / n)], f"{g['reached']} of {g['n']}",
                      f"{g['reached']} of {g['n']} code goals reached, {g['kept']} more with only what stays on purpose "
                      "left", big=True, prefix=prefix)


def headline(f: dict, rs: dict, r: dict, bars: Bars, prefix: str = "") -> list[str]:
    """The top of the page: three numbers, each a big bar and one plain line. The viewer's headline() draws the same."""
    o, c, g = f["overall"], rs["counts"], goals_of(r)
    return [f"**{ICON_FEATURES} Features of the original**", "",
            bars.image("features-all", feature_parts(o), f"{o['score']}%", feature_title(o), big=True, prefix=prefix), "",
            f"{o['done']} of the original game's {active(o)} behaviours are done and {o['chosen']} done as the user "
            f"chose; {partial_text(o)}; {o['inferred']} inferred, not yet checked against the original.", "",
            f"**{ICON_UNIFY} raffclar's work in our tree**", "",
            raffclar_bar(rs, bars, prefix), "",
            f"{c['in']} of his {rs['counted']} pull requests are in our tree, {c['partial']} more in part.", "",
            f"**{ICON_REFACTOR} Code clean-up**", "",
            cleanup_bar(r, bars, prefix), "",
            f"{g['reached']} of the {g['n']} code goals are reached; in {g['kept']} more, what is left stays on purpose."]


def static_section(f: dict, rs: dict, r: dict, iss: dict, rf: dict | None, ms: list[dict], bars: Bars) -> str:
    o, c = f["overall"], rs["counts"]
    out = [START, "", '<a id="where-we-are"></a>', "", "## 📊 Where we are", "", *headline(f, rs, r, bars), "",
           '<a id="features-of-the-original"></a>', "", f"## {ICON_FEATURES} Features of the original", "",
           "Each behaviour of the original game is one row of a [progress file](progress/INDEX.md): done, chosen (done "
           "the way the user chose), partial with the share done, inferred (written from an inference, not checked "
           "against the original) or pending. The score counts done and chosen as one, a partial row as its share, "
           "inferred and pending as nothing.", "", LEGEND, "",
           "| Family | Progress | Done | Chosen | Partial | Inferred | Pending |", "|---|---|---:|---:|---:|---:|---:|"]
    fams = families(f)
    for fam in fams:
        t = fam["total"]
        bar = bars.image(f"family-{fam['slug']}", feature_parts(t), f"{t['score']}%", feature_title(t))
        out.append(f"| {fam['icon']} {fam['name']} | {bar} | {t['done']} | {t['chosen']} | {partial_cell(t)} | "
                   f"{t['inferred']} | {t['pending']} |")
    out += ["", "<details><summary>Every domain</summary>", "",
            "| Domain | Progress | Done | Chosen | Partial | Inferred | Pending | Issues |",
            "|---|---|---:|---:|---:|---:|---:|---:|"]
    for fam in fams:
        for d in fam["members"]:
            bar = bars.image(f"features-{bar_slug(d['name'])}", feature_parts(d), f"{d['score']}%", feature_title(d))
            out.append(f"| [{d['title']}](progress/INDEX.md#{d['anchor']}) | {bar} | {d['done']} | {d['chosen']} | "
                       f"{partial_cell(d)} | {d['inferred']} | {d['pending']} | "
                       f"{issues_cell(iss, iss['domains'].get(d['name'], [own_label(d['name'])]))} |")
    for d in f["bugLogs"]:
        readme_md = PROGRESS / d["name"] / "README.md"
        page = f"progress/{d['name']}/README.md" if readme_md.is_file() else f"progress/INDEX.md#{d['anchor']}"
        out += ["", f"🐞 [{d['title']}]({page}): {d['bugs']} bugs of the original logged (not features, so no score)."]
    out += ["", "</details>", ""]
    tip = f" (`{rf['head']}`, {rf['headDate']})" if rf else ""
    out += ['<a id="unification-with-raffclars-tree"></a>', '<a id="raffclars-work-in-our-tree"></a>', "",
            f"## {ICON_UNIFY} raffclar's work in our tree", "",
            f"Every pull request merged into his master{tip}, checked against our tree.", "",
            raffclar_bar(rs, bars), "",
            f"**{c['in']}** in · **{c['partial']}** partial · **{c['pending']}** pending · **{c['n/a']}** n/a, not "
            f"taken: {na_line(rs)}.", "",
            "The list, pull request by pull request: [raffclar-status.md](raffclar-status.md).", "",
            '<a id="refactor"></a>', "", f"## {ICON_REFACTOR} Code clean-up", "",
            "How far the code follows the [code conventions](refactor-conventions.md#progress-counts); the notes on "
            "each goal are there.", "",
            f"{DONE} done · {KEPT} left on purpose · {PENDING} pending", "",
            "| Goal | Progress |", "|---|---|"]
    counts = []
    for m in r["metrics"]:
        name = m.get("short") or m["what"]
        if m["pct"] is None:
            counts.append(f"{name} {m['before']} → {m['now']}")
            continue
        kept = m["exception"] == "all"
        parts = [("done", m["pct"] / 100)] + ([("kept", 1 - m["pct"] / 100)] if kept else [])
        title = f"{m['done']} done" + (", the rest stays on purpose" if kept else "")
        bar = bars.image(f"refactor-{bar_slug(m['what'])}", parts, m["done"], title, rest=KEPT if kept else PENDING)
        out.append(f"| [{name}](refactor-conventions.md#progress-counts) | {bar} |")
    if counts:
        out += ["", "Also counted, with no goal: " + "; ".join(counts) + "."]
    if ms:
        out += ["", '<a id="milestones"></a>', "", "## 🏁 Milestones", "", *milestone_lines(ms, 6), "",
                "[All milestones](milestones.md)."]
    out += ["", END]
    return "\n".join(out)


README = PROJECT.parent.parent / "README.md"  # docs/README.md: what GitHub shows when the docs folder is opened


def reroot(text: str, base: str) -> str:
    """Every relative link and image of a section written for wiki/project, made relative to another folder."""
    import posixpath

    def one(m):
        target = m.group(1)
        if re.match(r"[a-z]+:|#", target):
            return m.group(0)
        return f"]({posixpath.normpath(posixpath.join(base, target))})"

    return re.sub(r"\]\(([^)]+)\)", one, text)


def readme(static: str) -> str:
    """docs/README.md for GitHub: how to read the wiki, then the dashboard's static copy with its links re-rooted."""
    body = reroot(static.replace(START, "").replace(END, ""), "wiki/project")
    return ("# openblack wiki\n\n"
            "How the original Black & White (v1.42) behaves, with addresses and formulas, and what openblack does.\n\n"
            "- **Read it:** open `docs/index.html` through `docs/open_wiki.bat` (Windows), or browse "
            "[the pages](SUMMARY.md) here.\n"
            "- **Start:** the [progress dashboard](wiki/project/dashboard.md) (live bars in the viewer) and "
            "[Reading this wiki](wiki/project/reading-the-wiki.md).\n\n"
            "*This page is written by `tools/progress_stats.py`.*\n" + body.rstrip() + "\n")

# the repository's README (tracked as readme.md), the fork's front page on GitHub; found by name in any case
ROOT_README = next((q for q in sorted(ROOT.parent.iterdir()) if q.name.lower() == "readme.md"), ROOT.parent / "README.md")
ROOT_START, ROOT_END = "<!-- progress:start -->", "<!-- progress:end -->"

def root_block(f: dict, rs: dict, r: dict, ms: list[dict], bars: Bars) -> str:
    """The progress block of the repository's README.md: the three bars, the latest milestones and a link to the
    dashboard, with the same SVG files as the dashboard."""
    out = [ROOT_START, "", "### 📊 Progress of the reconstruction", "", *headline(f, rs, r, bars), ""]
    if ms:
        out += ["**🏁 Latest milestones**", "", *milestone_lines(ms, 5), ""]
    out += ["The detail is on the [progress dashboard](dashboard.md). *This block is written by "
            "`docs/tools/progress_stats.py`.*", "", ROOT_END]
    return reroot("\n".join(out), "docs/wiki/project")


def write_root_readme(block: str) -> bool:
    """Replace the block between the progress markers of the repository's README.md; nothing else of it changes."""
    if not ROOT_README.is_file():
        return False
    text = ROOT_README.read_text(encoding="utf-8")
    if ROOT_START not in text or ROOT_END not in text:
        return False
    before, _, rest = text.partition(ROOT_START)
    _, _, after = rest.partition(ROOT_END)
    return write_if_changed(ROOT_README, before + block + after)


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
    if link_test_row():
        changed.append(REFACTOR)
    if "--raffclar" in sys.argv[1:]:
        at = sys.argv.index("--raffclar")
        if at + 1 >= len(sys.argv):
            print("--raffclar needs the path of a clone with the raffclar and origin remotes", file=sys.stderr)
            return 2
        his = raffclar(sys.argv[at + 1])
        if add_new_prs(his.pop("prs")):
            changed.append(STATUS_TSV)
        if write_if_changed(RAFFCLAR_OUT, json.dumps(his, indent=1) + "\n"):
            changed.append(RAFFCLAR_OUT)
    rf = load_raffclar()
    f, p, r, rs, ms = features(), port(), refactor(), raffclar_status(), milestones()
    text = DASHBOARD.read_text(encoding="utf-8")
    if START not in text or END not in text:
        print(f"{DASHBOARD} has no {START} ... {END} section", file=sys.stderr)
        return 1
    before, _, after = text.partition(START)
    _, _, after = after.partition(END)
    bars = Bars()
    static = static_section(f, rs, r, iss, rf, ms, bars)
    root = root_block(f, rs, r, ms, bars)
    if write_status_page(rs, rf, bars):
        changed.append(STATUS_MD)
    changed += bars.write()
    if write_root_readme(root):
        changed.append(ROOT_README)
    if write_if_changed(DASHBOARD, before + static + after):
        changed.append(DASHBOARD)
    if write_if_changed(README, readme(static)):
        changed.append(README)
    for problem in p["problems"]:
        print("port-audit.md:", problem, file=sys.stderr)
    if rs["counts"]["other"]:
        print(f"raffclar-status.tsv: {rs['counts']['other']} rows with a status other than "
              f"{', '.join(RAFF_STATUSES)}", file=sys.stderr)
    if "--json" in sys.argv[1:]:
        print(json.dumps({"features": f, "raffclar": {k: v for k, v in rs.items() if k != "rows"}, "refactor": r,
                          "raffclarTip": rf, "milestones": ms}, indent=1, ensure_ascii=False))
    else:
        o, c = f["overall"], rs["counts"]
        print(f"features {o['score']}%: {o['done']} done, {o['chosen']} chosen, {o['partial']} partial "
              f"({o['partialMeasured']} measured, {o['partialDone']:.1f} done), {o['inferred']} inferred, "
              f"{o['pending']} pending, {o['n/a']} n/a, "
              f"{o['other']} other, {o['files']} files ({o['fromIndex']} from INDEX); raffclar {rs['score']}%: "
              f"{c['in']} in, {c['partial']} partial, {c['pending']} pending, {c['n/a']} n/a of {c['all']}; "
              f"refactor {len(r['metrics'])} metrics; {len(ms)} milestones")
        for path in changed:
            print("wrote", path.relative_to(ROOT.parent))
    return 1 if p["problems"] or rs["counts"]["other"] else 0


if __name__ == "__main__":
    sys.exit(main())
