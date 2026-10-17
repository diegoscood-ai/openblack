"""Checks every relative link and #anchor of the Markdown pages under a folder, with GitHub's heading slugs.

    python -I docs/tools/links.py docs              (SUMMARY.md and every page under docs/wiki)
    python -I docs/tools/links.py docs --list out.tsv

Links are relative to the page's own folder. Code blocks and inline code are skipped. A link to a folder must name an
existing folder; a link to any other file must name an existing file; an anchor must be a heading of the target page
(a repeated heading gets -1, -2) or an explicit <a id="..."> alias. Exit code: 0 when every link resolves, 1 otherwise.
"""
import os
import posixpath
import re
import sys


def slug(heading):
    """GitHub's anchor for a heading: lower case, markup and punctuation dropped, spaces to hyphens."""
    text = heading.strip().lower()
    text = re.sub(r"<[^>]+>", "", text)
    text = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = text.replace("`", "").replace("*", "")
    text = re.sub(r"[^\w\- ]", "", text)
    return text.replace(" ", "-")


def strip_code(text):
    """Blanks fenced code blocks, keeping the line count."""
    return re.sub(r"(?ms)^\s*```.*?^\s*```", lambda m: "\n" * m.group(0).count("\n"), text)


def anchors_of(text):
    found, seen = set(), {}
    for match in re.finditer(r"(?m)^#{1,6} (.*?)\s*#*\s*$", text):
        base = slug(match.group(1))
        count = seen.get(base, 0)
        seen[base] = count + 1
        found.add(base if count == 0 else f"{base}-{count}")
    found |= set(re.findall(r'<a (?:id|name)="([^"]+)"', text))
    return found


def main(argv):
    if not argv or argv[0].startswith("-"):
        print(__doc__, file=sys.stderr)
        return 2
    root = argv[0]
    listing = argv[argv.index("--list") + 1] if "--list" in argv else None
    pages = {}
    for folder, _, files in os.walk(root):
        for name in files:
            if name.endswith(".md"):
                rel = os.path.relpath(os.path.join(folder, name), root).replace(os.sep, "/")
                with open(os.path.join(root, rel), encoding="utf-8") as f:
                    pages[rel] = strip_code(f.read())
    anchors = {rel: anchors_of(text) for rel, text in pages.items()}
    bad, total = [], 0
    for rel in sorted(pages):
        text = re.sub(r"`[^`\n]*`", "", pages[rel])
        for number, line in enumerate(text.split("\n"), 1):
            for match in re.finditer(r"\]\(<?([^)\s>]+)>?(?:\s+\"[^\"]*\")?\)", line):
                link = match.group(1)
                if re.match(r"[a-z]+:", link):
                    continue
                total += 1
                path, _, anchor = link.partition("#")
                target = rel if not path else posixpath.normpath(posixpath.join(posixpath.dirname(rel), path))
                on_disk = os.path.join(root, target)
                if path.endswith("/") or os.path.isdir(on_disk):
                    if not os.path.isdir(on_disk):
                        bad.append((rel, number, link, "no folder"))
                elif not os.path.isfile(on_disk):
                    bad.append((rel, number, link, "no file"))
                elif anchor and target in anchors and anchor not in anchors[target]:
                    bad.append((rel, number, link, "no anchor"))
    lines = ["\t".join(map(str, b)) for b in bad]
    if listing:
        with open(listing, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
    else:
        for line in lines:
            print(line)
    print(f"{len(pages)} pages, {total} links, {len(bad)} broken")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main(sys.argv[1:]))
