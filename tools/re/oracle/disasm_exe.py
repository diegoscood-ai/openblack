"""disasm_exe.py: disassemble ranges of runblack.exe (capstone), to see a function's callees, globals and constants
before writing a spec.

    python disasm_exe.py 0x6DE510:0x20 GameRand:0x20 [...]      ADDR|symbol:LEN (LEN in hex)
"""

import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import capstone  # noqa: E402
import oracle  # noqa: E402


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    with open(oracle.exe_path(), 'rb') as f:  # read only
        d = f.read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    secs = [struct.unpack_from('<8sIIII', d, pe + 24 + opt + 40 * i) for i in range(n)]

    def off(a):
        rva = a - 0x400000
        for _, vs, va, rs, ro in secs:
            if va <= rva < va + rs:
                return rva - va + ro
        raise ValueError('0x%X is not in the file' % a)

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    sym = oracle.symbols()
    for arg in argv:
        at, _, length = arg.rpartition(':')
        a = int(at, 16) if all(c in '0123456789abcdefABCDEFxX' for c in at) else oracle.where(at, sym)
        size = int(length, 16)
        print('--- 0x%08X %s' % (a, sym.name_at(a)))
        for i in md.disasm(d[off(a):off(a) + size], a):
            note = ''
            if i.mnemonic in ('call', 'jmp') and i.op_str.startswith('0x'):
                note = '  ; ' + sym.name_at(int(i.op_str, 16))
            print('  %08X %-8s %s%s' % (i.address, i.mnemonic, i.op_str, note))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
