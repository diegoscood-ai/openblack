"""STYLE.md rule 9: the status marks get their emoji; the word always stays. Code spans and fenced blocks untouched.
usage: python status_marks.py dry|run <wiki dir>"""
import os, re, sys

mode, root = (sys.argv[1], sys.argv[2]) if len(sys.argv) == 3 else ('', '')
if mode not in ('dry', 'run'):
    sys.exit('usage: status_marks.py dry|run <wiki dir>')
SKIP = {'dashboard.md', 'port-audit.md'}
RULES = [
    (re.compile(r'\*\*faithful\*\*|\(faithful\)'), '✅ faithful'),
    (re.compile(r'\*\*approximate\*\*|\(approximate\)'), '⚠️ approximate'),
    (re.compile(r'\*\*inferred\*\*|\(inferred\)'), '🔍 inferred'),
    (re.compile(r'(?<!📼 )\[L\]'), '📼 1.20 only'),
    (re.compile(r'(?<!🧪 )\bverify with (?:xref/oracle|xref|the oracle)'), lambda m: '🧪 ' + m.group(0)),
    (re.compile(r'(?<!🎯 )\b(?:chosen|deliberate) difference'), lambda m: '🎯 chosen difference'),
]
def convert(text):
    out, fence = [], False
    for line in text.split('\n'):
        if line.lstrip().startswith('```'):
            fence = not fence
            out.append(line)
            continue
        if fence:
            out.append(line)
            continue
        parts = re.split(r'(`[^`]*`)', line)  # keep inline code spans as they are
        for i in range(0, len(parts), 2):
            for rx, rep in RULES:
                parts[i] = rx.sub(rep, parts[i])
        out.append(''.join(parts))
    return '\n'.join(out)
changed = hits = 0
for r, _, fs in os.walk(root):
    for f in fs:
        if not f.endswith('.md') or f in SKIP:
            continue
        p = os.path.join(r, f)
        s = open(p, encoding='utf-8', newline='').read()
        t = convert(s)
        if t != s:
            changed += 1
            hits += sum(1 for a, b in zip(s.split('\n'), t.split('\n')) if a != b)
            if mode == 'run':
                open(p, 'w', encoding='utf-8', newline='').write(t)
print(f'{mode}: {changed} pages, {hits} lines')
