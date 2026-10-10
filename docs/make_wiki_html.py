"""Builds the HTML wiki from the Markdown pages in docs/bw1-notes, and the progress notes in docs/progress.

    python docs/make_wiki_html.py [--src docs/bw1-notes] [--out docs/wiki-html] [--progress docs/progress]
                                  [--no-progress]   (from the repository root)

Writes one HTML file per Markdown page, plus index.html, wiki.css and wiki.js, and a search index. The progress
notes, sub-folders included, go to <out>/progress/ with the same layout; each folder gets an index.html (its README,
or a generated list of its files). The sidebar gets a Progress entry and the search covers the progress pages too.
With --no-progress (or no progress folder) the output is the bw1-notes wiki alone, as before.
Needs Python 3.9+ and the `markdown` package (pip install markdown). The generated files are meant to be
committed, so a reader does not have to build anything.
"""

import argparse
import html
import json
import os
import re
import shutil

import markdown

EXTENSIONS = ["tables", "fenced_code", "toc", "sane_lists", "attr_list", "md_in_html"]


def github_slug(value, separator):
    """The anchor GitHub gives a heading, so that links written for GitHub work in the HTML pages too."""
    text = re.sub(r"[^\w\- ]", "", value.strip().lower().replace("`", ""), flags=re.U)
    return text.replace(" ", separator)


EXTENSION_CONFIGS = {"toc": {"slugify": github_slug}}

# The order pages appear in the sidebar. Anything not listed goes under "Other", alphabetically.
GROUPS = [
    ("Start here", ["README"]),
    ("Engine", ["openblack-internals", "engine-loop", "engine-math", "tooling", "parity", "original-frame"]),
    ("World", ["map-loading", "land-script-save", "water", "trees", "buildings", "objects-and-resources", "physics"]),
    ("Life", ["villagers", "animals"]),
    ("Magic and weather", ["magic", "miracles", "particles", "day-night-weather", "vortex"]),
    ("Hand and interface", ["hand-and-interface", "intro", "script-camera", "camera-tracks"]),
    ("Presentation", ["rendering", "rendering-objects", "animation", "audio", "video"]),
    ("Data", ["mods"]),
]

CSS = """
:root {
  --bg: #fdfdfc; --fg: #1f2328; --muted: #656d76; --line: #d8dee4; --accent: #0969da;
  --code-bg: #f6f8fa; --side-bg: #f6f8fa; --mark: #fff8c5;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #0d1117; --fg: #e6edf3; --muted: #9198a1; --line: #30363d; --accent: #4493f8;
    --code-bg: #161b22; --side-bg: #161b22; --mark: #3f2e00;
  }
}
* { box-sizing: border-box; }
body {
  margin: 0; background: var(--bg); color: var(--fg); display: flex; min-height: 100vh;
  font: 16px/1.6 -apple-system, "Segoe UI", system-ui, sans-serif;
}
#side {
  width: 19rem; flex: 0 0 19rem; background: var(--side-bg); border-right: 1px solid var(--line);
  padding: 1.25rem 1rem; overflow-y: auto; height: 100vh; position: sticky; top: 0;
}
#side h1 { font-size: 1.05rem; margin: 0 0 .25rem; }
#side .tag { color: var(--muted); font-size: .8rem; margin-bottom: 1rem; display: block; }
#side h2 { font-size: .72rem; text-transform: uppercase; letter-spacing: .06em; color: var(--muted);
  margin: 1.2rem 0 .35rem; }
#side a { display: block; padding: .18rem .4rem; border-radius: 6px; color: var(--fg); text-decoration: none; }
#side a:hover { background: var(--line); }
#side a.here { background: var(--accent); color: #fff; }
#q { width: 100%; padding: .45rem .6rem; border: 1px solid var(--line); border-radius: 6px;
  background: var(--bg); color: var(--fg); font-size: .9rem; }
#hits { margin: .5rem 0 0; font-size: .88rem; }
#hits .hit { padding: .3rem .4rem; border-radius: 6px; }
#hits .hit b { display: block; }
#hits .hit span { color: var(--muted); }
#hits mark { background: var(--mark); color: inherit; }
main { flex: 1; min-width: 0; padding: 2.2rem clamp(1rem, 4vw, 4rem); max-width: 60rem; }
main h1 { margin-top: 0; }
main h2 { border-bottom: 1px solid var(--line); padding-bottom: .25rem; margin-top: 2.2rem; }
a { color: var(--accent); }
code, pre { font-family: ui-monospace, "Cascadia Mono", Consolas, monospace; font-size: .88em; }
code { background: var(--code-bg); padding: .1rem .3rem; border-radius: 5px; }
pre { background: var(--code-bg); padding: .8rem 1rem; border-radius: 8px; overflow-x: auto; }
pre code { background: none; padding: 0; }
table { border-collapse: collapse; display: block; overflow-x: auto; max-width: 100%; }
th, td { border: 1px solid var(--line); padding: .4rem .6rem; text-align: left; vertical-align: top; }
th { background: var(--code-bg); }
blockquote { margin: 1rem 0; padding: .1rem 1rem; border-left: 4px solid var(--line); color: var(--muted); }
img { max-width: 100%; }
.state { display: inline-block; font-size: .72rem; font-weight: 600; text-transform: uppercase;
  letter-spacing: .04em; padding: .1rem .45rem; border-radius: 999px; border: 1px solid var(--line);
  color: var(--muted); vertical-align: middle; margin-left: .4rem; }
.state.pending { border-color: #bf8700; color: #bf8700; }
.state.approx { border-color: var(--accent); color: var(--accent); }
#toc { border: 1px solid var(--line); border-radius: 8px; padding: .6rem 1rem; margin: 1.5rem 0; }
#toc > summary { cursor: pointer; font-weight: 600; font-size: .85rem; text-transform: uppercase;
  letter-spacing: .05em; color: var(--muted); }
#toc[open] > summary { margin-bottom: .3rem; }
#toc ul { margin: 0; padding-left: 1.1rem; }
@media (max-width: 60rem) {
  body { flex-direction: column; }
  #side { width: auto; flex: none; height: auto; position: static; border-right: 0;
    border-bottom: 1px solid var(--line); }
}
"""

JS = """
const pages = window.WIKI_PAGES || [];
const q = document.getElementById('q');
const hits = document.getElementById('hits');
const nav = document.getElementById('nav');

function esc(s) { return s.replace(/[&<>]/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;'}[c])); }

function show(term) {
  if (!term) { hits.innerHTML = ''; nav.style.display = ''; return; }
  const needle = term.toLowerCase();
  const found = [];
  for (const page of pages) {
    const text = page.text;
    let at = text.toLowerCase().indexOf(needle);
    let count = 0;
    while (at !== -1 && count < 3) {
      const from = Math.max(0, at - 50);
      const line = text.slice(from, at + needle.length + 60);
      found.push({ title: page.title, href: page.href,
        snippet: esc(line.slice(0, at - from)) + '<mark>' + esc(line.slice(at - from, at - from + needle.length))
          + '</mark>' + esc(line.slice(at - from + needle.length)) });
      at = text.toLowerCase().indexOf(needle, at + needle.length);
      count += 1;
    }
    if (found.length > 60) { break; }
  }
  nav.style.display = 'none';
  hits.innerHTML = found.length
    ? found.map(f => `<a class="hit" href="${f.href}"><b>${esc(f.title)}</b><span>…${f.snippet}…</span></a>`).join('')
    : '<p>Nothing found.</p>';
}

q.addEventListener('input', () => show(q.value.trim()));
document.addEventListener('keydown', e => {
  if (e.key === '/' && document.activeElement !== q) { e.preventDefault(); q.focus(); }
  if (e.key === 'Escape' && document.activeElement === q) { q.value = ''; show(''); q.blur(); }
});
"""

# The progress pages sit in sub-folders: their search links are made relative to the wiki's root.
PROGRESS_JS = JS.replace("href: page.href", "href: (window.WIKI_ROOT || '') + page.href")

PROGRESS_PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title} — Black &amp; White notes</title>
<link rel="stylesheet" href="{root}wiki.css">
</head>
<body>
<nav id="side">
  <h1><a href="{root}index.html" style="color:inherit;text-decoration:none">Black &amp; White notes</a></h1>
  <span class="tag">openblack — progress, row by row</span>
  <input id="q" type="search" placeholder="Search (press /)" autocomplete="off">
  <div id="hits"></div>
  <div id="nav">{nav}</div>
</nav>
<main>
{toc}
{body}
</main>
<script>window.WIKI_ROOT = "{root}";</script>
<script src="{root}search.js"></script>
<script src="{root}progress/progress.js"></script>
</body>
</html>
"""

PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title} — Black &amp; White notes</title>
<link rel="stylesheet" href="wiki.css">
</head>
<body>
<nav id="side">
  <h1><a href="index.html" style="color:inherit;text-decoration:none">Black &amp; White notes</a></h1>
  <span class="tag">openblack — how the original behaves</span>
  <input id="q" type="search" placeholder="Search (press /)" autocomplete="off">
  <div id="hits"></div>
  <div id="nav">{nav}</div>
</nav>
<main>
{toc}
{body}
</main>
<script src="search.js"></script>
<script src="wiki.js"></script>
</body>
</html>
"""


def read(path):
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def title_of(text, stem):
    for line in text.splitlines():
        if line.startswith("# "):
            return line[2:].strip()
    return stem


def plain(text):
    """The page as searchable text: no code fences, markup or links."""
    text = re.sub(r"```.*?```", " ", text, flags=re.S)
    text = re.sub(r"`([^`]*)`", r"\1", text)
    text = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"[#>*|_-]+", " ", text)
    return re.sub(r"\s+", " ", text).strip()


def mark_states(body):
    """Turns the (inferred) and (approximate) markers, and the Pending headings, into visible labels."""
    body = body.replace("(inferred)", '<span class="state approx">inferred</span>')
    body = body.replace("(approximate)", '<span class="state approx">approximate</span>')
    body = re.sub(r"(<h2[^>]*>)(Pending[^<]*)(</h2>)",
                  r'\1\2<span class="state pending">pending</span>\3', body)
    return body


def md_href(path):
    """A Markdown link target as its HTML page: page.md to page.html, folder/ to folder/index.html."""
    if path.endswith("/"):
        return path + "index.html"
    if path.endswith(".md"):
        return path[:-3] + ".html"
    return path


def progress_links(body):
    """Links inside a progress page: other progress pages and folders, and bw1-notes pages (which sit at the root)."""
    def fix(match):
        target, anchor = match.group(1), match.group(2) or ""
        if target.startswith(("http:", "https:", "mailto:")) or not target.endswith((".md", "/")):
            return match.group(0)
        return 'href="%s%s"' % (md_href(target.replace("bw1-notes/", "")), anchor)
    return re.sub(r'href="([^"#]*)(#[^"]*)?"', fix, body)


def wiki_to_progress_links(body):
    """Links from a bw1-notes page into the progress notes, which sit in the progress/ folder of the output."""
    return re.sub(r'href="\.\./progress/([^"#]*)((#[^"]*)?)"',
                  lambda m: 'href="progress/%s%s"' % (md_href(m.group(1)), m.group(2)), body)


def folder_listing(src, folder):
    """A generated index page for a progress folder without a README: its sub-folders and files with their scores."""
    here = os.path.join(src, folder)
    up = "../" * (folder.count("/") + 1)
    name = folder.split("/")[-1].replace("_", " ")
    lines = ["# %s" % name.capitalize(), "",
             "The progress files of this folder. All domains and their scores: [the index](%sINDEX.md)." % up, ""]
    subs = sorted(d for d in os.listdir(here) if os.path.isdir(os.path.join(here, d)))
    if subs:
        lines += ["Sub-folders: " + ", ".join("[%s](%s/)" % (d.replace("_", " "), d) for d in subs) + ".", ""]
    lines += ["| File | Progress |", "|---|---|"]
    for f in sorted(f for f in os.listdir(here) if f.endswith(".md")):
        text = read(os.path.join(here, f))
        score = re.search(r"\*\*Progress: (.*)\*\*", text)
        lines.append("| [%s](%s) | %s |" % (title_of(text, f[:-3]), f, score.group(1) if score else ""))
    return "\n".join(lines) + "\n"


def build_progress(src, out):
    """Writes the progress notes to out/progress/, keeping their folders. Returns their search entries."""
    pages = {}
    for folder, dirs, files in os.walk(src):
        dirs[:] = sorted(d for d in dirs if d != "tools")
        rel = os.path.relpath(folder, src).replace("\\", "/")
        rel = "" if rel == "." else rel
        for f in sorted(files):
            if f.endswith(".md"):
                pages[(rel + "/" if rel else "") + f[:-3]] = read(os.path.join(folder, f))
        if rel and "README.md" not in files:
            pages[rel + "/index"] = folder_listing(src, rel)
    domains = sorted(k.split("/")[0] for k in pages if k.count("/") == 1 and k.endswith(("/index", "/README")))

    for key in sorted(pages):
        depth = key.count("/") + 1
        root = "../" * depth
        home = root + "progress/"
        nav = ['<h2>Wiki</h2>', '<a href="%sindex.html">Black &amp; White notes</a>' % root, '<h2>Progress</h2>']
        for stem, label in (("README", "About the progress notes"), ("INDEX", "Progress index")):
            nav.append('<a class="%s" href="%s%s.html">%s</a>' % ("here" if key == stem else "", home, stem, label))
        nav.append('<h2>Domains</h2>')
        for domain in domains:
            here = "here" if key.split("/")[0] == domain and depth > 1 else ""
            nav.append('<a class="%s" href="%s%s/index.html">%s</a>' % (
                here, home, domain, html.escape(domain.replace("_", " "))))
        converter = markdown.Markdown(extensions=EXTENSIONS, extension_configs=EXTENSION_CONFIGS)
        body = progress_links(converter.convert(pages[key]))
        toc = ""
        if getattr(converter, "toc", "").count("<li>") > 2:
            toc = '<details id="toc"><summary>On this page</summary>%s</details>' % converter.toc
        path = os.path.join(out, "progress", *key.split("/")) + ".html"
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(PROGRESS_PAGE.format(title=html.escape(title_of(pages[key], key)), nav="\n".join(nav),
                                              toc=toc, body=mark_states(body), root=root))
        if key == "README" or key.endswith("/README"):
            shutil.copyfile(path, os.path.join(os.path.dirname(path), "index.html"))
    with open(os.path.join(out, "progress", "progress.js"), "w", encoding="utf-8") as handle:
        handle.write(PROGRESS_JS)
    print("%d progress pages -> %s" % (len(pages), os.path.join(out, "progress")))
    return [{"title": "Progress: " + title_of(pages[k], k), "href": "progress/" + k + ".html",
             "text": plain(pages[k])[:120000]} for k in sorted(pages) if not k.endswith("/index")]


def build(src, out, progress=None):
    stems = sorted(f[:-3] for f in os.listdir(src) if f.endswith(".md"))
    listed = [s for _, names in GROUPS for s in names]
    groups = [(name, [s for s in names if s in stems]) for name, names in GROUPS]
    other = [s for s in stems if s not in listed]
    if other:
        groups.append(("Other", other))

    pages = {}
    for stem in stems:
        text = read(os.path.join(src, stem + ".md"))
        pages[stem] = {"title": title_of(text, stem), "text": text}

    entries = [
        {"title": pages[s]["title"], "href": s + ".html", "text": plain(pages[s]["text"])[:120000]}
        for s in stems
    ]

    os.makedirs(out, exist_ok=True)
    if progress:
        entries += build_progress(progress, out)
    search = json.dumps(entries, ensure_ascii=False)
    for stem in stems:
        nav = []
        for name, names in groups:
            if not names:
                continue
            nav.append("<h2>%s</h2>" % html.escape(name))
            for other_stem in names:
                nav.append('<a class="%s" href="%s.html">%s</a>' % (
                    "here" if other_stem == stem else "",
                    other_stem, html.escape(pages[other_stem]["title"])))
        if progress:
            nav.append("<h2>Progress</h2>")
            nav.append('<a class="" href="progress/INDEX.html">What our tree does, row by row</a>')
        converter = markdown.Markdown(extensions=EXTENSIONS, extension_configs=EXTENSION_CONFIGS)
        body = converter.convert(pages[stem]["text"])
        body = re.sub(r'href="([a-zA-Z0-9_-]+)\.md((#[^"]*)?)"', r'href="\1.html\2"', body)
        if progress:
            body = wiki_to_progress_links(body)
        toc = ""
        if getattr(converter, "toc", "").count("<li>") > 2:
            toc = '<details id="toc"><summary>On this page</summary>%s</details>' % converter.toc
        with open(os.path.join(out, stem + ".html"), "w", encoding="utf-8") as handle:
            handle.write(PAGE.format(title=html.escape(pages[stem]["title"]), nav="\n".join(nav),
                                     toc=toc, body=mark_states(body)))

    if os.path.exists(os.path.join(out, "README.html")):
        shutil.copyfile(os.path.join(out, "README.html"), os.path.join(out, "index.html"))
    with open(os.path.join(out, "search.js"), "w", encoding="utf-8") as handle:
        handle.write("window.WIKI_PAGES = " + search + ";\n")
    with open(os.path.join(out, "wiki.css"), "w", encoding="utf-8") as handle:
        handle.write(CSS)
    with open(os.path.join(out, "wiki.js"), "w", encoding="utf-8") as handle:
        handle.write(JS)
    img_src, img_out = os.path.join(src, "img"), os.path.join(out, "img")
    if os.path.isdir(img_src):
        shutil.rmtree(img_out, ignore_errors=True)
        shutil.copytree(img_src, img_out)
    print("%d pages -> %s" % (len(stems), out))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--src", default="docs/bw1-notes")
    parser.add_argument("--out", default="docs/wiki-html")
    parser.add_argument("--progress", default="docs/progress")
    parser.add_argument("--no-progress", action="store_true", help="build the bw1-notes wiki alone")
    args = parser.parse_args()
    use_progress = not args.no_progress and os.path.isdir(args.progress)
    build(args.src, args.out, args.progress if use_progress else None)
