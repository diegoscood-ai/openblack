"""Python client of oracle_host.exe: calls runblack.exe functions natively (see README.md).

    from oracle import Oracle, f32, ptr
    with Oracle() as o:
        r = o.call('?SetYXZMatrixOnly@LHMatrix@@QAEXMMM@Z', 'thiscall', [f32(0.5), f32(0.25), f32(-1.0)],
                   this='@m', alloc={'m': 48}, read=[('@m', 36)])
        print(r['read_bytes'][0].hex())

Command line (GUIDE.md has recipes):
    python oracle.py sym <regex>                      list symbols whose name matches
    python oracle.py quick <fn> [options] [arg ...]   one call, the result's bits and value (conv / ret from the name)
    python oracle.py peek <addr|sym> [type[:count]]   a global's value after the baseline (default u32)
    python oracle.py rng <game|gamefloat|local|localfloat|lh> [--seed S] [--n N] [--arg A]   a draw sequence
    python oracle.py call <fn> [--conv C] [--ret R] [--this X] [--alloc name=size] [--read addr:len] [type:value ...]
    python oracle.py imports                          the exe's imports (every one is a trap)

quick arguments: type:value (i32:-5, u32:0x10, f32:1.5, f32:0x3F800000 = bits, f64:..., ptr:0x...), *type:v1,v2,..
(a pointer to a new buffer holding the values, printed back after the call), *out:N (a pointer to N zero bytes).
quick options: --conv C, --ret R (void i32 u32 u16 u8 bool f32 f64 st0 ...), --this *type:v,.. | *out:N | 0x...,
--set where=type:v[,v..] (write a global first), --get where:type[:count] (read a global after the call),
--stub fn=type:value[/pop][@N[:t,..]] (calls to fn return value instead of running; @N or @t,pK:t,.. records ecx and
the arguments of each call; pK:t copies K dwords behind a pointer), --fake-game (the RNG
seeds' GGame), --seed S / --local-seed S (both imply --fake-game). where = 0xADDR | symbol, optionally +0xOFF.
"""

import json
import math
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import re_config  # noqa: E402

# Settings (environment variables or tools/re/local_config.json, see README.md): ORACLE_HOST, the built host (default:
# oracle_host.exe here); RUNBLACK_EXE, your copy of the original exe; BW1_DECOMP or BW1_SYMBOLS, bw1-decomp's symbols.
# The exe is only ever opened for reading (fopen "rb" in the host); keep a read-only copy for the oracle.
HOST = re_config.get('ORACLE_HOST', os.path.join(HERE, 'oracle_host.exe'))


def exe_path():
    return re_config.runblack_exe()


def symbols_path():
    return re_config.symbols_file()


# Free code space for stubs and scratch cells: the zero tail of .text (its code ends at 0x8A895D, the section's
# pages run to 0x8A9000). It is part of the image (RWX in the host), so reset / snapshot cover it.
POOL_START = 0x8A8A00
POOL_END = 0x8A9000

# The game's random seeds live in the GGame object (heap; pointer at 0xD0195C, null in the oracle):
# GRand::GameRand 0x6DE510 -> GData::Rand 0x510650 on GData at game + 0x205A30, seed at GData + 8 (synced stream);
# GRand::LocalRand 0x6DE570 / LocalFloatRand 0x6DE590 use the seed at game + 0x205A3C. Both end in LHRand 0x7DB600
# (max, &seed): seed = ror32(seed * 9377 + 0x24DF, 13), returns seed % max. GData::Rand logs when [0xCD3C80] != 0.
GGAME_PTR = 0xD0195C
SYNCED_SEED_OFF = 0x205A38
LOCAL_SEED_OFF = 0x205A3C
RAND_LOG_FLAG = 0xCD3C80

# The machine state the game sets up once, applied after the exe is loaded (cpu_baseline): fn_007AC9E0 (called by
# LH3DRender::Open) runs LH3DP3::CheckCPU 0x7ACA00, whose Intel dispatcher (___intel_cpu_indicator_init 0x8A2610)
# picks a body by the CPU family (cpuid, natively) and sets IsPentium4 / HasSSE / HasSSE2 [0xE83A20] (read by __ftol
# 0x7A1400) and the MXCSR flush-to-zero bit. A family 6 CPU (every Intel Core) takes the Pentium III body: HasSSE2 = 0,
# so __ftol takes its x87 branch; a family 0xF one (Pentium 4, AMD K8 / Zen) the Pentium 4 body, HasSSE2 = 1.
# _has_osfxsr_set 0x8A25D0, which the init calls, needs the CRT (signal + setjmp: imports), so while the baseline runs
# its two call sites return 1 (mov eax, 1: what it returns on any OS that saves the SSE state); the bytes are put back.
CPU_INDICATOR = 0xFAC840
OSFXSR_CALLS = (0x8A268B, 0x8A26CC)
HAS_SSE2 = 0xE83A20


class OracleError(RuntimeError):
    def __init__(self, message, response=None):
        super().__init__(message)
        self.response = response or {}


# ------------------------------------------------------------------------------------------------------------------
# Symbols

_SYM_RE = re.compile(r'^(\S+) = (\.\w+):0x([0-9A-Fa-f]+);(.*)$')


def plain_name(name):
    """'?GetYXZ@LHMatrix@@QBEXPAM00@Z' -> 'LHMatrix::GetYXZ'; other names unchanged"""
    if not name.startswith('?') or name.startswith('??'):
        return name
    body = name[1:].split('@@', 1)[0]
    return '::'.join(reversed(body.split('@')))


class Symbols:
    """symbols.txt of bw1-decomp (`Name = .text:0xADDR; // type:function size:0xSIZE ...`)"""

    def __init__(self, path=None):
        path = path or symbols_path()
        self.path = path
        self.by_name = {}
        self.by_plain = {}
        self.entries = []  # (addr, name, size, kind, section), sorted by address
        with open(path, encoding='utf-8', errors='replace') as f:
            for line in f:
                m = _SYM_RE.match(line.strip())
                if not m:
                    continue
                name, section, addr, rest = m.group(1), m.group(2), int(m.group(3), 16), m.group(4)
                size = re.search(r'size:0x([0-9A-Fa-f]+)', rest)
                kind = re.search(r'type:(\w+)', rest)
                entry = (addr, name, int(size.group(1), 16) if size else 0, kind.group(1) if kind else '', section)
                self.entries.append(entry)
                self.by_name.setdefault(name, entry)
                self.by_plain.setdefault(plain_name(name), []).append(entry)
        self.entries.sort()
        self._addrs = [e[0] for e in self.entries]

    def entry(self, name):
        if name in self.by_name:
            return self.by_name[name]
        hits = self.by_plain.get(name, [])
        if not hits:  # 'GetYXZ' for 'LHMatrix::GetYXZ'
            hits = [e for p, es in self.by_plain.items() if p.split('::')[-1] == name for e in es]
        if len(hits) == 1:
            return hits[0]
        if not hits:
            # an auto name (fn_XXXXXXXX) that bw1-decomp has since renamed: the address is in the name
            m = re.fullmatch(r'(?:fn|FUN)_([0-9A-Fa-f]{8})', name)
            if m:
                a = int(m.group(1), 16)
                named = [e for e in self.by_name.values() if e[0] == a]
                return named[0] if named else (a, name, 0, 'function', '.text')
            raise KeyError('no symbol %r' % name)
        raise KeyError('ambiguous symbol %r: %s' % (name, ', '.join(e[1] for e in hits[:8])))

    def addr(self, name):
        return self.entry(name)[0]

    def find(self, pattern):
        rx = re.compile(pattern, re.I)
        return [e for e in self.entries if rx.search(e[1]) or rx.search(plain_name(e[1]))]

    def object_at(self, addr):
        """'Name+0xOFF' of the symbol (any kind) whose range holds addr, else 'no symbol'"""
        import bisect
        i = bisect.bisect_right(self._addrs, addr) - 1
        if i >= 0:
            a, name, size = self.entries[i][:3]
            if addr < a + max(size, 1):
                return name if a == addr else '%s+0x%X' % (name, addr - a)
        return 'no symbol'

    def name_at(self, addr):
        """'Name+0xOFF' of the function symbol at or below addr"""
        import bisect
        i = bisect.bisect_right(self._addrs, addr) - 1
        while i >= 0 and self.entries[i][3] not in ('function', ''):
            i -= 1
        if i < 0:
            return hex(addr)
        a, name, size = self.entries[i][:3]
        return name if a == addr else '%s+0x%X' % (name, addr - a)


_symbols = None


def symbols():
    global _symbols
    if _symbols is None:
        _symbols = Symbols()
    return _symbols


# ------------------------------------------------------------------------------------------------------------------
# Values

FLT_MAX = struct.unpack('<f', b'\xff\xff\x7f\x7f')[0]


def f32_bits(x):
    """The bits of x rounded to the nearest float (ties to even), like a C cast"""
    if isinstance(x, float) and math.isnan(x):
        return 0x7FC00000
    x = float(x)
    try:
        return struct.unpack('<I', struct.pack('<f', x))[0]
    except OverflowError:  # beyond FLT_MAX: inf unless it rounds down to FLT_MAX
        limit = 3.4028235677973366e38  # FLT_MAX + half an ulp: from here up it rounds to inf
        if abs(x) < limit:
            return struct.unpack('<I', struct.pack('<f', math.copysign(FLT_MAX, x)))[0]
        return 0x7F800000 | (0x80000000 if x < 0 else 0)


def bits_f32(b):
    return struct.unpack('<f', struct.pack('<I', b & 0xFFFFFFFF))[0]


def f64_bits(x):
    return struct.unpack('<Q', struct.pack('<d', float(x)))[0]


def bits_f64(b):
    return struct.unpack('<d', struct.pack('<Q', b))[0]


def f80_value(hexstr):
    """An approximate Python float of an 80-bit hex string '0xSEEEMMMMMMMMMMMMMMMM'"""
    v = int(hexstr, 16)
    sign = -1.0 if v >> 79 else 1.0
    exp = (v >> 64) & 0x7FFF
    mant = v & ((1 << 64) - 1)
    if exp == 0x7FFF:
        return math.nan if mant & ((1 << 63) - 1) else sign * math.inf
    if exp == 0 and mant == 0:
        return sign * 0.0
    try:
        return sign * math.ldexp(mant, exp - 16383 - 63)
    except OverflowError:
        return sign * math.inf


def _arg(t, v):
    if t == 'f32':
        return {'type': 'f32', 'bits': '0x%08X' % f32_bits(v)}
    if t == 'f64':
        return {'type': 'f64', 'bits': '0x%016X' % f64_bits(v)}
    if isinstance(v, str):
        return {'type': t, 'v': v}
    return {'type': t, 'v': int(v)}


def f32(v):
    """An f32 argument (a Python float is rounded to float once)"""
    return _arg('f32', v)


def f32b(bits):
    """An f32 argument given by its bits"""
    return {'type': 'f32', 'bits': '0x%08X' % (bits & 0xFFFFFFFF)}


def f64(v):
    return _arg('f64', v)


def i32(v):
    return _arg('i32', v)


def u32(v):
    return _arg('u32', v)


def i16(v):
    return _arg('i16', v)


def u16(v):
    return _arg('u16', v)


def ptr(v):
    """A pointer argument: an address or '@buffer' / '@buffer+off'"""
    return _arg('ptr', v)


def _addr(a, syms=None, target=False):
    """An address for the host: an int, '0x...', '@buffer[+off]', or a symbol name. For a call target a symbol name
    wins over a buffer reference (decorated names like '@DotProduct__7Point2DRP7Point2D@12' start with '@')"""
    if isinstance(a, int):
        return '0x%08X' % a
    if target and isinstance(a, str) and a.startswith('@'):
        syms = syms or symbols()
        if a in syms.by_name:
            return '0x%08X' % syms.by_name[a][0]
    if isinstance(a, str) and (a.startswith('@') or a.startswith('0x') or a.startswith('0X') or a.isdigit()):
        return a
    return '0x%08X' % (syms or symbols()).addr(a)


def pack_f32(values):
    return b''.join(struct.pack('<I', f32_bits(v)) for v in values)


def unpack_f32(data):
    return [struct.unpack_from('<f', data, 4 * i)[0] for i in range(len(data) // 4)]


# Typed values: (struct code of the bits, size)
VALUE_TYPES = {'i8': ('<b', 1), 'u8': ('<B', 1), 'bool': ('<B', 1), 'i16': ('<h', 2), 'u16': ('<H', 2),
               'i32': ('<i', 4), 'u32': ('<I', 4), 'ptr': ('<I', 4), 'i64': ('<q', 8), 'u64': ('<Q', 8),
               'f32': ('<I', 4), 'f64': ('<Q', 8)}


def value_bits(t, v):
    """The bits (an unsigned int) of v as type t: numbers, '0x..' raw bits, 'nan' / 'inf' / '-inf' / '-0'"""
    size = VALUE_TYPES[t][1]
    mask = (1 << (8 * size)) - 1
    if isinstance(v, str):
        s = v.strip()
        if s.lower().startswith(('0x', '-0x')):
            return int(s, 16) & mask
        v = float(s) if t in ('f32', 'f64') else int(s, 0)
    if t == 'f32':
        return f32_bits(v)
    if t == 'f64':
        return f64_bits(v)
    return int(v) & mask


def encode(t, values):
    """The bytes of one value or a list of values of type t"""
    vs = values if isinstance(values, (list, tuple)) else [values]
    code, size = VALUE_TYPES[t]
    return b''.join(struct.pack('<%s' % {1: 'B', 2: 'H', 4: 'I', 8: 'Q'}[size], value_bits(t, v)) for v in vs)


def decode(t, data):
    """[(bits, value)] of the type-t values in data"""
    code, size = VALUE_TYPES[t]
    out = []
    for i in range(len(data) // size):
        bits = struct.unpack_from('<%s' % {1: 'B', 2: 'H', 4: 'I', 8: 'Q'}[size], data, i * size)[0]
        if t == 'f32':
            value = bits_f32(bits)
        elif t == 'f64':
            value = bits_f64(bits)
        else:
            value = struct.unpack_from(code, data, i * size)[0]
        out.append((bits, value))
    return out


def parse_capture(capture):
    """[(deref dwords, type)] from a capture count or list ('u32', 'f32', 'p3:f32'...); 0 dwords = the value itself"""
    if not capture:
        return []
    items = ['u32'] * capture if isinstance(capture, int) else list(capture)
    spec = []
    for item in items:
        deref, t = 0, item
        if item.startswith('p') and item[1:2].isdigit():
            k, _, t = item[1:].partition(':')
            deref, t = int(k), t or 'u32'
        if t not in VALUE_TYPES or VALUE_TYPES[t][1] != 4 or (deref and not 1 <= deref <= 16):
            raise OracleError('capture item %r: a 4-byte type, or pK:type with K 1..16' % item)
        spec.append((deref, t))
    if len(spec) > 12:
        raise OracleError('capture takes at most 12 arguments')
    return spec


def fmt_value(t, bits, value):
    digits = 2 * VALUE_TYPES[t][1]
    if t in ('f32', 'f64'):
        return '0x%0*X %s' % (digits, bits, repr(value))
    return '0x%0*X %d' % (digits, bits, value)


def where(a, syms=None):
    """An absolute address: int, '0x..', 'symbol', 'symbol+0x10' / '0x..+16'"""
    if isinstance(a, int):
        return a
    base, off = a, 0
    if '+' in a:
        head, tail = a.rsplit('+', 1)
        try:
            off = int(tail, 0)
            base = head
        except ValueError:
            pass
    if base.lower().startswith('0x') or base.isdigit():
        return int(base, 0) + off
    return (syms or symbols()).addr(base) + off


# ------------------------------------------------------------------------------------------------------------------
# Signatures from MSVC decorated names (enough to guess the convention and the return type for `quick`)

_MS_CONV = {'A': 'cdecl', 'B': 'cdecl', 'C': 'stdcall', 'E': 'thiscall', 'G': 'stdcall', 'I': 'fastcall'}
_MS_RET = {'X': 'void', 'C': 'i8', 'D': 'i8', 'E': 'u8', 'F': 'i16', 'G': 'u16', 'H': 'i32', 'I': 'u32', 'J': 'i32',
           'K': 'u32', 'M': 'st0', 'N': 'st0', 'O': 'st0', '_N': 'bool', '_J': 'i64', '_K': 'u64', 'P': 'ptr',
           'Q': 'ptr', 'A': 'ptr', 'W': 'i32'}


def guess_signature(name):
    """(conv, ret, note) guessed from a symbol name; ret None when it cannot be told (a class returned by value)"""
    if name.startswith('?') and '@@' in name and not name.startswith('??_'):
        code = name.split('@@', 1)[1]
        if code[:1] == 'Y' or code[:1] in 'CDKLST':
            conv, rest = code[1:2], code[2:]
        elif code[:1] in 'ABEFIJMNQRUV':
            conv, rest = code[2:3], code[3:]
        else:
            return 'cdecl', 'i32', 'unknown decoration: cdecl / i32 assumed'
        if rest.startswith('?A'):
            rest = rest[2:]
        ret = _MS_RET.get(rest[:2]) or _MS_RET.get(rest[:1])
        return _MS_CONV.get(conv, 'cdecl'), ret, ''
    if name.startswith('@') and re.match(r'^@\w+@\d+$', name):
        return 'fastcall', 'i32', 'fastcall decoration: return type assumed i32'
    if re.match(r'^_\w+@\d+$', name):
        return 'stdcall', 'i32', 'stdcall decoration: return type assumed i32'
    return 'cdecl', 'i32', 'no type information: cdecl / i32 assumed (give --conv / --ret)'


# ------------------------------------------------------------------------------------------------------------------
# The host

class Oracle:
    """One oracle_host.exe process. Requests go through request() (raw) or call() (convenient)."""

    def __init__(self, host=HOST, exe=None, baseline=True, sym=None):
        self.sym = sym
        exe = exe or exe_path()
        if not os.path.isfile(host):
            raise OracleError('no oracle host at %s: build it with build.bat (or set ORACLE_HOST)' % host)
        args = [host, '--exe', exe]
        self.proc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, cwd=HERE)
        line = self.proc.stdout.readline()
        if not line:
            raise OracleError('oracle_host exited at start-up (code %s)' % self.proc.wait())
        self.info = json.loads(line)
        if not self.info.get('ok'):
            self.close()
            raise OracleError('oracle_host: ' + self.info.get('error', '?'), self.info)
        self.baseline = None
        self.machine = {}
        self._pool = self._pool_snap = POOL_START
        self._imports = None
        self.stubs = []
        if baseline:
            self.baseline = cpu_baseline if baseline is True else baseline
            self._apply_baseline()

    def _apply_baseline(self):
        if self.baseline:
            self.machine = self.baseline(self) or {}
            # the baseline becomes the state every reset goes back to
            self.snapshot()

    @property
    def symbols(self):
        if self.sym is None:
            self.sym = symbols()
        return self.sym

    def close(self):
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.stdin.write(b'{"cmd":"quit"}\n')
                self.proc.stdin.flush()
                self.proc.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):
                self.proc.kill()
        self.proc = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def _resolve(self, req):
        if 'addr' in req:
            req['addr'] = _addr(req['addr'], self.symbols, target=True)
        return req

    def _send(self, req):
        self.proc.stdin.write(json.dumps(req, separators=(',', ':')).encode() + b'\n')
        self.proc.stdin.flush()
        line = self.proc.stdout.readline()
        if not line:
            raise OracleError('oracle_host died (exit code %s)' % self.proc.poll())
        return json.loads(line)

    def describe(self, r):
        """The error of a response with the exe addresses named"""
        msg = r.get('error', '?')
        for key in ('trap', 'exception', 'timeout'):
            info = r.get(key)
            if info:
                for field in ('from', 'addr', 'eip'):
                    if field in info:
                        msg += ' [%s %s]' % (field, self.symbols.name_at(int(info[field], 16)))
        return msg

    def request(self, req, check=False):
        r = self._send(req)
        if check and not r.get('ok'):
            raise OracleError(self.describe(r), r)
        if 'read' in r and r.get('ok'):
            r['read_bytes'] = [bytes.fromhex(h) for h in r['read']]
        return r

    def batch(self, reqs):
        """Several requests in one round trip; the responses in order"""
        r = self._send({'batch': [self._resolve(dict(q)) for q in reqs]})
        if 'batch' not in r:
            raise OracleError(r.get('error', 'batch failed'), r)
        for item in r['batch']:
            if 'read' in item and item.get('ok'):
                item['read_bytes'] = [bytes.fromhex(h) for h in item['read']]
        return r['batch']

    @staticmethod
    def make_request(fn, conv='cdecl', args=(), ret='void', this=None, alloc=None, write=None, read=None, regs=None,
                     fpu_push=None, fpu_cw=None, timeout_ms=None, reset=False):
        """The JSON request of a call (fn: address, '0x...' or symbol name, resolved by Oracle)"""
        req = {'addr': fn, 'conv': conv, 'ret': ret, 'args': [a if isinstance(a, dict) else _arg(*a) for a in args]}
        if this is not None:
            req['this'] = this if isinstance(this, str) else '0x%08X' % this
        if alloc:
            items = []
            for name, spec in (alloc.items() if isinstance(alloc, dict) else alloc):
                if isinstance(spec, int):
                    items.append({'name': name, 'size': spec})
                elif isinstance(spec, (bytes, bytearray)):
                    items.append({'name': name, 'size': len(spec), 'hex': bytes(spec).hex()})
                else:
                    items.append(dict(spec, name=name))
            req['alloc'] = items
        if write:
            req['write'] = [{'addr': a if isinstance(a, str) else '0x%08X' % a, 'hex': bytes(d).hex()}
                            for a, d in (write.items() if isinstance(write, dict) else write)]
        if read:
            req['read'] = [{'addr': a if isinstance(a, str) else '0x%08X' % a, 'len': n} for a, n in read]
        if regs:
            req['regs'] = {k: (v if isinstance(v, str) else '0x%08X' % (v & 0xFFFFFFFF)) for k, v in regs.items()}
        if fpu_push:
            req['fpu_push'] = list(fpu_push)
        if fpu_cw is not None:
            req['fpu_cw'] = '0x%04X' % fpu_cw
        if timeout_ms is not None:
            req['timeout_ms'] = timeout_ms
        if reset:
            req['reset'] = reset
        return req

    def call(self, fn, conv='cdecl', args=(), ret='void', check=True, set_globals=None, get_globals=None, **kw):
        """Call fn natively. args: dicts (f32(), i32(), ptr() ...) or (type, value) tuples. Raises OracleError on a
        trap / fault unless check=False. The response has 'read_bytes' when 'read' was asked.

        set_globals: [(where, type, value or [values])] written just before the call (in the same request);
        get_globals: [(where, type) or (where, type, count)] read just after it, decoded into r['globals'] as
        [[(bits, value), ...], ...]. `where` is an address, a symbol, or either + offset ('0xD0195C', 'sym+0x10')."""
        req = self._resolve(self.make_request(fn, conv, args, ret, **kw))
        if set_globals:
            req.setdefault('write', [])
            req['write'] += [{'addr': '0x%08X' % where(a, self.symbols), 'hex': encode(t, v).hex()}
                             for a, t, v in set_globals]
        first = len(req.get('read', []))
        gets = [(g[0], g[1], g[2] if len(g) > 2 else 1) for g in (get_globals or [])]
        if gets:
            req.setdefault('read', [])
            req['read'] += [{'addr': '0x%08X' % where(a, self.symbols), 'len': VALUE_TYPES[t][1] * n}
                            for a, t, n in gets]
        r = self.request(req, check=check)
        if gets and r.get('ok'):
            r['globals'] = [decode(t, r['read_bytes'][first + i]) for i, (_, t, _) in enumerate(gets)]
        return r

    def snapshot(self):
        """The current image becomes what reset() goes back to (stubs and globals written so far included)"""
        self.request({'snapshot': True}, check=True)
        self._pool_snap = self._pool
        self._stubs_snap = list(self.stubs)

    def reset(self, pristine=False):
        """Back to the state after the baseline (or the file's image, then the baseline again)"""
        if pristine:
            self.request({'reset': 'pristine'}, check=True)
            self._pool = self._pool_snap = POOL_START
            self.stubs = []
            self._apply_baseline()
        else:
            self.request({'reset': True}, check=True)
            self._pool = self._pool_snap
            self.stubs = list(getattr(self, '_stubs_snap', []))

    # --- globals, stubs, the game's RNG

    def peek(self, addr, t='u32', count=1):
        """[(bits, value)] of a global now"""
        return decode(t, self.read_mem(where(addr, self.symbols), VALUE_TYPES[t][1] * count))

    def poke(self, addr, t, values):
        """Write a global (persists until reset; take a snapshot() to keep it across resets)"""
        self.write_mem(where(addr, self.symbols), encode(t, values))

    def _pool_alloc(self, n):
        start = (self._pool + 15) & ~15
        if start + n > POOL_END - 0x10:  # the last 16 bytes are the fake GGame's seeds
            raise OracleError('stub pool full (%d bytes left); reset() frees the stubs made since the snapshot'
                              % (POOL_END - 0x10 - self._pool))
        self._pool = start + n
        return start

    def import_slot(self, name):
        """The IAT slot of an import: 'KERNEL32.dll!GetTickCount', 'GetTickCount' or 'WSOCK32.dll!#52'"""
        if self._imports is None:
            self._imports = self.imports()
        dll, _, fn = name.rpartition('!')
        hits = [i for i in self._imports if i['name'] == fn and (not dll or i['dll'].lower() == dll.lower())]
        if len(hits) != 1:
            raise OracleError('%s import %r' % ('no' if not hits else 'ambiguous', name))
        return int(hits[0]['iat'], 16)

    def stub(self, target, t='void', value=0, pop=0, capture=0, max_calls=16):
        """Make every call to `target` return `value` (type t: void, an integer type, i64 / u64 in edx:eax, f32 / f64
        on the x87 stack) and pop `pop` argument bytes (stdcall / thiscall callees pop their arguments), without
        running it. target: a function (symbol / address: its first 5 bytes become a jmp to the stub) or an import
        ('KERNEL32.dll!GetTickCount', 'imp:GetTickCount': its IAT slot points at the stub). Each stub counts its
        calls (stub_calls). capture records ecx and the stack arguments of each of the first max_calls calls
        (stub_args): an argument count N, or one item per argument: a VALUE_TYPES name ('u32', 'f32'...) or 'pK:type'
        for a pointer whose K dwords are copied as the call is made (an LHPoint& is 'p3:f32'). Stubs last until
        reset(); take a snapshot() to keep them."""
        spec = parse_capture(capture)
        is_import = isinstance(target, str) and ('!' in target or target.startswith('imp:'))
        if isinstance(target, str) and target.startswith('imp:'):
            target = target[4:]
        if not is_import:
            try:
                addr = where(target, self.symbols)
            except KeyError:
                is_import = True
        rec = 4 + 4 * sum(k or 1 for k, _ in spec)
        records = self._pool_alloc(rec * max_calls) if spec else 0
        code_len = 32 + 12 * len(spec) + 12 * sum(k for k, _ in spec)
        base = self._pool_alloc(48 + (code_len if spec else 0))
        counter, cell, code = base, base + 8, base + 16
        body = b''
        if spec:
            # push eax; push edx; push ecx; mov eax, [counter]; cmp eax, max; jae skip; imul eax, eax, rec;
            # add eax, records; mov [eax], ecx; then per argument: mov edx, [esp + 16 + 4k] and either
            # mov [eax + off], edx or, for a pointer, mov ecx, [edx + 4j]; mov [eax + off], ecx per dword;
            # skip: pop ecx; pop edx; pop eax
            inner = b'\x69\xc0' + struct.pack('<I', rec) + b'\x05' + struct.pack('<I', records) + b'\x89\x08'
            off = 4
            for k, (deref, _) in enumerate(spec):
                inner += b'\x8b\x94\x24' + struct.pack('<I', 16 + 4 * k)
                if not deref:
                    inner += b'\x89\x90' + struct.pack('<I', off)
                    off += 4
                for j in range(deref):
                    inner += b'\x8b\x8a' + struct.pack('<I', 4 * j) + b'\x89\x88' + struct.pack('<I', off)
                    off += 4
            body += (b'\x50\x52\x51\xa1' + struct.pack('<I', counter) + b'\x3d' + struct.pack('<I', max_calls)
                     + b'\x0f\x83' + struct.pack('<I', len(inner)) + inner + b'\x59\x5a\x58')
        body += b'\xff\x05' + struct.pack('<I', counter)  # inc dword [counter]
        data = b''
        if t in ('f32', 'f64'):
            data = encode(t, value)
            body += (b'\xd9\x05' if t == 'f32' else b'\xdd\x05') + struct.pack('<I', cell)  # fld [cell]
        elif t in ('i64', 'u64'):
            bits = value_bits(t, value)
            body += b'\xb8' + struct.pack('<I', bits & 0xFFFFFFFF) + b'\xba' + struct.pack('<I', bits >> 32)
        elif t != 'void':
            bits = value_bits(t, value)
            if t in ('i8', 'i16') and bits >> (8 * VALUE_TYPES[t][1] - 1):  # sign-extend into eax
                bits |= ~((1 << (8 * VALUE_TYPES[t][1])) - 1)
            body += b'\xb8' + struct.pack('<I', bits & 0xFFFFFFFF)  # mov eax, imm32
        body += b'\xc3' if not pop else b'\xc2' + struct.pack('<H', pop)
        self.write_mem(counter, b'\0' * 8 + data.ljust(8, b'\0') + body)
        if is_import:
            slot = self.import_slot(target)
            self.write_mem(slot, struct.pack('<I', code))
            info = {'target': target, 'iat': slot}
        else:
            entry = self.symbols.entries[self.symbols._addrs.index(addr)] if addr in self.symbols._addrs else None
            if entry and 0 < entry[2] < 5:
                raise OracleError('%s is %d bytes: too short for a jmp; stub its callers or its import instead'
                                  % (entry[1], entry[2]))
            self.write_mem(addr, b'\xe9' + struct.pack('<i', code - (addr + 5)))
            info = {'target': target, 'addr': addr}
        info.update({'thunk': code, 'counter': counter, 'type': t, 'value': value, 'pop': pop, 'capture': spec,
                     'records': records, 'max_calls': max_calls})
        self.stubs.append(info)
        return info

    def stub_calls(self, stub):
        """How many times a stub was called since it was made (or since the last reset to a snapshot holding it)"""
        return struct.unpack('<I', self.read_mem(stub['counter'], 4))[0]

    def stub_args(self, stub):
        """The calls a capture stub recorded, at most max_calls of them: [{'ecx': bits, 'args': [...]}], each
        argument a (bits, value) pair, or for a 'pK' argument a list of K of them"""
        spec = stub.get('capture')
        if not spec:
            raise OracleError('stub %s was made without capture' % stub['target'])
        n = min(self.stub_calls(stub), stub['max_calls'])
        words_per = sum(k or 1 for k, _ in spec)
        rec = 4 + 4 * words_per
        raw = self.read_mem(stub['records'], rec * n) if n else b''
        out = []
        for i in range(n):
            words = struct.unpack('<%dI' % (1 + words_per), raw[rec * i:rec * (i + 1)])
            args, w = [], 1
            for deref, t in spec:
                vals = [decode(t, struct.pack('<I', x))[0] for x in words[w:w + (deref or 1)]]
                args.append(vals if deref else vals[0])
                w += deref or 1
            out.append({'ecx': words[0], 'args': args})
        return out

    def fake_game(self, synced=None, local=None):
        """Point the GGame pointer at a fake object whose two seeds are the pool's last cells, so GRand::GameRand /
        GameFloatRand (synced) and LocalRand / LocalFloatRand (local) run without the game. Returns (synced, local)
        seed addresses. The real GGame layout is kept: seed addresses = pointer + 0x205A38 / + 0x205A3C."""
        cell = POOL_END - 0x10
        game = cell - SYNCED_SEED_OFF
        writes = [(GGAME_PTR, struct.pack('<I', game)), (RAND_LOG_FLAG, b'\0\0\0\0')]
        if synced is not None:
            writes.append((cell, encode('u32', synced)))
        if local is not None:
            writes.append((game + LOCAL_SEED_OFF, encode('u32', local)))
        self.request({'write': [{'addr': '0x%08X' % a, 'hex': d.hex()} for a, d in writes]}, check=True)
        return cell, game + LOCAL_SEED_OFF

    RNG_KINDS = {  # kind: (function, args of one draw given the argument, ret, seed: 'synced' | 'local' | 'lh')
        'game': ('?GameRand@GRand@@SAIIPBDI@Z', lambda a: [u32(a), u32(0), u32(0)], 'u32', 'synced'),
        'gamefloat': ('?GameFloatRand@GRand@@SAMMPBDI@Z', lambda a: [f32(a), u32(0), u32(0)], 'st0', 'synced'),
        'local': ('?LocalRand@GRand@@SAIJ@Z', lambda a: [i32(a)], 'u32', 'local'),
        'localfloat': ('?LocalFloatRand@GRand@@SAMM@Z', lambda a: [f32(a)], 'st0', 'local'),
    }

    def rng(self, kind, arg, n=1, seed=None):
        """n draws of the game's RNG in one round trip: kind 'game' (GameRand(arg)), 'gamefloat' (GameFloatRand(arg)),
        'local' (LocalRand(arg)), 'localfloat' (LocalFloatRand(arg)) or 'lh' (LHRand 0x7DB600(arg, &seed), the
        generator under all four). seed: the stream's seed before the first draw (None: as it is now).
        Returns [(ret bits, ret value, seed after)]; float draws give the f32 a caller stores."""
        synced_cell, local_cell = self.fake_game()
        if kind == 'lh':
            fn, make_args, ret, cell = '_LHRand__FlRU', (lambda a: [u32(a), ptr('0x%08X' % synced_cell)]), 'u32', \
                synced_cell
        else:
            fn, make_args, ret, stream = self.RNG_KINDS[kind]
            cell = synced_cell if stream == 'synced' else local_cell
        reqs = []
        for i in range(n):
            q = self.make_request(fn, 'cdecl', make_args(arg), ret, read=[(cell, 4)])
            if i == 0 and seed is not None:
                q['write'] = [{'addr': '0x%08X' % cell, 'hex': encode('u32', seed).hex()}]
            reqs.append(q)
        out = []
        for r in self.batch(reqs):
            if not r.get('ok'):
                raise OracleError(self.describe(r), r)
            after = struct.unpack('<I', r['read_bytes'][0])[0]
            if ret == 'st0':
                bits = int(r['ret_f32_hex'], 16)
                out.append((bits, bits_f32(bits), after))
            else:
                bits = int(r['ret_hex'], 16)
                out.append((bits, bits, after))
        return out

    def read_mem(self, addr, n):
        return self.request({'read': [{'addr': _addr(addr, self.symbols), 'len': n}]}, check=True)['read_bytes'][0]

    def write_mem(self, addr, data):
        self.request({'write': [{'addr': _addr(addr, self.symbols), 'hex': bytes(data).hex()}]}, check=True)

    def imports(self):
        return self.request({'cmd': 'imports'}, check=True)['imports']


def cpu_baseline(o):
    """LH3DP3::CheckCPU as the game runs it at start-up (see CPU_INDICATOR above); returns the flags it set"""
    original = [o.read_mem(a, 5) for a in OSFXSR_CALLS]
    o.request({'write': [{'addr': '0x%08X' % a, 'hex': 'b801000000'} for a in OSFXSR_CALLS]}, check=True)
    try:
        o.call('fn_007AC9E0', 'cdecl', [], 'void')
    finally:
        o.request({'write': [{'addr': '0x%08X' % a, 'hex': b.hex()} for a, b in zip(OSFXSR_CALLS, original)]},
                  check=True)
    flags = o.read_mem(0xE83A20, 0x20)
    return {
        'baseline': 'fn_007AC9E0 (LH3DP3::CheckCPU) run natively',
        'cpu': os.environ.get('PROCESSOR_IDENTIFIER', '?'),
        'intel_cpu_indicator': '0x%X' % struct.unpack('<I', o.read_mem(CPU_INDICATOR, 4))[0],
        'HasSSE2': struct.unpack_from('<I', flags, 0)[0],
        'HasSSE': struct.unpack_from('<I', flags, 4)[0],
        'IsPentium4': flags[0x10],
        'game_mxcsr': o.request({'cmd': 'state'}, check=True)['game_mxcsr'],
    }


# ------------------------------------------------------------------------------------------------------------------
# Command line

def _cli(argv):
    if not argv or argv[0] in ('-h', '--help'):
        print(__doc__)
        return 0
    cmd, rest = argv[0], argv[1:]
    if cmd == 'sym':
        if len(rest) != 1:
            print('usage: oracle.py sym <regex>')
            return 2
        for addr, name, size, kind, section in symbols().find(rest[0])[:200]:
            print('0x%08X %-8s size 0x%-5X %s  (%s)' % (addr, kind, size, name, plain_name(name)))
        return 0
    if cmd == 'imports':
        with Oracle(baseline=False) as o:
            for imp in o.imports():
                print(imp['iat'], imp['dll'] + '!' + imp['name'])
        return 0
    if cmd == 'call':
        if not rest:
            print('usage: oracle.py call <fn> [options] [type:value ...]')
            return 2
        fn, opts, args, alloc, reads = rest[0], {'conv': 'cdecl', 'ret': 'void'}, [], {}, []
        it = iter(rest[1:])
        for a in it:
            if a in ('--conv', '--ret', '--this'):
                opts[a[2:]] = next(it)
            elif a == '--alloc':
                name, size = next(it).split('=')
                alloc[name] = int(size, 0)
            elif a == '--read':
                at, n = next(it).rsplit(':', 1)
                reads.append((at if at.startswith("@") else int(at, 0), int(n, 0)))
            elif ':' in a:
                t, v = a.split(':', 1)
                if t in ('f32', 'f64'):
                    args.append({'type': t, 'bits': v} if v.lower().startswith('0x') else _arg(t, float(v)))
                else:
                    args.append({'type': t, 'v': v if (v.startswith('@') or v.lower().startswith('0x')) else int(v)})
            else:
                print('bad argument %r (want type:value)' % a)
                return 2
        with Oracle() as o:
            r = o.call(fn, opts['conv'], args, opts['ret'], check=False, this=opts.get('this'), alloc=alloc or None,
                       read=reads or None)
            r.pop('read_bytes', None)
            print(json.dumps(r, indent=1))
            return 0 if r.get('ok') else 1
    if cmd == 'quick':
        return _quick(rest)
    if cmd == 'peek':
        if not 1 <= len(rest) <= 2:
            print('usage: oracle.py peek <addr|sym[+off]> [type[:count]]')
            return 2
        t, _, n = (rest[1] if len(rest) > 1 else 'u32').partition(':')
        with Oracle() as o:
            a = where(rest[0], o.symbols)
            vals = o.peek(a, t, int(n, 0) if n else 1)
            print('%s (0x%08X, %s):' % (rest[0], a, o.symbols.object_at(a)))
            for i, (bits, value) in enumerate(vals):
                print('  [%d] %s' % (i, fmt_value(t, bits, value)))
        return 0
    if cmd == 'rng':
        return _rng(rest)
    print('unknown command %r' % cmd)
    return 2


def _parse_scalar(t, v):
    if t in ('f32', 'f64'):
        return {'type': t, 'bits': '0x%0*X' % (8 if t == 'f32' else 16, value_bits(t, v))}
    if t not in VALUE_TYPES:
        raise ValueError('unknown type %r' % t)
    return {'type': t, 'v': v if v.startswith('@') else '0x%X' % value_bits(t, v)}


def _parse_buffer(spec):
    """'*f32:1,2,3' -> ('f32', bytes); '*out:12' -> ('out', 12 zero bytes)"""
    t, _, vals = spec[1:].partition(':')
    if t == 'out':
        return 'out', bytes(int(vals, 0))
    return t, encode(t, vals.split(','))


def _show_buffer(label, t, data, limit=256):
    if len(data) > limit:
        print('%s (%d bytes, the first %d shown)' % (label, len(data), limit))
        data = data[:limit]
    if t == 'out':
        print('%s %s' % (label, data.hex()))
        if len(data) % 4 == 0:
            print('   as f32: %s' % ' '.join(repr(v) for _, v in decode('f32', data)))
            print('   as i32: %s' % ' '.join(str(v) for _, v in decode('i32', data)))
        return
    print('%s %s' % (label, ', '.join(fmt_value(t, b, v) for b, v in decode(t, data))))


def _quick(argv):
    """One call: python oracle.py quick <fn> [options] [args]; see the module docstring"""
    if not argv:
        print('usage: oracle.py quick <fn> [--conv C] [--ret R] [--this X] [--set w=t:v] [--get w:t[:n]] '
              '[--stub fn=t:v[/pop][@N[:t,..]]] [--fake-game] [--seed S] [--local-seed S] [--json] [arg ...]')
        return 2
    fn, rest = argv[0], argv[1:]
    conv = ret = this = None
    sets, gets, stubs, args, bufs = [], [], [], [], []
    fake, seed, local_seed, as_json = False, None, None, False
    it = iter(rest)
    try:
        for a in it:
            if a == '--conv':
                conv = next(it)
            elif a == '--ret':
                ret = next(it)
            elif a == '--this':
                this = next(it)
            elif a == '--set':
                w, _, tv = next(it).partition('=')
                t, _, v = tv.partition(':')
                sets.append((w, t, v.split(',')))
            elif a == '--get':
                parts = next(it).split(':')
                gets.append((parts[0], parts[1], int(parts[2], 0) if len(parts) > 2 else 1))
            elif a == '--stub':
                target, _, tv = next(it).partition('=')
                tv, _, cap = tv.partition('@')
                tv, _, pop = tv.partition('/')
                t, _, v = tv.partition(':')
                stubs.append((target, t or 'void', v or 0, int(pop, 0) if pop else 0,
                              (int(cap) if cap.isdigit() else cap.split(',')) if cap else 0))
            elif a == '--fake-game':
                fake = True
            elif a == '--seed':
                seed, fake = next(it), True
            elif a == '--local-seed':
                local_seed, fake = next(it), True
            elif a == '--json':
                as_json = True
            elif a.startswith('*'):
                name = 'a%d' % len(args)
                t, data = _parse_buffer(a)
                bufs.append((name, t, data))
                args.append({'type': 'ptr', 'v': '@' + name})
            elif ':' in a:
                t, v = a.split(':', 1)
                args.append(_parse_scalar(t, v))
            else:
                print('bad argument %r (want type:value, *type:v,v,.. or *out:N)' % a)
                return 2
    except (StopIteration, ValueError) as e:
        print('bad command line: %s' % (e or 'an option lacks its value'))
        return 2
    with Oracle() as o:
        try:
            if fn.lower().startswith('0x'):  # an address: the symbol that starts there, if any
                a = int(fn, 16)
                i = o.symbols._addrs.index(a) if a in o.symbols._addrs else -1
                entry = o.symbols.entries[i] if i >= 0 else None
            else:
                entry = o.symbols.entry(fn)
        except KeyError as e:
            print(e)
            return 2
        name = entry[1] if entry else fn
        g_conv, g_ret, note = guess_signature(name)
        if conv and ret:
            note = ''
        elif note and (conv or ret):
            note = 'no type information: %s assumed' % ('cdecl' if ret else 'i32 return')
        conv = conv or g_conv
        if ret is None:
            ret = g_ret or 'u32'
            if g_ret is None:
                note = 'returns a class by value (hidden pointer): ret u32 shown'
        if this is not None:
            if this.startswith('*'):
                t, data = _parse_buffer(this)
                bufs.append(('this', t, data))
                this = '@this'
        for s in stubs:
            o.stub(*s)
        seed_cells = o.fake_game(seed, local_seed) if fake else None
        r = o.call(fn, conv, args, ret, check=False, this=this, alloc=[(n, d) for n, _, d in bufs] or None,
                   read=[('@' + n, len(d)) for n, _, d in bufs] or None, set_globals=sets or None,
                   get_globals=gets or None, timeout_ms=5000)
        if as_json:
            r.pop('read_bytes', None)
            print(json.dumps(r, indent=1, default=str))
            return 0 if r.get('ok') else 1
        print('%s  0x%08X  %s, ret %s%s' % (plain_name(name), entry[0] if entry else where(fn, o.symbols), conv, ret,
                                            ('  (' + note + ')') if note else ''))
        if not r.get('ok'):
            print('FAILED: %s' % o.describe(r))
            return 1
        if ret == 'st0':
            f32b_ = int(r['ret_f32_hex'], 16)
            f64b_ = int(r['ret_f64_hex'], 16)
            print('ret f32 0x%08X %r   (fstp dword)' % (f32b_, bits_f32(f32b_)))
            print('    f64 0x%016X %r   (fstp qword)' % (f64b_, bits_f64(f64b_)))
            print('    f80 %s' % r['ret_hex'])
        elif ret != 'void':
            t = {'bool': 'u8', 'ptr': 'u32'}.get(ret, ret)
            bits = int(r['ret_hex'], 16)
            if t in VALUE_TYPES:
                print('ret %s' % fmt_value(t, bits, decode(t, encode(t, '0x%X' % bits))[0][1]))
            else:
                print('ret %s' % r['ret_hex'])
        print('    eax %s edx %s, callee popped %s bytes, x87 depth %s' % (r['eax'], r['edx'], r['callee_pop'],
                                                                         r['fpu_depth']))
        for i, (n, t, _) in enumerate(bufs):
            _show_buffer('*%s after:' % n, t, r['read_bytes'][i])
        for (w, t, _), vals in zip(gets, r.get('globals', [])):
            print('get %s: %s' % (w, ', '.join(fmt_value(t, b, v) for b, v in vals)))
        if seed_cells:
            s, l = (struct.unpack('<I', o.read_mem(c, 4))[0] for c in seed_cells)
            print('seeds after: synced 0x%08X, local 0x%08X' % (s, l))
        for s in o.stubs:
            print('stub %s: called %d times' % (s['target'], o.stub_calls(s)))
            if s.get('capture'):
                for c, call in enumerate(o.stub_args(s)):
                    shown = []
                    for (deref, t), arg in zip(s['capture'], call['args']):
                        vals = arg if deref else [arg]
                        txt = ', '.join(fmt_value(t, b, v) for b, v in vals)
                        shown.append('[%s]' % txt if deref else txt)
                    print('  call %d: ecx 0x%08X, args %s' % (c, call['ecx'], '; '.join(shown)))
        for w in r.get('warn', []):
            print('warning: %s' % w)
    return 0


def _rng(argv):
    kinds = ('game', 'gamefloat', 'local', 'localfloat', 'lh')
    if not argv or argv[0] not in kinds:
        print('usage: oracle.py rng <%s> [--seed S] [--n N] [--arg A]' % '|'.join(kinds))
        return 2
    kind, seed, n, arg = argv[0], None, 10, None
    it = iter(argv[1:])
    for a in it:
        if a == '--seed':
            seed = int(next(it), 0)
        elif a == '--n':
            n = int(next(it), 0)
        elif a == '--arg':
            arg = next(it)
        else:
            print('unknown option %r' % a)
            return 2
    if arg is None:
        arg = '1.0' if 'float' in kind else '100'
    arg = float(arg) if 'float' in kind else int(arg, 0)
    with Oracle() as o:
        rows = o.rng(kind, arg, n, seed)
    print('%s(%s), seed before: %s' % (kind, arg, '0x%08X' % seed if seed is not None else 'as the baseline left it'))
    for i, (bits, value, after) in enumerate(rows):
        shown = '0x%08X %r' % (bits, value) if 'float' in kind else '%d' % value
        print('%4d  %-32s seed after 0x%08X' % (i, shown, after))
    return 0


if __name__ == '__main__':
    sys.exit(_cli(sys.argv[1:]))
