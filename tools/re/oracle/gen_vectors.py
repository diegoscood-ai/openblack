"""gen_vectors.py: golden vectors of runblack.exe functions, run natively through oracle_host (see README.md).

    python gen_vectors.py specs/yxz.json [specs/other.json ...]     -> vectors/<function>.json
    python gen_vectors.py --all                                       every specs/*.json
    options: --cases N (override random_cases), --out DIR

A spec (JSON) names the function, its convention, its arguments and buffers, how to draw the inputs (generators,
edge values, hand-picked cases), the seed and the number of random cases; optionally globals at fixed addresses
(inputs written before each call, outputs read after it), stubs, the fake GGame (the RNG seeds) and a chain (an
input taken from the previous case's output, for draw sequences). README.md has the format. Every input and output is stored as the
exact bits, as hex strings ("0x3F800000"), so a test can compare bit for bit.
"""

import argparse
import datetime
import glob
import hashlib
import json
import math
import os
import random
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import oracle  # noqa: E402

FORMAT = 'bw-oracle-vectors/1'
BATCH = 200

TYPES = {  # name: (size, struct code, is_float)
    'f32': (4, '<I', True),
    'f64': (8, '<Q', True),
    'i32': (4, '<I', False),
    'u32': (4, '<I', False),
    'ptr': (4, '<I', False),
    'i16': (2, '<H', False),
    'u16': (2, '<H', False),
    'i8': (1, '<B', False),
    'u8': (1, '<B', False),
}
RET_TYPES = {'void': None, 'i32': 'i32', 'u32': 'u32', 'ptr': 'u32', 'i16': 'u16', 'u16': 'u16', 'i8': 'u8',
             'u8': 'u8', 'bool': 'u8', 'f32': 'f32', 'f64': 'f64', 'st0': 'f80'}
HEX_DIGITS = {'f32': 8, 'f64': 16, 'i32': 8, 'u32': 8, 'ptr': 8, 'i16': 4, 'u16': 4, 'i8': 2, 'u8': 2, 'f80': 20}


class SpecError(ValueError):
    pass


# ------------------------------------------------------------------------------------------------------------------
# Values

def to_bits(t, v):
    """The bits of a spec value for type t: a number, '0x...' raw bits, 'nan' / 'inf' / '-inf' / '-0'"""
    size, _, is_float = TYPES[t]
    mask = (1 << (8 * size)) - 1
    if isinstance(v, str):
        s = v.strip()
        if s.lower().startswith('0x') or s.lower().startswith('-0x'):
            n = int(s, 16)
            return n & mask
        if is_float:
            v = float({'nan': 'nan', 'inf': 'inf', '+inf': 'inf', '-inf': '-inf', '-0': '-0.0'}.get(s.lower(), s))
        else:
            v = int(s, 0)
    if is_float:
        return oracle.f32_bits(v) if t == 'f32' else oracle.f64_bits(v)
    return int(v) & mask


def bits_hex(t, b):
    return '0x%0*X' % (HEX_DIGITS[t], b)


def wrap_int(t, v):
    size = TYPES[t][0]
    v &= (1 << (8 * size)) - 1
    return v


def signed(t, b):
    size = TYPES[t][0]
    if t.startswith('i') and b >> (8 * size - 1):
        return b - (1 << (8 * size))
    return b


# ------------------------------------------------------------------------------------------------------------------
# Generators

class Generators:
    def __init__(self, named, rng):
        self.named = named or {}
        self.rng = rng

    def resolve(self, g):
        depth = 0
        while isinstance(g, str) or (isinstance(g, dict) and 'ref' in g):
            g = self.named[g] if isinstance(g, str) else self.named[g['ref']]
            depth += 1
            if depth > 16:
                raise SpecError('generator references loop')
        return g

    def scalar(self, g, t, values):
        """One value (bits) of type t; `values` holds the inputs drawn so far (for "near")"""
        g = self.pick(g)
        rng = self.rng
        is_float = TYPES[t][2]
        if 'choice' in g:
            return to_bits(t, rng.choice(g['choice']))
        if 'uniform' in g or 'int' in g:
            lo, hi = g.get('uniform', g.get('int'))
            if is_float and 'int' not in g:
                return to_bits(t, lo + (hi - lo) * rng.random())
            return to_bits(t, rng.randint(int(lo), int(hi)))
        if 'loguniform' in g:
            lo, hi = g['loguniform']
            v = math.exp(math.log(lo) + (math.log(hi) - math.log(lo)) * rng.random())
            if g.get('signed') and rng.random() < 0.5:
                v = -v
            return to_bits(t, v if is_float else int(v))
        if 'bits' in g:
            return rng.getrandbits(8 * TYPES[t][0])
        if 'near' in g:
            base = values[g['near']]
            if isinstance(base, list):
                raise SpecError('"near" needs a scalar input')
            d = self.scalar(g['delta'], t, values)
            if is_float:
                if t == 'f32':
                    return oracle.f32_bits(oracle.bits_f32(base) + oracle.bits_f32(d))
                return oracle.f64_bits(oracle.bits_f64(base) + oracle.bits_f64(d))
            return wrap_int(t, signed(t, base) + signed(t, d))
        raise SpecError('unknown generator %r' % g)

    def pick(self, g):
        """A mix's branch (by weight); the generator itself otherwise"""
        g = self.resolve(g)
        while 'mix' in g:
            total = sum(w for w, _ in g['mix'])
            x = self.rng.random() * total
            for w, sub in g['mix']:
                x -= w
                if x <= 0:
                    break
            g = self.resolve(sub)
        return g

    def field(self, g, t, count, values):
        # a mix picks one branch for the whole field (a matrix is all rotation or all random cells)
        g = self.pick(g)
        if 'rotation' in g:
            if t != 'f32' or count not in (9, 12):
                raise SpecError('"rotation" fills 9 or 12 f32')
            return self.rotation(g['rotation'], count, values)
        if count == 1:
            return self.scalar(g, t, values)
        return [self.scalar(g, t, values) for _ in range(count)]

    def rotation(self, r, count, values):
        """LH rows of SetYXZMatrixOnly(a, b, c) (Ry(-a) Rx(-b) Rz(-c), see ObjectMatrix.h), built in double and rounded
        to float, times an optional scale, plus an optional translation (12 cells)"""
        a, b, c = (oracle.bits_f32(self.scalar(r.get('angle', {'uniform': [-math.pi, math.pi]}), 'f32', values))
                   for _ in range(3))
        ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
        m = [ca * cc - sc * sb * sa, -(sc * cb), sc * sb * ca + sa * cc,
             sa * cc * sb + sc * ca, cc * cb, sc * sa - ca * cc * sb,
             -(cb * sa), sb, cb * ca]
        if 'scale' in r:
            s = oracle.bits_f32(self.scalar(r['scale'], 'f32', values))
            m = [v * s for v in m]
        cells = [oracle.f32_bits(v) for v in m]
        if count == 12:
            tg = r.get('translation', {'choice': [0]})
            cells += [self.scalar(tg, 'f32', values) for _ in range(3)]
        return cells


# ------------------------------------------------------------------------------------------------------------------
# The spec

class Field:
    def __init__(self, spec, buffer=None):
        self.name = spec['name']
        self.type = spec['type']
        if self.type not in TYPES:
            raise SpecError('%s: unknown type %s' % (self.name, self.type))
        self.count = int(spec.get('count', 1))
        self.gen = spec.get('gen')
        self.value = spec.get('value')
        self.out = bool(spec.get('out'))
        self.offset = spec.get('offset')
        self.buffer = buffer
        if self.gen is not None and self.value is not None:
            raise SpecError('%s: gen and value exclude each other' % self.name)
        if buffer is None and self.out:
            raise SpecError('%s: only buffer fields are outputs' % self.name)

    @property
    def size(self):
        return TYPES[self.type][0] * self.count

    @property
    def is_input(self):
        return self.gen is not None

    def const_bits(self):
        v = self.value if self.value is not None else 0
        if self.count == 1:
            return to_bits(self.type, v)
        vs = v if isinstance(v, list) else [v] * self.count
        if len(vs) != self.count:
            raise SpecError('%s: %d values for count %d' % (self.name, len(vs), self.count))
        return [to_bits(self.type, x) for x in vs]


class Spec:
    def __init__(self, path, sym):
        self.path = path
        with open(path, encoding='utf-8') as f:
            s = json.load(f)
        seen = {os.path.abspath(path)}
        here = os.path.dirname(os.path.abspath(path))
        while 'extends' in s:  # a variant: its keys over the base spec's (top-level keys replace)
            base_path = os.path.abspath(os.path.join(here, s.pop('extends')))
            if base_path in seen:
                raise SpecError('"extends" loops at %s' % base_path)
            seen.add(base_path)
            here = os.path.dirname(base_path)
            with open(base_path, encoding='utf-8') as f:
                base = json.load(f)
            base.update(s)
            s = base
        self.raw = s
        self.function = s['function']
        self.vectors = s.get('vectors', self.function)  # the output file's stem
        self.conv = s.get('conv', 'cdecl')
        self.ret = s.get('ret', 'void')
        if self.ret not in RET_TYPES:
            raise SpecError('unknown ret %s' % self.ret)
        self.symbol = s.get('symbol')
        addr = int(s['addr'], 16) if 'addr' in s else None
        if self.symbol:
            try:
                sa = sym.addr(self.symbol)
            except KeyError:
                # bw1-decomp renames symbols as it goes (e.g. a corrected return type changes the decorated name);
                # the exe's addresses never move, so a spec that also gives addr keeps working
                if addr is None:
                    raise
                now = sym.name_at(addr)
                print('warning: %s: symbol %s is gone from symbols.txt; using addr 0x%X (now %s)'
                      % (os.path.basename(path), self.symbol, addr, now), file=sys.stderr)
                sa = addr
                self.symbol = now if '+' not in now else self.symbol
            if addr is not None and addr != sa:
                raise SpecError('addr 0x%X but %s is at 0x%X' % (addr, self.symbol, sa))
            addr = sa
            if self.symbol in sym.by_name or self.symbol in sym.by_plain:
                self.symbol = sym.entry(self.symbol)[1]
        if addr is None:
            raise SpecError('a spec needs symbol or addr')
        self.addr = addr
        self.seed = s.get('seed', 1)
        self.random_cases = int(s.get('random_cases', 1000))
        self.setup = s.get('setup', [])
        self.reset_each = bool(s.get('reset_each', False))
        self.generators = s.get('generators', {})
        self.buffers = {}  # name -> (size, [Field])
        for bname, b in s.get('buffers', {}).items():
            fields_spec = b['fields'] if isinstance(b, dict) else b
            fields, offset = [], 0
            for fs in fields_spec:
                fld = Field(fs, bname)
                align = TYPES[fld.type][0]
                if fld.offset is None:
                    fld.offset = (offset + align - 1) // align * align
                offset = max(offset, fld.offset + fld.size)
                fields.append(fld)
            size = b.get('size', offset) if isinstance(b, dict) else offset
            if size < offset:
                raise SpecError('buffer %s: size %d < its fields %d' % (bname, size, offset))
            self.buffers[bname] = (size, fields)
        # globals: fields at fixed exe addresses ("addr": 0x.. / symbol [+off]); gen / value fields are written before
        # each call, "out" fields read after it (gen + out = in / out, e.g. a seed)
        self.globals = []
        for gs in s.get('globals', []):
            fld = Field(gs, '@global')
            fld.addr = oracle.where(gs['addr'], sym)
            self.globals.append(fld)
        # stubs: calls to these return a fixed value instead of running (oracle.Oracle.stub), set up once
        self.stubs = s.get('stubs', [])
        self.fake_game = bool(s.get('fake_game', False))
        # chain: {input: output}: each case after the first takes that input from the previous case's output
        self.chain = s.get('chain', {})
        self.args = s.get('args', [])
        self.this = s.get('this')
        if self.this is not None and self.this not in self.buffers:
            raise SpecError('this: unknown buffer %s' % self.this)
        self.arg_fields = []
        for a in self.args:
            if a['type'] == 'ptr' and 'buffer' in a:
                if a['buffer'] not in self.buffers:
                    raise SpecError('arg %s: unknown buffer %s' % (a.get('name', '?'), a['buffer']))
                continue
            self.arg_fields.append(Field(a))
        self.inputs = [f for _, fs in self.buffers.values() for f in fs if f.is_input]
        self.inputs += [f for f in self.arg_fields if f.is_input]
        self.inputs += [f for f in self.globals if f.is_input]
        names = [f.name for f in self.inputs]
        if len(set(names)) != len(names):
            raise SpecError('input names must be unique')
        self.outputs = [f for _, fs in self.buffers.values() for f in fs if f.out]
        self.outputs += [f for f in self.globals if f.out]
        self.ret_type = RET_TYPES[self.ret]
        self.edges = s.get('edges', {})
        self.cases = s.get('cases', [])
        for n in list(self.edges) + [k for c in self.cases for k in c]:
            if n not in names:
                raise SpecError('edges / cases name an unknown input %s' % n)
        outs = [f.name for f in self.outputs] + (['ret'] if self.ret_type else [])
        for i, o in self.chain.items():
            if i not in names or o not in outs:
                raise SpecError('chain %s <- %s: not an input / output' % (i, o))

    def output_schema(self):
        out = []
        if self.ret_type:
            out.append({'name': 'ret', 'type': self.ret_type if self.ret != 'st0' else 'f80'})
            if self.ret == 'st0':
                out += [{'name': 'ret_f32', 'type': 'f32'}, {'name': 'ret_f64', 'type': 'f64'}]
        for f in self.outputs:
            d = {'name': f.name, 'type': f.type}
            if f.count > 1:
                d['count'] = f.count
            out.append(d)
        return out

    def input_schema(self):
        out = []
        for f in self.inputs:
            d = {'name': f.name, 'type': f.type}
            if f.count > 1:
                d['count'] = f.count
            if f.buffer == '@global':
                d['addr'] = '0x%08X' % f.addr
            elif f.buffer:
                d['buffer'] = f.buffer
            out.append(d)
        return out

    # --- cases

    def draw(self, gens, fixed):
        """One case: {input name: bits or [bits]}; `fixed` values (spec values) are taken as given"""
        values = {}
        for f in self.inputs:
            if f.name in fixed:
                v = fixed[f.name]
                if f.count == 1:
                    values[f.name] = to_bits(f.type, v)
                else:
                    vs = v if isinstance(v, list) else [v] * f.count
                    if len(vs) != f.count:
                        raise SpecError('%s: %d values for count %d' % (f.name, len(vs), f.count))
                    values[f.name] = [to_bits(f.type, x) for x in vs]
            else:
                values[f.name] = gens.field(f.gen, f.type, f.count, values)
        return values

    def all_cases(self, random_cases=None):
        rng = random.Random(self.seed)
        gens = Generators(self.generators, rng)
        cases = [self.draw(gens, c) for c in self.cases]
        for name, vals in self.edges.items():
            for v in vals:
                cases.append(self.draw(gens, {name: v}))
        n = self.random_cases if random_cases is None else random_cases
        cases += [self.draw(gens, {}) for _ in range(n)]
        return cases

    def request(self, values):
        """The host request of one case"""
        alloc, reads = [], []
        for bname, (size, fields) in self.buffers.items():
            data = bytearray(size)
            for f in fields:
                bits = values[f.name] if f.is_input else f.const_bits()
                code = TYPES[f.type][1]
                items = bits if isinstance(bits, list) else [bits]
                for i, b in enumerate(items):
                    struct.pack_into(code, data, f.offset + i * TYPES[f.type][0], b)
            alloc.append({'name': bname, 'size': size, 'hex': data.hex()})
            for f in fields:
                if f.out:
                    reads.append({'addr': '@%s+%d' % (bname, f.offset), 'len': f.size})
        writes = []
        for f in self.globals:
            if f.is_input or f.value is not None:
                bits = values[f.name] if f.is_input else f.const_bits()
                code, size = TYPES[f.type][1], TYPES[f.type][0]
                items = bits if isinstance(bits, list) else [bits]
                writes.append({'addr': '0x%08X' % f.addr,
                               'hex': b''.join(struct.pack(code, b) for b in items).hex()})
        for f in self.globals:
            if f.out:
                reads.append({'addr': '0x%08X' % f.addr, 'len': f.size})
        args = []
        for a in self.args:
            if a['type'] == 'ptr' and 'buffer' in a:
                args.append({'type': 'ptr', 'v': '@%s+%d' % (a['buffer'], a.get('offset', 0))})
                continue
            f = next(x for x in self.arg_fields if x is not None and x.name == a['name'])
            bits = values[f.name] if f.is_input else f.const_bits()
            args.append({'type': f.type, 'v': bits_hex(f.type, bits)} if not TYPES[f.type][2]
                        else {'type': f.type, 'bits': bits_hex(f.type, bits)})
        req = {'addr': '0x%08X' % self.addr, 'conv': self.conv, 'ret': self.ret, 'args': args, 'alloc': alloc}
        if self.this is not None:
            req['this'] = '@' + self.this
        if writes:
            req['write'] = writes
        if reads:
            req['read'] = reads
        if self.reset_each:
            req['reset'] = True
        return req

    def outputs_of(self, resp):
        out = {}
        if self.ret_type:
            if self.ret == 'st0':
                out['ret'] = resp['ret_hex']
                out['ret_f32'] = resp['ret_f32_hex']
                out['ret_f64'] = resp['ret_f64_hex']
            else:
                out['ret'] = bits_hex(self.ret_type, int(resp['ret_hex'], 16))
        k = 0
        for f in self.outputs:
            data = bytes.fromhex(resp['read'][k])
            k += 1
            size, code = TYPES[f.type][0], TYPES[f.type][1]
            vals = [bits_hex(f.type, struct.unpack_from(code, data, i * size)[0]) for i in range(f.count)]
            out[f.name] = vals[0] if f.count == 1 else vals
        return out


def hex_values(spec, values):
    out = {}
    for f in spec.inputs:
        v = values[f.name]
        out[f.name] = [bits_hex(f.type, b) for b in v] if isinstance(v, list) else bits_hex(f.type, v)
    return out


def file_info(path):
    h = hashlib.sha1()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return {'path': path, 'size': os.path.getsize(path), 'sha1': h.hexdigest()}


def generate(spec_path, out_dir, random_cases=None, sym=None):
    sym = sym or oracle.symbols()
    spec = Spec(spec_path, sym)
    with oracle.Oracle(sym=sym) as o:
        for req in spec.setup:
            o.request(o._resolve(dict(req)), check=True)
        for st in spec.stubs:
            o.stub(st['target'], st.get('type', 'void'), st.get('value', 0), int(st.get('pop', 0)))
        if spec.fake_game:
            o.fake_game()
        if spec.setup or spec.stubs or spec.fake_game:
            o.snapshot()
        cases = spec.all_cases(random_cases)
        results, errors, warnings = [], 0, {}
        step = 1 if spec.chain else BATCH  # a chain needs each case's outputs before the next request
        previous = None
        for start in range(0, len(cases), step):
            chunk = cases[start:start + step]
            if spec.chain and previous is not None and 'out' in previous:
                for i, out_name in spec.chain.items():
                    f = next(x for x in spec.inputs if x.name == i)
                    v = previous['out'][out_name]
                    chunk[0][i] = [int(x, 16) for x in v] if isinstance(v, list) else int(v, 16)
                    if f.count == 1 and isinstance(chunk[0][i], list):
                        raise SpecError('chain %s: a list for a scalar' % i)
            responses = o.batch([spec.request(c) for c in chunk])
            for values, r in zip(chunk, responses):
                item = {'in': hex_values(spec, values)}
                if r.get('ok'):
                    item['out'] = spec.outputs_of(r)
                else:
                    item['error'] = o.describe(r)
                    errors += 1
                for w in r.get('warn', []):
                    warnings[w] = warnings.get(w, 0) + 1
                results.append(item)
                previous = item
        constants = {}
        for fields in [fs for _, fs in spec.buffers.values()] + [spec.globals]:
            for f in fields:
                if not f.is_input and (f.value is not None or f.buffer != '@global'):
                    b = f.const_bits()
                    constants[f.name] = [bits_hex(f.type, x) for x in b] if isinstance(b, list) else bits_hex(f.type, b)
        doc = {
            'format': FORMAT,
            'function': spec.function,
            'symbol': spec.symbol or '',
            'addr': '0x%08X' % spec.addr,
            'conv': spec.conv,
            'ret': spec.ret,
            'exe': file_info(o.info['exe']),
            'host': o.info['host'],
            'fpu_cw': o.info['fpu_cw'],
            'machine': o.machine,
            'setup': spec.setup,
            **({'stubs': spec.stubs} if spec.stubs else {}),
            **({'fake_game': True} if spec.fake_game else {}),
            **({'chain': spec.chain} if spec.chain else {}),
            'spec': os.path.relpath(spec_path, HERE).replace('\\', '/'),
            'seed': spec.seed,
            'generated': datetime.datetime.now().isoformat(timespec='seconds'),
            'notes': spec.raw.get('notes', ''),
            'inputs': spec.input_schema(),
            'outputs': spec.output_schema(),
            'constants': constants,
            'cases': results,
        }
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, spec.vectors + '.json')
    with open(out, 'w', encoding='utf-8', newline='\n') as f:
        # one case per line: diffable and still plain JSON
        head = dict(doc)
        head.pop('cases')
        text = json.dumps(head, indent=1)
        f.write(text[:-2] + ',\n "cases": [\n')
        for i, c in enumerate(results):
            f.write('  ' + json.dumps(c, separators=(',', ':')) + (',\n' if i + 1 < len(results) else '\n'))
        f.write(' ]\n}\n')
    print('%-32s %5d cases, %d errors -> %s' % (spec.vectors, len(results), errors, os.path.relpath(out, HERE)))
    for w, n in warnings.items():
        print('   warning x%d: %s' % (n, w))
    return out


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('specs', nargs='*')
    ap.add_argument('--all', action='store_true', help='every specs/*.json')
    ap.add_argument('--cases', type=int, help='random cases (overrides the spec)')
    ap.add_argument('--out', default=os.path.join(HERE, 'vectors'))
    a = ap.parse_args(argv)
    paths = list(a.specs)
    if a.all:
        paths += sorted(glob.glob(os.path.join(HERE, 'specs', '*.json')))
    if not paths:
        ap.error('no spec given (or --all)')
    sym = oracle.symbols()
    for p in paths:
        generate(p, a.out, a.cases, sym)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
