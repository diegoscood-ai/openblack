"""vectors_to_cpp.py: a few cases of a vector file as a C++ constant table, to paste into an openblack gtest so that the
check runs in CI without the exe or the vector file (see GUIDE.md, "Vectors into a gtest").

    python vectors_to_cpp.py vectors/GameRand.json [--n 12] [--skip-out ret] [--name k_GameRandCases]

Every value is its exact bits (uint32_t, or uint64_t for f64); the test bit_casts the floats. Cases where the exe
faulted are left out. The chosen cases: the spec's hand-picked ones first (they come first in the file), then evenly
spaced ones. 80-bit outputs ("ret" of an st0 function) are left out: compare ret_f32 / ret_f64.
"""

import argparse
import json
import sys


def ctype(t):
    return 'uint64_t' if t in ('f64', 'i64', 'u64') else 'uint32_t'


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('vectors')
    ap.add_argument('--n', type=int, default=12, help='how many cases (default 12)')
    ap.add_argument('--head', type=int, default=None, help='how many leading cases to keep before spacing (default n/2)')
    ap.add_argument('--skip-out', action='append', default=[], help='an output to leave out (repeatable)')
    ap.add_argument('--name', default=None, help='the table name (default k_<Function>Cases)')
    a = ap.parse_args(argv)
    with open(a.vectors, encoding='utf-8') as f:
        doc = json.load(f)
    fields = [(x['name'], x['type'], x.get('count', 1), 'in') for x in doc['inputs']]
    fields += [(x['name'], x['type'], x.get('count', 1), 'out') for x in doc['outputs']
               if x['type'] != 'f80' and x['name'] not in a.skip_out]
    good = [c for c in doc['cases'] if 'out' in c]
    head = min(len(good), a.head if a.head is not None else a.n // 2)
    picked = good[:head]
    rest = good[head:]
    want = max(0, a.n - head)
    if rest and want:
        step = max(1, len(rest) // want)
        picked += rest[::step][:want]
    name = a.name or 'k_%sCases' % doc['function']
    in_names = {n for n, _, _, side in fields if side == 'in'}

    def ident(n, side):
        # an in / out field (a seed) gets "_after" on its output
        return n.replace('.', '_') + ('_after' if side == 'out' and n in in_names else '')

    # Plain English only: openblack's comments name no addresses or decompiled symbols (CLAUDE.md, Comments)
    sse2 = doc.get('machine', {}).get('HasSSE2')
    print('// Measured on the original game with the exe oracle: %d of the %d cases of %s, exact bits%s.'
          % (len(picked), len(doc['cases']), doc['function'] + '.json',
             '' if sse2 is None else (', x87 conversions' if sse2 == 0 else ', SSE2 conversions')))
    print('struct %sCase\n{' % doc['function'])
    for n, t, count, side in fields:
        member = ctype(t) if count == 1 else 'std::array<%s, %d>' % (ctype(t), count)
        print('\t%s %s; // %s %s' % (member, ident(n, side), side, t))
    print('};')
    print('constexpr std::array<%sCase, %d> %s {{' % (doc['function'], len(picked), name))
    for c in picked:
        vals = []
        for n, t, count, side in fields:
            v = c[side][n]
            if count == 1:
                vals.append(v)
            else:
                vals.append('{' + ', '.join(v) + '}')
        print('    {%s},' % ', '.join(vals))
    print('}};')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
