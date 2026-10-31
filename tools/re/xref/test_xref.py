"""Checks xref.db against facts confirmed by disassembling runblack.exe directly with capstone.

  python test_xref.py        (needs xref.db: python xref.py build)
"""
import os
import random
import re
import sqlite3
import struct
import subprocess
import sys
import tempfile
import unittest

import capstone
import pefile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xref  # noqa: E402
import re_config  # noqa: E402

HERE = xref.HERE


def setUpModule():
    global IMG, BASE, MD, DB, Q
    pe = pefile.PE(xref.exe_path(), fast_load=True)
    BASE = pe.OPTIONAL_HEADER.ImageBase
    IMG = bytes(pe.get_memory_mapped_image())
    MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    DB = sqlite3.connect(xref.DB)
    Q = xref.Q()


def insn(a):
    i = next(MD.disasm(IMG[a - BASE:a - BASE + 16], a, 1))
    return '%s %s' % (i.mnemonic, i.op_str)


def refs_at(addr, kinds=None):
    rows = DB.execute("SELECT frm, kind FROM refs WHERE target=? AND mode='abs'", (addr,)).fetchall()
    return {f for f, k in rows if kinds is None or k in kinds}


class KnownFacts(unittest.TestCase):
    def test_writer_mov_imm(self):
        self.assertEqual(insn(0x5E5903), 'mov dword ptr [0xd20184], 0')
        self.assertIn(0x5E5903, refs_at(0xD20184, ('w', 'rw')))
        self.assertNotIn(0x5E5903, refs_at(0xD20184, ('r', 'rw')))

    def test_writer_fstp(self):
        # capstone 5 flags fstp's operand as a read; the index must call it a write
        self.assertEqual(insn(0x5E5985), 'fstp dword ptr [0xd20184]')
        r = DB.execute('SELECT kind, fpu, size FROM refs WHERE frm=0x5E5985').fetchone()
        self.assertEqual(r, ('w', 'x87', 4))

    def test_reader(self):
        self.assertEqual(insn(0x5E59E8), 'mov eax, dword ptr [0xd20184]')
        self.assertIn(0x5E59E8, refs_at(0xD20184, ('r',)))
        self.assertEqual(insn(0x5E4D89), 'fld dword ptr [0xd20184]')
        self.assertIn(0x5E4D89, refs_at(0xD20184, ('r',)))

    def test_writers_query_overlap(self):
        # a write to 0xD20180 must not show for 0xD20184, a range must show both
        lo, hi = Q.parse_target('0xD20180-0xD20190')
        self.assertEqual((lo, hi), (0xD20180, 0xD20190))
        rows = DB.execute("SELECT frm FROM refs WHERE target>=? AND target<?", (lo, hi)).fetchall()
        self.assertIn((0x5E54A7,), rows)
        self.assertEqual(insn(0x5E54A7), 'mov dword ptr [0xd20180], eax')

    def test_struct_field_form(self):
        a, _ = Q.parse_target('?g_camera@LH3DTech@@2ULH3DCamera@@A+0x8')
        self.assertEqual(a, 0xEA1DC0)
        self.assertEqual(insn(0x819A32), 'mov dword ptr [0xea1dc0], edx')
        self.assertIn(0x819A32, refs_at(0xEA1DC0, ('w',)))

    def test_direct_call(self):
        self.assertEqual(insn(0x5E488E), 'call 0x5e5830')
        r = DB.execute('SELECT func, kind FROM calls WHERE frm=0x5E488E').fetchone()
        self.assertEqual(r[1], 'direct')
        self.assertEqual(Q.fname(r[0]), '?Draw@GLandscape@@QAEXXZ')

    def test_callees(self):
        ftol = Q.resolve('__ftol')
        n = DB.execute("SELECT count(*) FROM calls WHERE func=0x5E5830 AND target=?", (ftol,)).fetchone()[0]
        self.assertEqual(n, 2)

    def test_vtable_call(self):
        self.assertEqual(insn(0x4011C8), 'call dword ptr [eax + 4]')
        r = DB.execute('SELECT kind, vt_off, reg FROM calls WHERE frm=0x4011C8').fetchone()
        self.assertEqual(r, ('vt', 4, 'eax'))

    def test_vtable_slots(self):
        vt = Q.resolve('??_7Base@@6B@')
        rows = DB.execute('SELECT slot, target FROM vtslots WHERE vt=? ORDER BY slot', (vt,)).fetchall()
        # 7 slots: the symbol's eighth dword is the next vtable's RTTI locator, not code
        self.assertEqual(len(rows), 7)
        self.assertNotEqual(xref.Image().section_of(struct.unpack_from('<I', IMG, vt - BASE + 28)[0]), '.text')
        for slot, t in rows:
            self.assertEqual(t, struct.unpack_from('<I', IMG, vt - BASE + slot)[0])
        self.assertEqual(Q.fname(rows[1][1]), '??_GBase@@UAEPAXI@Z')
        self.assertEqual(Q.fname(rows[2][1]), '?Delete@Base@@UAEXXZ')

    def test_vtable_stops_at_constants(self):
        # PSysBase's symbol size runs into the float constants after it
        vt = Q.resolve('??_7PSysBase@@6B@')
        n = DB.execute('SELECT slots FROM vtables WHERE addr=?', (vt,)).fetchone()[0]
        self.assertLess(n * 4, 0x118)
        self.assertNotIn('PSysBase', Q.name(0x8C9D38))

    def test_import_call(self):
        a = Q.resolve('__imp__RegCloseKey@4')
        rows = DB.execute("SELECT frm FROM calls WHERE target=? AND kind='mem'", (a,)).fetchall()
        self.assertTrue(rows)
        for (f,) in rows:
            self.assertEqual(insn(f), 'call dword ptr [0x8a9004]')

    def test_strings(self):
        r = DB.execute("SELECT addr FROM strings WHERE text=?", ('.\\scripts\\land1.txt',)).fetchone()
        self.assertEqual(r[0], 0xBFE1FC)
        r = DB.execute("SELECT addr FROM strings WHERE text='LEAVE_THROUGH_VORTEX_LAND1'").fetchone()
        self.assertEqual(insn(0x415CF1), 'mov dword ptr [esp + 0x15c], 0x%x' % r[0])
        self.assertIn((0x415CF1, 'addr', 'imm'),
                      DB.execute('SELECT frm, kind, mode FROM refs WHERE target=?', (r[0],)).fetchall())
        # a short literal only known from its ??_C symbol
        self.assertTrue(DB.execute("SELECT 1 FROM strings WHERE addr=0x9A084C AND text='Dec'").fetchone())

    def test_const(self):
        self.assertEqual(struct.unpack_from('<f', IMG, 0x8AA3B4 - BASE)[0], 0.5)
        r = DB.execute('SELECT size, value FROM consts WHERE addr=0x8AA3B4').fetchone()
        self.assertEqual(r, (4, 0.5))
        self.assertEqual(insn(0x404C6E), 'fmul dword ptr [0x8aa3b4]')
        self.assertIn(0x404C6E, refs_at(0x8AA3B4, ('r',)))

    def test_float_immediate(self):
        self.assertEqual(insn(0x5E598F), 'mov dword ptr [0xd20184], 0x3f800000')
        r = DB.execute('SELECT value FROM fimms WHERE frm=0x5E598F').fetchone()
        self.assertEqual(r[0], 1.0)

    def test_indexed(self):
        self.assertEqual(insn(0x81C4D8), 'fsub dword ptr [ebx + 0xe437e4]')
        r = DB.execute('SELECT target, mode, kind FROM refs WHERE frm=0x81C4D8').fetchone()
        self.assertEqual(r, (0xE437E4, 'indexed', 'r'))

    def test_jump_table_not_code(self):
        # SetupEdit::KeyDown: dword table at 0x40BFA4 (7 cases) then a byte table, both after the symbol's end
        r = DB.execute("SELECT kind, entries FROM jtables WHERE addr=0x40BFA4").fetchone()
        self.assertEqual(r, ('jumptable', 7))
        f = Q.resolve('?KeyDown@SetupEdit@@UAEXHH@Z')
        for k in range(7):
            t = struct.unpack_from('<I', IMG, 0x40BFA4 - BASE + 4 * k)[0]
            self.assertTrue(f <= t < 0x40BFA4)
        self.assertFalse(DB.execute('SELECT 1 FROM refs WHERE frm>=0x40BFA4 AND frm<0x40C085').fetchone())
        self.assertFalse(DB.execute('SELECT 1 FROM calls WHERE frm>=0x40BFA4 AND frm<0x40C085').fetchone())

    def test_symbol_inside_instruction(self):
        # fn_00666C65 starts inside an instruction; the call two bytes later belongs to the real flow
        self.assertTrue(DB.execute("SELECT 1 FROM suspects WHERE name='fn_00666C65'").fetchone())
        self.assertEqual(insn(0x666C6C), 'call 0x73e400')
        self.assertTrue(DB.execute('SELECT 1 FROM calls WHERE frm=0x666C6C AND target=0x73E400').fetchone())

    def test_patch_code_in_padding(self):
        # code in the alignment gap after GameThing::CalculateInfluence, called from elsewhere
        self.assertEqual(insn(0x4017FB), 'call 0x4012a4')
        self.assertTrue(DB.execute('SELECT 1 FROM calls WHERE frm=0x4017FB AND target=0x4012A4').fetchone())
        self.assertTrue(DB.execute("SELECT 1 FROM calls WHERE target=0x4017F9 AND kind='direct'").fetchone())


class Consistency(unittest.TestCase):
    def test_sample_calls_redisassemble(self):
        rows = DB.execute("SELECT frm, target FROM calls WHERE kind='direct'").fetchall()
        random.seed(1)
        for frm, t in random.sample(rows, 3000):
            self.assertEqual(insn(frm), 'call 0x%x' % t)

    def test_sample_refs_redisassemble(self):
        rows = DB.execute("SELECT frm, target, kind, insn FROM refs WHERE mode!='imm'").fetchall()
        random.seed(2)
        for frm, t, kind, text in random.sample(rows, 3000):
            s = insn(frm)
            self.assertEqual(s, text)
            self.assertIn('0x%x' % t, s)
            mn, _, ops = s.partition(' ')
            if mn == 'mov' and ops.split(',')[0].endswith(']'):
                self.assertEqual(kind, 'w', s)
            if mn in ('fld', 'cmp', 'push', 'test'):
                self.assertIn(kind, ('r', 'addr'), s)
            if mn in ('fstp', 'fst', 'fistp'):
                self.assertEqual(kind, 'w', s)

    def test_raw_call_scan(self):
        # every E8 rel32 whose target is a symbol function start, found by a byte scan;
        # the ones the index lacks must be bytes inside other decoded instructions
        fs = {a for (a,) in DB.execute("SELECT addr FROM syms WHERE section='text' AND type='function'")}
        text = IMG[0x1000:0x1000 + 0x4A795D]
        raw = set()
        for m in re.finditer(b'\xe8', text):
            i = m.start()
            if i + 5 <= len(text) and 0x401005 + i + struct.unpack_from('<i', text, i + 1)[0] in fs:
                raw.add(0x401000 + i)
        have = {a for (a,) in DB.execute("SELECT frm FROM calls WHERE kind='direct'")}
        miss = raw - have
        self.assertLess(len(miss), len(raw) * 0.0005, sorted(hex(a) for a in miss)[:20])

    def test_raw_store_scan(self):
        text = IMG[0x1000:0x1000 + 0x4A795D]
        pat = re.compile(rb'(?=(\xc7\x05|\xa3|\x89[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]|\xd9\x1d|\xdd\x1d|\xc6\x05))')
        have = {a for (a,) in DB.execute("SELECT frm FROM refs WHERE kind IN ('w','rw') AND mode='abs'")}
        raw = miss = 0
        for m in pat.finditer(text):
            t = struct.unpack_from('<I', text, m.start() + len(m.group(1)))[0]
            if 0x9C6000 <= t < 0xFC0000:
                raw += 1
                a = 0x401000 + m.start()
                miss += a not in have and a - 1 not in have
        self.assertLess(miss, raw * 0.001)

    def test_decoded_coverage(self):
        tot = DB.execute('SELECT sum(end-start) FROM decoded').fetchone()[0]
        bad = DB.execute("SELECT coalesce(sum(end-start),0) FROM decoded WHERE kind='invalid'").fetchone()[0]
        self.assertGreaterEqual(tot, 0x4A795D)
        self.assertLess(bad, 100)

    def test_size_fixes(self):
        # CallNearInState returns at 0x6F119C; the script CREATE switch after it is a function of its own
        row = DB.execute('SELECT addr, end FROM funcs WHERE addr = 0x6F1010').fetchone()
        self.assertEqual(row, (0x6F1010, 0x6F11A0))
        self.assertEqual(IMG[0x6F119C - BASE], 0xC3)
        callers = {r[0] for r in DB.execute("SELECT frm FROM calls WHERE target = 0x6F11A0")}
        self.assertEqual(callers, {0x6F1BBC, 0x6F20CE, 0x6F2EF0})


class Cli(unittest.TestCase):
    def run_cli(self, *args):
        p = subprocess.run([sys.executable, xref.__file__] + list(args), capture_output=True, text=True, cwd=HERE)
        self.assertEqual(p.returncode, 0, p.stderr)
        return p.stdout

    def test_queries(self):
        self.assertIn('0x5E5903', self.run_cli('writers', '0xD20184'))
        self.assertNotIn('0x5E5903', self.run_cli('readers', '0xD20184'))
        self.assertIn('0x5E54A7', self.run_cli('refs', '0xD20180-0xD20190'))
        self.assertIn('?Draw@GLandscape@@QAEXXZ', self.run_cli('callers', 'fn_005E5830'))
        self.assertIn('__ftol', self.run_cli('callees', '0x5E5830'))
        self.assertIn('?Delete@Base@@UAEXXZ', self.run_cli('vtcalls', '0x8', '--class', 'Base'))
        self.assertIn('0x415CF1', self.run_cli('str', 'VORTEX_LAND1'))
        self.assertIn('0x404C6E', self.run_cli('const', '0.5'))
        out = self.run_cli('func', '?Open@GLandscape@@QAEXPAD@Z')
        self.assertIn('writes:', out)
        self.assertIn('0xD20184', out)

    def test_notes(self):
        # a scratch notes file, never the real one: under RE_TMP if it is set, else the system's temp folder
        tmp = re_config.get('RE_TMP') or tempfile.gettempdir()
        os.makedirs(tmp, exist_ok=True)
        path = os.path.join(tmp, 'test_notes.db')
        if os.path.exists(path):
            os.remove(path)
        env = dict(os.environ, XREF_NOTES=path)

        def cli(*args):
            p = subprocess.run([sys.executable, xref.__file__] + list(args), capture_output=True, text=True,
                               cwd=HERE, env=env)
            self.assertEqual(p.returncode, 0, p.stderr)
            return p.stdout
        self.assertIn('note #1 on 0x5E5903',
                      cli('note', '0x5E5903', 'Test', 'writes the land height', '--text', 'second line'))
        cli('note', '0xD20184', 'Other', 'a global')
        out = cli('notes', 'fn_005E5830')
        self.assertIn('1 note(s)', out)
        self.assertIn('writes the land height', out)
        self.assertIn('2 note(s)', cli('notes'))
        self.assertIn('1 note(s)', cli('notes', '--author', 'Other'))
        self.assertIn('second line', cli('notes', '--grep', 'SECOND', '--full'))
        out = cli('show', 'fn_005E5830', '--max', '2000')
        self.assertIn('notes: 1', out)
        self.assertIn('*0x5E5903', out)
        self.assertIn('disassembly:', out)
        os.remove(path)


if __name__ == '__main__':
    unittest.main(verbosity=2)
