#!/usr/bin/env python3
"""Prove that no fact of the old documentation is lost in the new wiki.

Usage:
    python coverage.py [options] <old dir or file>... <new dir>

Every fact token found in the old sources must appear somewhere in the new wiki. The tokens are:

    addr   hex addresses: 0x5B3C70, 0xBF358C, the halves of ranges 0x5E5D32..0x5E5ED2, and fn_005FF4F0
           (an fn_ name counts as its address too). Five to eight hex digits; smaller hex numbers are offsets
           (+0x48C8) and are not addresses.
    func   function and class names: Class::Method, fn_XXXXXXXX, the PSys rule names (UR_, ER_, SF_, ZR_, PT_),
           and identifiers in backticks that look like code (CamelCase with two capitals, or with an underscore).
    unit   numbers with a unit: 225 ms, 10 s, 40 m, 0x30 bytes, 24 bits, 60 fps, 2048ths, 45 degrees, 15 %, ...
           Units are folded (seconds -> s, metres -> m, byte -> bytes) and numbers normalised (1.50 -> 1.5).
    pair   an address together with the name written next to it ("Tree::Grow 0x74A3F0"): the claim that this
           address is that function. Reported, and checked only with --fail-on including pair.
    code   openblack's own identifiers (lower-case namespaces such as abodes::TypeOf, lower-case names in
           backticks). Reported, and checked only with --fail-on including code: the code itself holds them.

Old sources are .md and .txt files, read whole (code blocks included). The new wiki is every .md under <new dir>.

Options:
    --drop FILE       TSV of facts dropped on purpose, each with its reason (a line without one is an error):
                        kind<TAB>token<TAB>reason   one token (a dead end, a renamed function: "renamed: <symbol>")
                        file<TAB>path<TAB>reason    every token found only in files whose path contains this text
                                                    (a report dropped whole). Lines starting with # are comments.
    --fail-on KINDS   comma list of kinds whose missing tokens fail the run (default addr,func,unit).
    --report FILE     write every missing token with the first file and line it was seen in (TSV).
    --exclude GLOB    skip old files whose path contains this text (repeatable), e.g. wiki-html.
    --xref DB         the xref index (dev/tools/xref/xref.db). A missing address that lies inside a function of
                      runblack.exe whose start address or name is in the new wiki is counted as folded into that
                      function (an instruction address the page's function already stands for), not as missing.
    --selftest        also check that the checker sees a removed token (runs on the given sources).

Exit code: 0 when nothing in the failing kinds is missing, 1 otherwise, 2 on a usage error.
"""
import argparse
import collections
import os
import re
import sys

HEX = re.compile(r'(?<![\w+\-])0x([0-9A-Fa-f]{5,8})\b')
RANGE_HI = re.compile(r'0x[0-9A-Fa-f]{5,8}\s*(?:\.\.|-|–)\s*(?:0x)?([0-9A-Fa-f]{5,8})\b')
FN = re.compile(r'\b(?:fn|sub|FUN)_([0-9A-Fa-f]{6,8})\b')
QUALIFIED = re.compile(r'\b([A-Za-z_]\w*(?:::~?[A-Za-z_]\w*)+)\b')
PSYS = re.compile(r'\b((?:UR|ER|SF|ZR|PT)_[A-Za-z]\w+)\b')
TICKED = re.compile(r'`([A-Za-z_][\w:]*)`')
NUM = r'(-?\d+(?:[.,]\d+)?)'
UNITS = {
    'ms': 'ms', 'msec': 'ms', 'millisecond': 'ms', 'milliseconds': 'ms',
    's': 's', 'sec': 's', 'secs': 's', 'second': 's', 'seconds': 's',
    'min': 'min', 'minute': 'min', 'minutes': 'min', 'h': 'h', 'hour': 'h', 'hours': 'h',
    'm': 'm', 'metre': 'm', 'metres': 'm', 'meter': 'm', 'meters': 'm', 'cm': 'cm', 'km': 'km', 'mm': 'mm',
    'px': 'px', 'pixel': 'px', 'pixels': 'px', 'hz': 'Hz', 'khz': 'kHz', 'fps': 'fps',
    'byte': 'bytes', 'bytes': 'bytes', 'bit': 'bits', 'bits': 'bits', 'kb': 'kB', 'mb': 'MB',
    'frame': 'frames', 'frames': 'frames', 'turn': 'turns', 'turns': 'turns', 'sample': 'samples',
    'samples': 'samples', 'degree': 'deg', 'degrees': 'deg', '°': 'deg', 'rad': 'rad', 'radians': 'rad',
    '%': '%', 'percent': '%', '2048ths': '2048ths', 'db': 'dB', 'texels': 'texels', 'cells': 'cells', 'cell': 'cells',
}
UNIT = re.compile(NUM + r'\s?(' + '|'.join(sorted((re.escape(u) for u in UNITS), key=len, reverse=True)) + r')(?![\w])',
                  re.IGNORECASE)
KINDS = ('addr', 'func', 'unit', 'pair', 'code')
PAIR = re.compile(r'`?([A-Za-z_]\w*(?:::~?\w+)+|(?:UR|ER|SF|ZR)_\w+)`?\s*\(?\s*0x([0-9A-Fa-f]{5,8})\b')


def norm_addr(h):
    return '0x' + h.upper().lstrip('0').rjust(1, '0')


def norm_num(n):
    n = n.replace(',', '.')
    if '.' in n:
        n = n.rstrip('0').rstrip('.')
    return n.lstrip('+') or '0'


def looks_like_code(t):
    if '::' in t or '_' in t.strip('_'):
        return len(t) > 3
    return len(re.findall(r'[A-Z]', t)) >= 2 and bool(re.search(r'[a-z]', t)) and not t.isupper()


def tokens(text):
    """Yield (kind, token, line number) for one document."""
    for n, line in enumerate(text.split('\n'), 1):
        for m in HEX.finditer(line):
            yield 'addr', norm_addr(m.group(1)), n
        for m in RANGE_HI.finditer(line):
            yield 'addr', norm_addr(m.group(1)), n
        for m in FN.finditer(line):
            yield 'addr', norm_addr(m.group(1)), n
            yield 'func', 'fn_' + m.group(1).upper().rjust(8, '0'), n
        for m in QUALIFIED.finditer(line):
            t = m.group(1)
            if re.match(r'^(std|glm|bgfx|entt)::', t):
                continue
            # a lower-case namespace is openblack's own C++ (abodes::TypeOf); the original's classes are capitalised
            yield ('code' if t[0].islower() or t.startswith('openblack::') else 'func'), t, n
        for m in PSYS.finditer(line):
            yield 'func', m.group(1), n
        for m in TICKED.finditer(line):
            t = m.group(1)
            if looks_like_code(t) and '::' not in t:
                yield ('code' if t[0].islower() else 'func'), t, n
        for m in UNIT.finditer(line):
            num, unit = m.group(1), m.group(2)
            if unit.lower() in ('m', 's', 'h') and m.group(0)[len(num):len(num) + 1] != ' ' and not unit.islower():
                continue
            yield 'unit', norm_num(num) + ' ' + UNITS[unit.lower()], n
        for m in PAIR.finditer(line):
            yield 'pair', m.group(1) + ' ' + norm_addr(m.group(2)), n


def walk(paths, exts, exclude):
    for p in paths:
        if os.path.isfile(p):
            yield p
            continue
        for d, dirs, fs in os.walk(p):
            dirs.sort()
            for f in sorted(fs):
                full = os.path.join(d, f)
                if os.path.splitext(f)[1].lower() in exts and not any(x in full.replace('\\', '/') for x in exclude):
                    yield full


def collect(paths, exts, exclude):
    seen = {}  # (kind, token) -> (first file, line)
    files_of = collections.defaultdict(set)  # (kind, token) -> every file it is in
    for f in walk(paths, exts, exclude):
        text = open(f, encoding='utf-8', errors='replace').read()
        for kind, tok, n in tokens(text):
            key = (kind, tok)
            if key not in seen:
                seen[key] = (f, n)
            files_of[key].add(f.replace('\\', '/'))
    return seen, files_of


def apply_file_drops(old, files_of, file_drops):
    """Tokens found only in files dropped whole (a dead-end report) count as dropped."""
    out = {}
    if not file_drops:
        return out
    for key in old:
        fs = files_of[key]
        if fs and all(any(pat in f for pat in file_drops) for f in fs):
            out[key] = 'in a file dropped whole'
    return out


def demangle(sym):
    m = re.match(r'^\?([~\w]+)@([\w@]+?)@@', sym or '')
    if m:
        return '::'.join(reversed(m.group(2).split('@'))) + '::' + m.group(1)
    return (sym or '').lstrip('_?')


class Funcs:
    """Function ranges of runblack.exe from the xref index."""

    def __init__(self, db):
        import bisect
        import sqlite3
        rows = list(sqlite3.connect(db).execute('SELECT addr, end, name FROM funcs ORDER BY addr'))
        self.starts = [r[0] for r in rows]
        self.rows = rows
        self.bisect = bisect

    def containing(self, addr):
        i = self.bisect.bisect_right(self.starts, addr) - 1
        if i >= 0 and self.rows[i][0] <= addr < self.rows[i][1]:
            return self.rows[i]
        return None


def compare(old, new_keys, dropped, funcs=None):
    missing = collections.defaultdict(list)
    folded = collections.Counter()
    new_funcs = {t for k, t in new_keys if k == 'func'}
    for (kind, tok), where in old.items():
        if (kind, tok) in new_keys or (kind, tok) in dropped:
            continue
        if kind == 'addr' and funcs:
            f = funcs.containing(int(tok, 16))
            if f:
                name = demangle(f[2])
                if (('addr', norm_addr('%X' % f[0])) in new_keys or name in new_funcs
                        or ('func', 'fn_%08X' % f[0]) in new_keys):
                    folded[kind] += 1
                    continue
        missing[kind].append((tok, where))
    missing['_folded'] = folded
    return missing


def main(argv):
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument('paths', nargs='*')
    ap.add_argument('--drop')
    ap.add_argument('--fail-on', default='addr,func,unit')
    ap.add_argument('--report')
    ap.add_argument('--exclude', action='append', default=[])
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--xref')
    ap.add_argument('-h', '--help', action='store_true')
    a, unknown = ap.parse_known_args(argv)
    if a.help:
        print(__doc__)
        return 0
    if unknown or len(a.paths) < 2:
        print(__doc__.split('\n\n')[1], file=sys.stderr)
        print('error: give at least one old source and the new wiki directory', unknown, file=sys.stderr)
        return 2
    fail_on = {k for k in a.fail_on.split(',') if k}
    if not fail_on <= set(KINDS):
        print('error: --fail-on takes', ','.join(KINDS), file=sys.stderr)
        return 2
    olds, new = a.paths[:-1], a.paths[-1]
    if not os.path.isdir(new):
        print('error: the new wiki must be a directory:', new, file=sys.stderr)
        return 2
    dropped, file_drops = {}, []
    if a.drop:
        for line in open(a.drop, encoding='utf-8'):
            if line.strip() and not line.startswith('#'):
                parts = line.rstrip('\n').split('\t')
                if len(parts) < 3 or not parts[2].strip():
                    print('error: every drop needs a reason:', line.strip(), file=sys.stderr)
                    return 2
                if parts[0] == 'file':
                    file_drops.append(parts[1].replace('\\', '/'))
                elif parts[0] in KINDS:
                    dropped[(parts[0], parts[1])] = parts[2]
                else:
                    print('error: a drop is kind<TAB>token<TAB>reason or file<TAB>path<TAB>reason:', line.strip(),
                          file=sys.stderr)
                    return 2

    old, files_of = collect(olds, {'.md', '.txt'}, a.exclude + ['wiki-html'])
    dropped.update(apply_file_drops(old, files_of, file_drops))
    new_tokens, _ = collect([new], {'.md'}, [])
    new_keys = set(new_tokens)
    funcs = Funcs(a.xref) if a.xref else None
    missing = compare(old, new_keys, dropped, funcs)
    folded = missing.pop('_folded')

    totals = collections.Counter(k for k, _ in old)
    print(f'old sources: {len(list(walk(olds, {".md", ".txt"}, a.exclude + ["wiki-html"])))} files; new wiki: '
          f'{len(list(walk([new], {".md"}, [])))} files')
    print(f'{"kind":5} {"distinct":>9} {"missing":>8} {"dropped":>8} {"folded":>7}  covered')
    for k in KINDS:
        dr = sum(1 for (kk, _t) in dropped if kk == k and (kk, _t) in old and (kk, _t) not in new_keys)
        tot = totals[k]
        cov = 100.0 * (tot - len(missing[k])) / tot if tot else 100.0
        print(f'{k:5} {tot:9d} {len(missing[k]):8d} {dr:8d} {folded[k]:7d}  {cov:6.2f}%'
              + ('' if k in fail_on else '  (not failing)'))
    if a.report:
        with open(a.report, 'w', encoding='utf-8', newline='\n') as f:
            f.write('kind\ttoken\tfirst seen in\tline\n')
            for k in KINDS:
                for tok, (fn, n) in sorted(missing[k]):
                    f.write(f'{k}\t{tok}\t{fn}\t{n}\n')
        print('missing tokens written to', a.report)
    else:
        for k in KINDS:
            if k in fail_on:
                for tok, (fn, n) in sorted(missing[k])[:20]:
                    print(f'  missing {k}: {tok}  ({fn}:{n})')

    if a.selftest:
        # the checker must notice a token that disappears, and must not count one that is dropped on purpose
        probe = next(((k, t) for (k, t) in sorted(old) if k == 'addr'), None)
        if probe:
            fake_new = set(old) - {probe}
            m = compare(old, fake_new, {})
            ok1 = [t for t, _ in m['addr']] == [probe[1]]
            m2 = compare(old, fake_new, {probe: 'test'})
            m2.pop('_folded')
            ok2 = not m2['addr']
            print(f'selftest: removed {probe[1]} -> reported {"yes" if ok1 else "NO"}; '
                  f'dropped on purpose -> not reported {"yes" if ok2 else "NO"}')
            if not (ok1 and ok2):
                return 1
    bad = sum(len(missing[k]) for k in fail_on)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
