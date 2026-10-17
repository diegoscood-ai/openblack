"""Cross-reference index for runblack.exe, the original Black & White, named with bw1-decomp's symbols.

  python xref.py build                 disassemble .text once, write xref.db
  python xref.py writers|readers|refs <addr|a-b|base+off|name>
  python xref.py callers|callees <name-or-addr>
  python xref.py vtcalls <slot-offset> [--class Name]
  python xref.py str <text>
  python xref.py const <float>
  python xref.py func <name-or-addr>
  python xref.py show <name-or-addr> [--max N]    func + its notes + its disassembly with names
  python xref.py note <addr|name> <author> "<summary>" [--text "<more>" | --file <path>]
  python xref.py notes [<addr|name>] [--author A] [--grep W] [--full]

Notes are kept in notes.db, apart from xref.db, so a rebuild keeps them. A note on an
address inside a function shows with that function.

Only the standard library, pefile and capstone. See README.md for limits.
"""
import bisect
import os
import re
import sqlite3
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import re_config  # noqa: E402

# The exe and the symbols are only needed by "build" (and the tests): see README.md for the settings.
EXE = re_config.get('XREF_EXE') or re_config.get('RUNBLACK_EXE')
SYMS = re_config.get('XREF_SYMS') or re_config.get('BW1_SYMBOLS')
DB = re_config.get('XREF_DB', os.path.join(HERE, 'xref.db'))
NOTES = re_config.get('XREF_NOTES', os.path.join(HERE, 'notes.db'))


def exe_path():
    return EXE if EXE and os.path.isfile(EXE) else re_config.runblack_exe()


def symbols_path():
    return SYMS if SYMS and os.path.isfile(SYMS) else re_config.symbols_file()


SYM_RE = re.compile(r"(\S+) = \.(\w+):0x([0-9A-Fa-f]+);(.*)")

# ---------------------------------------------------------------- access rules
# capstone 5's operand access flags are wrong for several instructions (fstp,
# fistp, cmpxchg, movups/movq stores, test), so the kind is decided here.
X87_WRITE = {'fst', 'fstp', 'fist', 'fistp', 'fisttp', 'fbstp', 'fnstsw', 'fstsw', 'fnstcw',
             'fstcw', 'fnstenv', 'fstenv', 'fnsave', 'fsave'}
RW_DEST = {'add', 'sub', 'adc', 'sbb', 'and', 'or', 'xor', 'inc', 'dec', 'neg', 'not', 'shl', 'sal',
           'shr', 'sar', 'rol', 'ror', 'rcl', 'rcr', 'shld', 'shrd', 'xchg', 'xadd', 'cmpxchg',
           'cmpxchg8b', 'bts', 'btr', 'btc'}
R_ONLY = {'cmp', 'test', 'bt', 'push', 'call', 'jmp', 'mul', 'imul', 'div', 'idiv', 'prefetch',
          'prefetchnta', 'prefetcht0', 'prefetcht1', 'prefetcht2', 'prefetchw', 'verr', 'verw',
          'lgdt', 'lidt', 'lldt', 'ltr', 'lmsw', 'bound', 'arpl'}
W_SINGLE = {'pop', 'sgdt', 'sidt', 'sldt', 'str', 'smsw', 'stmxcsr'}


def access_kind(mn, mem_index, nops):
    if mn == 'lea':
        return 'addr'
    if mn.startswith('f'):
        return 'w' if mn in X87_WRITE else 'r'
    if mem_index > 0:
        return 'r'
    if mn in RW_DEST:
        return 'rw'
    if mn in R_ONLY or mn.startswith('j') or mn.startswith('cmov'):
        return 'r'
    if mn.startswith('set') or mn in W_SINGLE:
        return 'w'
    if nops >= 2:
        return 'w'          # mov family, SSE/MMX stores, ...
    return 'r'


def fpu_kind(insn):
    mn = insn.mnemonic
    if mn.startswith('f') and mn not in ('femms',):
        return 'x87'
    s = insn.op_str
    if 'xmm' in s:
        return 'sse'
    if re.search(r'\bmm\d', s):
        return 'mmx'
    return ''


# ---------------------------------------------------------------- inputs
# Symbol sizes that are wrong, read in the exe. CallNearInState's size 0xB10 covers the script CREATE switch: the
# function returns at 0x6F119C; 0x6F11A0..0x6F1B14 is the switch (jump table 0x6F1A70..0x6F1B14), called from
# CreateThing 0x6F1BBC, PopulateContainer 0x6F20CE and CreateWithAngleAndScale 0x6F2EF0 (read 2026-10-15)
SIZE_FIXES = [
    (0x6F1010, 0x6F11A0, [(0x6F11A0, 0x6F1B20, 'fn_006F11A0')]),
]


def load_symbols():
    syms = []
    for line in open(symbols_path(), encoding='utf-8', errors='replace'):
        m = SYM_RE.match(line)
        if not m:
            continue
        rest = m.group(4)
        t = re.search(r'type:(\w+)', rest)
        s = re.search(r'size:0x([0-9A-Fa-f]+)', rest)
        syms.append((int(m.group(3), 16), m.group(1), m.group(2), t.group(1) if t else '',
                     int(s.group(1), 16) if s else None))
    return syms


class Image:
    def __init__(self):
        import pefile
        pe = pefile.PE(exe_path(), fast_load=True)
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.img = bytes(pe.get_memory_mapped_image())
        self.sections = []
        for s in pe.sections:
            name = s.Name.rstrip(b'\0').decode('latin1')
            va = self.base + s.VirtualAddress
            self.sections.append((name, va, va + s.Misc_VirtualSize, va + min(s.SizeOfRawData, s.Misc_VirtualSize)))
        self.lo = self.sections[0][1]
        self.hi = max(s[2] for s in self.sections)

    def sec(self, name):
        return next(s for s in self.sections if s[0] == name)

    def section_of(self, a):
        for n, lo, hi, _ in self.sections:
            if lo <= a < hi:
                return n
        return None

    def bytes(self, a, n):
        o = a - self.base
        return self.img[o:o + n]

    def u32(self, a):
        return struct.unpack_from('<I', self.img, a - self.base)[0]


# ---------------------------------------------------------------- build
HEX = re.compile(r'0x([0-9a-f]{6,8})\b')
JT_RE = re.compile(r'\[(\w+)\*4 \+ 0x([0-9a-f]+)\]')
BT_RE = re.compile(r'byte ptr \[(\w+) \+ 0x([0-9a-f]+)\]')
CMP_RE = re.compile(r'^(\w+), (0x[0-9a-f]+|\d+)$')


def build():
    import capstone
    from capstone import x86
    t0 = time.time()
    im = Image()
    syms = load_symbols()
    tname, tlo, thi, _ = im.sec('.text')
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    mdd = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    mdd.detail = True

    # functions from the symbols: size from the comment, else up to the next function
    fsyms = sorted({(a, n, sz) for a, n, sec, t, sz in syms if sec == 'text' and t == 'function'})
    starts = [f[0] for f in fsyms]
    funcs = []
    for k, (a, n, sz) in enumerate(fsyms):
        nxt = starts[k + 1] if k + 1 < len(starts) else thi
        end = a + sz if sz else nxt
        funcs.append([a, end, n, 'sym' if sz else 'sym-nosize'])
    # symbol sizes read wrong in the exe: (start, real end, [functions in the rest of the old range])
    for a, real_end, extra in SIZE_FIXES:
        for f in funcs:
            if f[0] == a:
                f[1], f[3] = real_end, 'sym-fixed'
        funcs.extend([lo, hi, n, 'fixed'] for lo, hi, n in extra)
    funcs.sort()

    # Some auto-named symbols (fn_XXXXXXXX, not 16-byte aligned) sit in the middle of an
    # instruction: their addresses come from data that looks like a pointer (often ASCII text).
    # When the linear decode of the code before one runs across its start instead of landing on
    # it, the symbol is kept as a name only and its bytes are decoded as part of what precedes it.
    suspects = []
    kept = []
    region = tlo
    for f in funcs:
        a, end, n, k = f
        if n.startswith('fn_') and a % 16 and kept:
            p = kept[-1]
            lo = p[0] if p[1] > a else max(p[1], region)
            last = lo
            for (ia, isz, mn, ops) in md.disasm_lite(im.img[lo - im.base:a + 15 - im.base], lo):
                if ia >= a:
                    break
                last = ia + isz
            if last > a:
                suspects.append((a, n, end))
                if p[1] >= a:
                    p[1] = max(p[1], end)
                continue
        kept.append(f)
        region = a
    funcs = kept

    def in_image(v):
        return im.lo <= v < im.hi

    decoded = []          # (start, end, kind, func)
    calls = []            # (frm, func, target, kind, vt_off, reg, insn)
    refs = []             # (frm, func, target, kind, size, fpu, mode, insn)
    fimms = []            # (frm, func, bits, value, insn)
    jumps = []            # (frm, target, mnemonic, insn) direct jmp/jcc for tail-call detection
    tables = []           # (addr, end, kind, func, entries)
    ninsn = [0]

    def detail(a):
        o = a - im.base
        return next(mdd.disasm(im.img[o:o + 16], a, 1), None)

    def handle(a, sz, mn, ops, func):
        ninsn[0] += 1
        is_call = mn == 'call'
        is_jmp = mn == 'jmp' or (mn.startswith('j') or mn in ('loop', 'loope', 'loopne'))
        if (is_call or is_jmp) and ops.startswith('0x'):
            tgt = int(ops, 16)
            if is_call:
                calls.append((a, func, tgt, 'direct', None, None, '%s %s' % (mn, ops)))
            else:
                jumps.append((a, tgt, mn, '%s %s' % (mn, ops)))
            return
        if not (is_call or is_jmp) and not HEX.search(ops):
            return
        i = detail(a)
        if i is None:
            return
        text = '%s %s' % (i.mnemonic, i.op_str)
        nops = len(i.operands)
        fk = None
        for k, op in enumerate(i.operands):
            if op.type == x86.X86_OP_MEM:
                disp = op.mem.disp & 0xffffffff
                base = op.mem.base
                idx = op.mem.index
                if (is_call or is_jmp):
                    if base == 0 and idx == 0:
                        kind = 'mem' if is_call else 'tailmem'
                        calls.append((a, func, disp, kind, None, None, text))
                    elif idx == 0 and i.reg_name(base) not in ('esp', 'ebp'):
                        if is_call:
                            calls.append((a, func, None, 'vt', disp if disp < 0x80000000 else disp - (1 << 32),
                                          i.reg_name(base), text))
                    elif is_call:
                        calls.append((a, func, disp if in_image(disp) else None, 'memind', None,
                                      i.reg_name(base) if base else i.reg_name(idx), text))
                if not in_image(disp):
                    continue
                mode = 'abs' if (base == 0 and idx == 0) else 'indexed'
                kind = access_kind(i.mnemonic, k, nops)
                if fk is None:
                    fk = fpu_kind(i)
                refs.append((a, func, disp, kind, op.size, fk, mode, text))
            elif op.type == x86.X86_OP_IMM:
                v = op.imm & 0xffffffff
                if (is_call or is_jmp):
                    continue
                if in_image(v):
                    if fk is None:
                        fk = fpu_kind(i)
                    refs.append((a, func, v, 'addr', op.size, fk, 'imm', text))
                elif op.size == 4 and v and (i.mnemonic in ('push', 'mov')):
                    e = (v >> 23) & 0xff
                    if 0 < e < 255:
                        fimms.append((a, func, v, struct.unpack('<f', struct.pack('<I', v))[0], text))
            elif op.type == x86.X86_OP_REG and is_call:
                calls.append((a, func, None, 'reg', None, i.reg_name(op.reg), text))

    def lite(lo, hi, skips):
        """Linear sweep [lo, hi) skipping known data ranges; returns decoded list and invalid ranges."""
        out = []
        bad = []
        pos = lo
        sk = sorted(skips)
        while pos < hi:
            inside = next((e for s, e in sk if s <= pos < e), None)
            if inside is not None:
                pos = inside
                continue
            stop = min([s for s, e in sk if s > pos] + [hi])
            # at the range end the last instruction may cross it (some symbol sizes cut one short)
            lim = min(stop + 15, thi) if stop == hi else stop
            last = pos
            o = pos - im.base
            for (ia, isz, mn, ops) in md.disasm_lite(im.img[o:o + (lim - pos)], pos):
                if ia >= stop:
                    break
                out.append((ia, isz, mn, ops))
                last = ia + isz
            if last >= stop:
                pos = last
                continue
            if last < stop:
                # undecodable byte, or an instruction running past `stop`: skip one byte
                if bad and bad[-1][1] == last:
                    bad[-1] = (bad[-1][0], last + 1)
                else:
                    bad.append((last, last + 1))
                last += 1
            pos = last
        return out, bad

    def find_tables(ins, lo, hi, known):
        new = []
        for k, (ia, isz, mn, ops) in enumerate(ins):
            m = None
            kind = None
            if mn == 'jmp':
                m = JT_RE.search(ops)
                kind = 'jumptable'
            elif mn in ('mov', 'movzx') and 'byte ptr' in ops:
                m = BT_RE.search(ops)
                kind = 'bytetable'
            if not m:
                continue
            t = int(m.group(2), 16)
            if not (tlo <= t < thi) or t in known:
                continue
            reg = m.group(1)
            count = None
            for j in range(k - 1, max(k - 8, -1), -1):
                pm, po = ins[j][2], ins[j][3]
                c = CMP_RE.match(po)
                if pm == 'cmp' and c and c.group(1) == reg:
                    count = int(c.group(2), 0) + 1
                    break
            new.append((t, kind, count))
        return new

    reached = {}          # range start -> end of its last decoded instruction (may pass the range end)
    ext_data = []         # tables found outside the sweep that uses them (in a gap or another range)

    def table_end(t, k, c, lim, lo, hi):
        if k == 'jumptable':
            e = t
            while e + 4 <= lim and (c is None or (e - t) // 4 < c) and lo <= im.u32(e) < hi:
                e += 4
            return e
        return min(lim, t + c) if c else lim

    def sweep(lo, hi, func, kind, pre_skips=()):
        tabs = {}
        skips = list(pre_skips)
        for _ in range(6):
            ins, bad = lite(lo, hi, skips)
            new = find_tables(ins, lo, hi, tabs)
            if not new:
                break
            for t, k, c in new:
                tabs[t] = (k, c)
            # lay out data ranges in address order
            skips = list(pre_skips)
            ts = sorted(t for t in tabs if lo <= t < hi)
            for n, t in enumerate(ts):
                k, c = tabs[t]
                lim = ts[n + 1] if n + 1 < len(ts) else hi
                e = table_end(t, k, c, lim, lo, hi)
                if e > t:
                    skips.append((t, e))
        for t, e in skips:
            if (t, e) in pre_skips:
                continue
            k, c = tabs[t]
            n = (e - t) // 4 if k == 'jumptable' else e - t
            tables.append((t, e, k, func, n))
            decoded.append((t, e, k, func))
        for t, (k, c) in tabs.items():
            if lo <= t < hi or (k == 'bytetable' and not c):
                continue
            e = table_end(t, k, c, min(t + 0x1000, thi), lo, hi)
            if e > t:
                ext_data.append((t, e, k, func, (e - t) // 4 if k == 'jumptable' else e - t))
        for b, e in bad:
            decoded.append((b, e, 'invalid', func))
        run = None
        after_pad = []
        prev_pad = False
        for (ia, isz, mn, ops) in ins:
            pad = mn == 'int3' or (mn == 'nop' and kind == 'gap')
            k = 'padding' if pad else kind
            if not pad and prev_pad:
                after_pad.append(ia)
            prev_pad = pad
            if run and run[1] == ia and run[2] == k:
                run[1] = ia + isz
            else:
                if run:
                    decoded.append((run[0], run[1], run[2], func))
                run = [ia, ia + isz, k]
            if not pad:
                handle(ia, isz, mn, ops, func)
        if run:
            decoded.append((run[0], run[1], run[2], func))
        if ins:
            reached[lo] = ins[-1][0] + ins[-1][1]
        return after_pad

    print('sweeping %d functions...' % len(funcs), flush=True)
    for a, end, n, k in funcs:
        if tlo <= a < end <= thi:
            sweep(a, end, a, 'func')
    t1 = time.time()
    print('  %.1fs, %d instructions' % (t1 - t0, ninsn[0]), flush=True)

    # gaps between known functions
    cover = sorted((f[0], f[1]) for f in funcs)
    gaps = []
    pos = tlo
    for a, e in cover:
        if a > pos:
            gaps.append((pos, a))
        pos = max(pos, e, reached.get(a, e))
    if pos < thi:
        gaps.append((pos, thi))
    sub_starts = set()
    gap_bytes = 0
    for lo, hi in gaps:
        # one sweep per gap; a pseudo function starts at the gap start and after each padding run
        gap_bytes += hi - lo
        sub_starts.add(lo)
        pre = [(max(t, lo), min(e, hi)) for t, e, k, f, n in ext_data if t < hi and e > lo]
        sub_starts.update(sweep(lo, hi, None, 'gap', pre))
    for t, e, k, f, n in ext_data:
        tables.append((t, e, k, f, n))
        decoded.append((t, e, k, f))
    t2 = time.time()
    print('  gaps: %d pieces, %d bytes, %.1fs' % (len(sub_starts), gap_bytes, t2 - t1), flush=True)

    # pseudo functions in the gaps: piece starts + direct call targets landing in gaps
    gap_lo = [g[0] for g in gaps]

    def in_gap(v):
        k = bisect.bisect_right(gap_lo, v) - 1
        return k >= 0 and v < gaps[k][1]

    for c in calls:
        if c[3] == 'direct' and c[2] is not None and in_gap(c[2]):
            sub_starts.add(c[2])
    for s in sorted(sub_starts):
        funcs.append([s, None, 'sub_%06X' % s, 'gap'])
    funcs.sort()
    # ends for pseudo functions: next function/pseudo start or gap end
    fst = [f[0] for f in funcs]
    for k, f in enumerate(funcs):
        if f[1] is None:
            g = gaps[bisect.bisect_right(gap_lo, f[0]) - 1]
            nxt = fst[k + 1] if k + 1 < len(fst) else thi
            f[1] = min(nxt, g[1])
    # owner of gap rows
    sym_end = {}

    def owner(a):
        k = bisect.bisect_right(fst, a) - 1
        for j in range(k, max(k - 4, -1), -1):
            if a < funcs[j][1]:
                return funcs[j][0]
        return None

    calls = [c if c[1] is not None else (c[0], owner(c[0])) + c[2:] for c in calls]
    refs = [r if r[1] is not None else (r[0], owner(r[0])) + r[2:] for r in refs]
    fimms = [r if r[1] is not None else (r[0], owner(r[0])) + r[2:] for r in fimms]
    decoded = [d if d[3] is not None else d[:3] + (owner(d[0]),) for d in decoded]
    tables = [t if t[3] is not None else t[:3] + (owner(t[0]),) + t[4:] for t in tables]
    # tail calls: direct jumps whose target is owned by another function
    for a, tgt, mn, text in jumps:
        fa = owner(a)
        ft = owner(tgt)
        if ft != fa and tlo <= tgt < thi:
                calls.append((a, fa, tgt, 'tail' if mn == 'jmp' else 'tailjcc', None, None, text))

    # strings in .rdata / .data (initialised part)
    strings = []
    for sname in ('.rdata', '.data', '.data1'):
        try:
            n, lo, hi, rawhi = im.sec(sname)
        except StopIteration:
            continue
        blob = im.bytes(lo, rawhi - lo)
        for m in re.finditer(rb'[\x20-\x7e\t\r\n]{4,}(?=\x00)', blob):
            strings.append((lo + m.start(), 'ascii', m.group().decode('latin1')))
        for m in re.finditer(rb'(?:[\x20-\x7e\t\r\n]\x00){4,}(?=\x00\x00)', blob):
            if (lo + m.start()) % 2 == 0:
                strings.append((lo + m.start(), 'utf16', m.group().decode('utf-16-le')))

    # string literal symbols (??_C@_0 narrow, ??_C@_1 wide) catch the short ones the scan skips
    have = {a for a, e, t in strings}
    for a, n, sec, t, sz in syms:
        if n.startswith('??_C@_') and a not in have and im.section_of(a):
            blob = im.bytes(a, 0x400)
            if n.startswith('??_C@_1'):
                k = next((j for j in range(0, len(blob) - 1, 2) if blob[j:j + 2] == b'\x00\x00'), len(blob))
                strings.append((a, 'utf16', blob[:k].decode('utf-16-le', 'replace')))
            else:
                strings.append((a, 'ascii', blob.split(b'\x00', 1)[0].decode('latin1')))

    # vtables from ??_7...@@6B@ symbols
    vtslots = []
    vtables = []
    text_syms = {}
    for a, n, sec, t, sz in syms:
        text_syms.setdefault(a, n)
    vts = sorted((a, n, sz) for a, n, sec, t, sz in syms if n.startswith('??_7') and sec in ('rdata', 'data'))
    vstarts = sorted({a for a, n, sec, t, sz in syms if sec in ('rdata', 'data')})
    for a, n, sz in vts:
        m = re.match(r'\?\?_7(.+?)@@6B(.*)@$', n)
        if m:
            parts = m.group(1).split('@')
            cls = '::'.join(reversed(parts))
            forb = m.group(2).rstrip('@')
            if forb:
                cls += ' {for %s}' % '::'.join(reversed(forb.split('@')))
        else:
            cls = n
        # the symbol size often runs past the table into constants: stop at the first non-code pointer
        k = bisect.bisect_right(vstarts, a)
        lim = a + sz if sz else (vstarts[k] if k < len(vstarts) else a + 0x1000)
        if True:
            cnt = 0
            while a + 4 * cnt < lim and tlo <= im.u32(a + 4 * cnt) < thi:
                cnt += 1
        vtables.append((a, cls, n, cnt))
        for s in range(cnt):
            p = im.u32(a + 4 * s)
            vtslots.append((a, cls, 4 * s, p, text_syms.get(p)))

    # data pointers: aligned dwords in .rdata/.data pointing into the image
    dptrs = []
    for sname in ('.rdata', '.data', '.data1'):
        try:
            n, lo, hi, rawhi = im.sec(sname)
        except StopIteration:
            continue
        blob = im.bytes(lo, rawhi - lo)
        for k, (v,) in enumerate(struct.iter_unpack('<I', blob[:len(blob) // 4 * 4])):
            if im.lo <= v < im.hi:
                dptrs.append((lo + 4 * k, v))

    # float / double constants loaded from memory
    consts = {}
    for r in refs:
        tgt, kind, size, fk, mode = r[2], r[3], r[4], r[5], r[6]
        if mode != 'abs' or kind not in ('r', 'rw'):
            continue
        sec = im.section_of(tgt)
        if size in (4, 8, 10) and (fk in ('x87', 'sse') or sec == '.rdata') and (tgt, size) not in consts:
            b = im.bytes(tgt, size)
            if size == 4:
                v = struct.unpack('<f', b)[0]
            elif size == 8:
                v = struct.unpack('<d', b)[0]
            else:
                mant, se = struct.unpack('<QH', b)
                e = se & 0x7fff
                try:
                    v = 0.0 if e == 0 else (-1) ** (se >> 15) * mant / (1 << 63) * 2.0 ** (e - 16383) if e < 0x7fff else float('nan')
                except OverflowError:
                    v = float('nan')
            if v == v and abs(v) != float('inf'):
                consts[(tgt, size)] = (tgt, size, v, sec)

    t3 = time.time()
    # ---------------------------------------------------------------- write
    tmp = DB + '.tmp'
    if os.path.exists(tmp):
        os.remove(tmp)
    db = sqlite3.connect(tmp)
    c = db.cursor()
    c.executescript('''
    PRAGMA journal_mode=OFF; PRAGMA synchronous=OFF;
    CREATE TABLE meta(key TEXT PRIMARY KEY, value TEXT);
    CREATE TABLE syms(addr INTEGER, name TEXT, section TEXT, type TEXT, size INTEGER);
    CREATE TABLE funcs(addr INTEGER PRIMARY KEY, end INTEGER, name TEXT, kind TEXT);
    CREATE TABLE decoded(start INTEGER, end INTEGER, kind TEXT, func INTEGER);
    CREATE TABLE calls(frm INTEGER, func INTEGER, target INTEGER, kind TEXT, vt_off INTEGER, reg TEXT, insn TEXT);
    CREATE TABLE refs(frm INTEGER, func INTEGER, target INTEGER, kind TEXT, size INTEGER, fpu TEXT, mode TEXT, insn TEXT);
    CREATE TABLE fimms(frm INTEGER, func INTEGER, bits INTEGER, value REAL, insn TEXT);
    CREATE TABLE jtables(addr INTEGER, end INTEGER, kind TEXT, func INTEGER, entries INTEGER);
    CREATE TABLE strings(addr INTEGER, enc TEXT, text TEXT);
    CREATE TABLE suspects(addr INTEGER, name TEXT, end INTEGER);
    CREATE TABLE vtables(addr INTEGER PRIMARY KEY, class TEXT, name TEXT, slots INTEGER);
    CREATE TABLE vtslots(vt INTEGER, class TEXT, slot INTEGER, target INTEGER, target_name TEXT);
    CREATE TABLE dptrs(addr INTEGER, target INTEGER);
    CREATE TABLE consts(addr INTEGER, size INTEGER, value REAL, section TEXT);
    CREATE VIEW imms AS SELECT * FROM refs WHERE mode='imm';
    ''')
    c.executemany('INSERT INTO syms VALUES(?,?,?,?,?)', syms)
    c.executemany('INSERT OR IGNORE INTO funcs VALUES(?,?,?,?)', funcs)
    c.executemany('INSERT INTO decoded VALUES(?,?,?,?)', sorted(decoded))
    c.executemany('INSERT INTO calls VALUES(?,?,?,?,?,?,?)', calls)
    c.executemany('INSERT INTO refs VALUES(?,?,?,?,?,?,?,?)', refs)
    c.executemany('INSERT INTO fimms VALUES(?,?,?,?,?)', fimms)
    c.executemany('INSERT INTO jtables VALUES(?,?,?,?,?)', tables)
    c.executemany('INSERT INTO strings VALUES(?,?,?)', strings)
    c.executemany('INSERT INTO suspects VALUES(?,?,?)', suspects)
    c.executemany('INSERT OR IGNORE INTO vtables VALUES(?,?,?,?)', vtables)
    c.executemany('INSERT INTO vtslots VALUES(?,?,?,?,?)', vtslots)
    c.executemany('INSERT INTO dptrs VALUES(?,?)', dptrs)
    c.executemany('INSERT INTO consts VALUES(?,?,?,?)', list(consts.values()))
    c.executescript('''
    CREATE INDEX syms_addr ON syms(addr); CREATE INDEX syms_name ON syms(name);
    CREATE INDEX funcs_end ON funcs(end);
    CREATE INDEX decoded_start ON decoded(start);
    CREATE INDEX calls_target ON calls(target); CREATE INDEX calls_func ON calls(func);
    CREATE INDEX calls_vt ON calls(vt_off); CREATE INDEX calls_frm ON calls(frm);
    CREATE INDEX refs_target ON refs(target); CREATE INDEX refs_func ON refs(func); CREATE INDEX refs_frm ON refs(frm);
    CREATE INDEX fimms_value ON fimms(value);
    CREATE INDEX strings_addr ON strings(addr);
    CREATE INDEX vtslots_slot ON vtslots(slot); CREATE INDEX vtslots_target ON vtslots(target);
    CREATE INDEX dptrs_target ON dptrs(target);
    CREATE INDEX consts_value ON consts(value);
    ''')
    t4 = time.time()
    meta = {'exe': os.path.basename(exe_path()), 'symbols': os.path.basename(symbols_path()), 'instructions': ninsn[0], 'build_seconds': '%.1f' % (t4 - t0),
            'built': time.strftime('%Y-%m-%d %H:%M:%S'), 'gap_bytes': gap_bytes}
    c.executemany('INSERT INTO meta VALUES(?,?)', [(k, str(v)) for k, v in meta.items()])
    db.commit()
    db.close()
    if os.path.exists(DB):
        os.remove(DB)
    os.rename(tmp, DB)
    print('wrote %s (%.1f MB) in %.1fs' % (DB, os.path.getsize(DB) / 1e6, t4 - t0))
    print('  %d auto-named symbols inside instructions kept as names only' % len(suspects))
    print('  functions %d (%d from symbols), instructions %d, calls %d, memory refs %d (abs %d, indexed %d, imm %d)'
          % (len(funcs), len(fsyms), ninsn[0], len(calls), len(refs),
             sum(r[6] == 'abs' for r in refs), sum(r[6] == 'indexed' for r in refs), sum(r[6] == 'imm' for r in refs)))
    print('  strings %d, vtable slots %d (%d vtables), jump/byte tables %d, data pointers %d, float consts %d, float imms %d'
          % (len(strings), len(vtslots), len(vts), len(tables), len(dptrs), len(consts), len(fimms)))


# ---------------------------------------------------------------- queries
class Q:
    def __init__(self):
        if not os.path.exists(DB):
            sys.exit('no %s: run "python xref.py build" first' % DB)
        self.db = sqlite3.connect(DB)
        self.names = {}
        data = []
        for a, n, sec, t, sz in self.db.execute('SELECT addr,name,section,type,size FROM syms'):
            if a not in self.names or (t == 'function' and not self.names[a][1] == 'function'):
                self.names[a] = (n, t)
            if sec != 'text':
                data.append((a, n, sz))
        vsz = dict((a, 4 * n) for a, n in self.db.execute('SELECT addr, slots FROM vtables'))
        data = [(a, n, vsz.get(a, sz) if n.startswith('??_7') else sz) for a, n, sz in data]
        data.sort()
        self.data = data
        self.dstart = [d[0] for d in data]
        self.funcs = list(self.db.execute('SELECT addr,end,name,kind FROM funcs ORDER BY addr'))
        self.fstart = [f[0] for f in self.funcs]
        self.fbyaddr = {f[0]: f for f in self.funcs}

    def fname(self, a):
        if a is None:
            return '?'
        f = self.fbyaddr.get(a)
        if f:
            return f[2]
        return self.name(a)

    def owner(self, a):
        k = bisect.bisect_right(self.fstart, a) - 1
        if k >= 0 and a < self.funcs[k][1]:
            return self.funcs[k]
        return None

    def name(self, a):
        if a is None:
            return '?'
        if a in self.names:
            return self.names[a][0]
        if 0x401000 <= a < 0x8a9000:
            f = self.owner(a)
            if f:
                return '%s+0x%X' % (f[2], a - f[0])
            return '0x%X' % a
        k = bisect.bisect_right(self.dstart, a) - 1
        if k >= 0:
            da, dn, dsz = self.data[k]
            if (dsz and a < da + dsz) or (not dsz and a - da < 0x40):
                return '%s+0x%X' % (dn, a - da)
        return '0x%X' % a

    def resolve(self, s, want_func=False):
        """Address from hex, exact name or unique substring; exits listing matches if ambiguous."""
        s = s.strip()
        if re.fullmatch(r'0x[0-9A-Fa-f]+', s) or (re.fullmatch(r'[0-9A-Fa-f]{6,8}', s) and re.search(r'\d', s)):
            return int(s, 16)
        rows = list(self.db.execute('SELECT addr,name,section,type FROM syms WHERE name=?', (s,)))
        if not rows:
            rows = list(self.db.execute("SELECT addr,name,section,type FROM syms WHERE instr(name, ?) > 0", (s,)))
            if want_func:
                fr = [r for r in rows if r[2] == 'text']
                rows = fr or rows
        if not rows:
            f = [f for f in self.funcs if s in f[2]]
            if len(f) == 1:
                return f[0][0]
            sys.exit('no symbol matches %r' % s)
        if len({r[0] for r in rows}) > 1:
            print('%d symbols match %r:' % (len(rows), s))
            for r in rows[:60]:
                print('  0x%06X  %-6s %s' % (r[0], r[2], r[1]))
            if len(rows) > 60:
                print('  ...')
            sys.exit(1)
        return rows[0][0]

    def parse_target(self, s):
        """addr | lo-hi | base+off | name  ->  (lo, hi) half-open byte range."""
        m = re.fullmatch(r'(.+?)-(0x[0-9A-Fa-f]+|[0-9A-Fa-f]{6,8})', s)
        if m and not m.group(1).endswith('@'):
            lo = self.resolve(m.group(1))
            hi = int(m.group(2), 16)
            return lo, max(hi, lo + 1)
        m = re.fullmatch(r'(.+)\+(0x[0-9A-Fa-f]+|\d+)', s)
        if m:
            a = self.resolve(m.group(1)) + int(m.group(2), 0)
            return a, a + 1
        a = self.resolve(s)
        return a, a + 1


def fmt_ref(q, r):
    frm, func, tgt, kind, size, fk, mode, insn = r
    ex = (' ' + fk) if fk else ''
    return '  0x%06X  %-40s %-4s %-2s%-4s %-7s %s   -> %s' % (
        frm, q.fname(func)[:40], kind, size, ex, mode, insn, q.name(tgt))


def cmd_refs(q, args, kinds):
    window = 0x100
    if '--window' in args:
        k = args.index('--window')
        window = int(args[k + 1], 0)
        args = args[:k] + args[k + 2:]
    lo, hi = q.parse_target(args[0])
    print('%s 0x%X-0x%X (%s)' % ({'w': 'writers', 'r': 'readers', None: 'refs'}[kinds], lo, hi, q.name(lo)))
    rows = q.db.execute('SELECT * FROM refs WHERE target BETWEEN ? AND ? ORDER BY frm', (lo - 16, hi - 1)).fetchall()
    direct = []
    indexed = []
    for r in rows:
        tgt, kind, size, mode = r[2], r[3], r[4], r[6]
        if mode == 'indexed':
            continue
        if not (tgt < hi and tgt + max(size, 1) > lo):
            continue
        if kinds == 'w' and kind not in ('w', 'rw'):
            continue
        if kinds == 'r' and kind not in ('r', 'rw'):
            continue
        direct.append(r)
    for r in q.db.execute("SELECT * FROM refs WHERE mode='indexed' AND target BETWEEN ? AND ? ORDER BY frm",
                          (lo - window, hi - 1)):
        kind = r[3]
        if kinds == 'w' and kind not in ('w', 'rw'):
            continue
        if kinds == 'r' and kind not in ('r', 'rw'):
            continue
        indexed.append(r)
    for r in direct:
        print(fmt_ref(q, r))
    print('%d direct' % len(direct))
    if indexed:
        print('indexed accesses with base in [0x%X, 0x%X) (element unknown):' % (lo - window, hi))
        for r in indexed:
            print(fmt_ref(q, r))
    if kinds is None:
        dp = q.db.execute('SELECT addr,target FROM dptrs WHERE target>=? AND target<?', (lo, hi)).fetchall()
        if dp:
            print('pointed to from data:')
            for a, t in dp:
                print('  0x%06X  %s -> %s' % (a, q.name(a), q.name(t)))


def cmd_callers(q, args):
    a = q.resolve(args[0], True)
    print('callers of 0x%06X %s' % (a, q.fname(a)))
    targets = [a]
    # an import: calls through the IAT slot and through jmp-thunks to it
    rows = q.db.execute('SELECT * FROM calls WHERE target=? ORDER BY frm', (a,)).fetchall()
    thunks = [r for r in rows if r[3] == 'tailmem']
    for t in thunks:
        f = q.owner(t[0])
        if f and f[0] == t[0]:
            targets.append(t[0])
            rows += q.db.execute('SELECT * FROM calls WHERE target=? ORDER BY frm', (t[0],)).fetchall()
    for r in rows:
        print('  0x%06X  %-50s %-8s %s' % (r[0], q.fname(r[1]), r[3], r[6]))
    print('%d calls/jumps' % len(rows))
    vt = q.db.execute('SELECT vt,class,slot FROM vtslots WHERE target=?', (a,)).fetchall()
    for v, cls, slot in vt:
        n = q.db.execute("SELECT count(*) FROM calls WHERE kind='vt' AND vt_off=?", (slot,)).fetchone()[0]
        print('  in vtable %s slot 0x%X (%d call sites use vt+0x%X; see vtcalls)' % (cls, slot, n, slot))
    other = q.db.execute("SELECT * FROM refs WHERE target=? AND mode='imm' ORDER BY frm", (a,)).fetchall()
    for r in other:
        print('  address taken:' + fmt_ref(q, r)[1:])
    dp = q.db.execute('SELECT addr FROM dptrs WHERE target=?', (a,)).fetchall()
    dp = [d for d in dp if not any(d[0] == v + s for v, c, s in vt)]
    for (d,) in dp:
        print('  pointer in data at 0x%06X %s' % (d, q.name(d)))


def cmd_callees(q, args):
    a = q.resolve(args[0], True)
    f = q.owner(a)
    if not f:
        sys.exit('0x%X is not inside a known function' % a)
    print('callees of 0x%06X %s (0x%X-0x%X)' % (f[0], f[2], f[0], f[1]))
    for r in q.db.execute('SELECT * FROM calls WHERE func=? ORDER BY frm', (f[0],)):
        frm, func, tgt, kind, vto, reg, insn = r
        if kind == 'vt':
            what = 'vt+0x%X via %s' % (vto, reg) if vto >= 0 else 'vt-0x%X via %s' % (-vto, reg)
        elif kind == 'reg':
            what = 'register %s' % reg
        elif kind in ('mem', 'tailmem'):
            what = '[%s]' % q.name(tgt)
        else:
            what = q.fname(tgt)
        print('  0x%06X  %-8s %s' % (frm, kind, what))


def cmd_vtcalls(q, args):
    cls = None
    if '--class' in args:
        k = args.index('--class')
        cls = args[k + 1]
        args = args[:k] + args[k + 2:]
    off = int(args[0], 0) if args[0].lower().startswith('0x') else int(args[0], 16)
    rows = q.db.execute("SELECT * FROM calls WHERE kind='vt' AND vt_off=? ORDER BY frm", (off,)).fetchall()
    print('call [reg+0x%X]: %d sites' % (off, len(rows)))
    for r in rows:
        print('  0x%06X  %-50s %s' % (r[0], q.fname(r[1]), r[6]))
    sql = 'SELECT vt,class,target,target_name FROM vtslots WHERE slot=?'
    p = [off]
    if cls:
        sql += ' AND instr(class, ?) > 0'
        p.append(cls)
    sl = q.db.execute(sql + ' ORDER BY class', p).fetchall()
    print('slot 0x%X in %d vtables%s:' % (off, len(sl), (' matching %r' % cls) if cls else ''))
    for vt, c, t, tn in sl:
        print('  %-50s 0x%06X  %s' % (c, t, tn or q.name(t)))


def cmd_str(q, args):
    text = ' '.join(args)
    rows = q.db.execute('SELECT addr,enc,text FROM strings WHERE instr(lower(text), lower(?)) > 0 ORDER BY addr',
                        (text,)).fetchall()
    print('%d strings contain %r' % (len(rows), text))
    for a, enc, s in rows[:200]:
        print('0x%06X %s %r' % (a, enc, s[:120]))
        end = a + len(s) * (2 if enc == 'utf16' else 1)
        for r in q.db.execute('SELECT * FROM refs WHERE target>=? AND target<? ORDER BY frm', (a, end)):
            print(fmt_ref(q, r))
        for d, t in q.db.execute('SELECT addr,target FROM dptrs WHERE target>=? AND target<?', (a, end)):
            print('  pointer in data at 0x%06X %s' % (d, q.name(d)))
            for r in q.db.execute("SELECT * FROM refs WHERE target BETWEEN ? AND ? AND mode!='imm' ORDER BY frm",
                                  (d - 0x400, d)):
                if r[2] == d or r[6] == 'indexed':
                    print('    ' + fmt_ref(q, r)[2:])
    if len(rows) > 200:
        print('... %d more' % (len(rows) - 200))


def cmd_const(q, args):
    x = float(args[0])
    tol = max(abs(x) * 2e-7, 1e-30)
    print('constants equal to %r (relative tolerance 2e-7)' % x)
    cs = q.db.execute('SELECT addr,size,value,section FROM consts WHERE value BETWEEN ? AND ? ORDER BY addr',
                      (x - tol, x + tol)).fetchall()
    for a, size, v, sec in cs:
        print('0x%06X %s %s = %.9g %s' % (a, {4: 'float', 8: 'double', 10: 'long double'}[size], sec, v, q.name(a)))
        for r in q.db.execute("SELECT * FROM refs WHERE target=? AND mode='abs' ORDER BY frm", (a,)):
            if r[4] == size:
                print(fmt_ref(q, r))
    fi = q.db.execute('SELECT * FROM fimms WHERE value BETWEEN ? AND ? ORDER BY frm', (x - tol, x + tol)).fetchall()
    if fi:
        print('immediate 32-bit operands with these float bits (push/mov imm; may also be an integer):')
        for frm, func, bits, v, insn in fi:
            print('  0x%06X  %-40s %s   (0x%08X)' % (frm, q.fname(func)[:40], insn, bits))


def cmd_func(q, args):
    a = q.resolve(args[0], True)
    f = q.owner(a)
    if not f:
        sys.exit('0x%X is not inside a known function' % a)
    a, end, name, kind = f
    print('%s\n  range 0x%06X-0x%06X (0x%X bytes, %s)' % (name, a, end, end - a, kind))
    dec = q.db.execute('SELECT kind,sum(end-start) FROM decoded WHERE func=? GROUP BY kind', (a,)).fetchall()
    print('  decoded: ' + ', '.join('%s %d' % d for d in dec))
    nc = q.db.execute("SELECT count(*), count(DISTINCT func) FROM calls WHERE target=? AND kind IN ('direct','tail','tailjcc')",
                      (a,)).fetchone()
    print('  callers: %d call sites in %d functions' % nc)
    vt = q.db.execute('SELECT class,slot FROM vtslots WHERE target=?', (a,)).fetchall()
    if vt:
        print('  in vtables: ' + ', '.join('%s+0x%X' % v for v in vt[:12]) + (' ...' if len(vt) > 12 else ''))
    print('  callees:')
    seen = {}
    for frm, func, tgt, ck, vto, reg, insn in q.db.execute('SELECT * FROM calls WHERE func=? ORDER BY frm', (a,)):
        if ck == 'vt':
            key = 'vt+0x%X' % vto
        elif ck == 'reg':
            key = 'register ' + reg
        elif ck in ('mem', 'tailmem'):
            key = '[%s]' % q.name(tgt)
        elif ck == 'memind':
            key = 'table %s' % q.name(tgt)
        else:
            key = q.fname(tgt) + (' (%s)' % ck if ck != 'direct' else '')
        seen[key] = seen.get(key, 0) + 1
    for k, n in seen.items():
        print('    %3dx %s' % (n, k))
    for title, ks in (('writes', ('w', 'rw')), ('reads', ('r', 'rw')), ('address of', ('addr',))):
        g = {}
        for tgt, kind, mode in q.db.execute('SELECT target,kind,mode FROM refs WHERE func=?', (a,)):
            if kind in ks:
                if 0x401000 <= tgt < 0x8a9000 and mode == 'imm':
                    key = q.fname(tgt) if tgt in q.fbyaddr else q.name(tgt)
                else:
                    key = q.name(tgt) + (' [indexed]' if mode == 'indexed' else '')
                g[key] = g.get(key, 0) + 1
        if g:
            print('  %s:' % title)
            for k, n in sorted(g.items()):
                print('    %3dx %s' % (n, k))


# ---------------------------------------------------------------- notes
def notes_db():
    db = sqlite3.connect(NOTES, timeout=30)
    db.execute('CREATE TABLE IF NOT EXISTS notes (id INTEGER PRIMARY KEY, addr INTEGER, at TEXT, lane TEXT, '
               'summary TEXT, text TEXT)')
    db.execute('CREATE INDEX IF NOT EXISTS notes_addr ON notes(addr)')
    return db


def take_opt(args, name, default=None):
    """Removes `name value` from args and returns value."""
    if name in args:
        i = args.index(name)
        if i + 1 >= len(args):
            sys.exit('%s needs a value' % name)
        v = args[i + 1]
        del args[i:i + 2]
        return v
    return default


def take_flag(args, name):
    if name in args:
        args.remove(name)
        return True
    return False


def notes_in(q, db, a):
    """Notes on a, or on any address of the function a is in."""
    f = q.owner(a)
    lo, hi = (f[0], f[1]) if f else (a, a + 1)
    return db.execute('SELECT id,addr,at,lane,summary,text FROM notes WHERE addr>=? AND addr<? ORDER BY addr,id',
                      (lo, hi)).fetchall()


def print_note(q, n, full):
    nid, addr, at, lane, summary, text = n
    print('  #%d  %s  %-11s 0x%06X %s: %s' % (nid, at, lane, addr, q.name(addr), summary))
    if full and text:
        for line in text.splitlines():
            print('        ' + line)


def cmd_note(q, args):
    args = list(args)
    text = take_opt(args, '--text')
    path = take_opt(args, '--file')
    if path:
        text = open(path, encoding='utf-8').read()
    if len(args) != 3:
        sys.exit('usage: note <addr|name> <author> "<summary>" [--text "<more>" | --file <path>]')
    a = q.resolve(args[0])
    db = notes_db()
    cur = db.execute('INSERT INTO notes (addr,at,lane,summary,text) VALUES (?,?,?,?,?)',
                     (a, time.strftime('%Y-%m-%d %H:%M'), args[1], args[2], text))
    db.commit()
    print('note #%d on 0x%06X %s' % (cur.lastrowid, a, q.name(a)))


def cmd_notes(q, args):
    args = list(args)
    lane = take_opt(args, '--author') or take_opt(args, '--lane')
    word = take_opt(args, '--grep')
    full = take_flag(args, '--full')
    db = notes_db()
    if args:
        rows = notes_in(q, db, q.resolve(args[0]))
    else:
        rows = db.execute('SELECT id,addr,at,lane,summary,text FROM notes ORDER BY addr,id').fetchall()
    rows = [n for n in rows if (not lane or n[3] == lane)
            and (not word or word.lower() in ((n[4] or '') + ' ' + (n[5] or '')).lower())]
    print('%d note(s)' % len(rows))
    for n in rows:
        print_note(q, n, full)


def cmd_show(q, args):
    args = list(args)
    limit = int(take_opt(args, '--max', '400'))
    a = q.resolve(args[0], True)
    f = q.owner(a)
    if not f:
        sys.exit('0x%X is not inside a known function' % a)
    cmd_func(q, ['0x%X' % f[0]])
    rows = notes_in(q, notes_db(), f[0])
    print('  notes: %d' % len(rows))
    for n in rows:
        print_note(q, n, True)
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    img = Image()
    noted = {n[1] for n in rows}
    print('  disassembly:')
    count = 0
    for insn in md.disasm(img.bytes(f[0], f[1] - f[0]), f[0]):
        if count == limit:
            print('    ... (--max %d)' % limit)
            break
        names = [q.name(int(h, 16)) for h in HEX.findall(insn.op_str)]
        names = [n for n in names if not n.startswith('0x')]
        mark = '*' if insn.address in noted else ' '
        print('   %s0x%06X  %-8s %-40s%s' % (mark, insn.address, insn.mnemonic, insn.op_str,
                                            ('  ; ' + ', '.join(names)) if names else ''))
        count += 1


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    cmd, args = sys.argv[1], sys.argv[2:]
    if cmd == 'build':
        build()
        return
    need = {'writers': 1, 'readers': 1, 'refs': 1, 'callers': 1, 'callees': 1, 'vtcalls': 1, 'str': 1,
            'const': 1, 'func': 1, 'show': 1, 'note': 3, 'notes': 0}
    if cmd not in need or len(args) < need[cmd]:
        print(__doc__)
        sys.exit(2)
    q = Q()
    try:
        run(q, cmd, args)
        sys.stdout.flush()
    except (BrokenPipeError, OSError) as e:
        if isinstance(e, BrokenPipeError) or getattr(e, 'errno', None) == 22:
            sys.stderr.close()
            os._exit(0)
        raise


def run(q, cmd, args):
    if cmd in ('writers', 'readers', 'refs'):
        cmd_refs(q, args, {'writers': 'w', 'readers': 'r', 'refs': None}[cmd])
    elif cmd == 'callers':
        cmd_callers(q, args)
    elif cmd == 'callees':
        cmd_callees(q, args)
    elif cmd == 'vtcalls':
        cmd_vtcalls(q, args)
    elif cmd == 'str':
        cmd_str(q, args)
    elif cmd == 'const':
        cmd_const(q, args)
    elif cmd == 'func':
        cmd_func(q, args)
    elif cmd == 'show':
        cmd_show(q, args)
    elif cmd == 'note':
        cmd_note(q, args)
    elif cmd == 'notes':
        cmd_notes(q, args)


if __name__ == '__main__':
    main()
