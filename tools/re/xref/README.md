# xref: cross-reference index for runblack.exe

`xref.py` disassembles the original game's `.text` once (capstone, 32-bit) and writes an SQLite index, `xref.db`.
Queries then answer "who writes this global", "who calls this", "who uses vtable slot N", "who uses this string or
float" in well under a second, with the function names of
[bw1-decomp](https://github.com/openblack/bw1-decomp)'s symbol file.

Nothing from the game is stored in this repository: the index is built on your machine from your own copy of the
game, and stays there (it is gitignored).

## Install

- Python 3.10 or newer.
- `pip install pefile capstone` (capstone 5).
- Your copy of the original `runblack.exe` (opened read only) and a clone of bw1-decomp.

Settings (environment variables, or `tools/re/local_config.json`; see [../README.md](../README.md)):

| Setting | What | Default |
|---|---|---|
| `RUNBLACK_EXE` | the original `runblack.exe` (`XREF_EXE` also works) | none: `build` and the tests need it |
| `BW1_DECOMP` | a bw1-decomp clone; its `config/BW1W120/symbols.txt` is read | none |
| `BW1_SYMBOLS` | the symbols file itself (`XREF_SYMS` also works), instead of `BW1_DECOMP` | |
| `XREF_DB` | where the index is written and read | `xref.db` next to `xref.py` |
| `XREF_NOTES` | the notes database | `notes.db` next to `xref.py` |
| `RE_TMP` | where the tests write their throw-away notes file | the system's temp folder |

Only `build` and the tests read the exe and the symbols; the queries read only `xref.db`.

## Build and test

    python xref.py build        # about 15 s, about 50 MB; rerun any time (writes xref.db.tmp, then swaps)
    python test_xref.py         # checks the index against instructions decoded straight from the exe

The tests pin facts of the v1.42 unofficial-patch exe; with another version some of them fail.

If you run the tools with `python -I` (isolated mode), pefile and capstone must be installed outside the user
site-packages folder, which `-I` hides; `python -E -P` keeps it while still ignoring the current folder.

How it decodes:

- One linear sweep per symbol function (`type:function`, the size from the symbol comment, else up to the next
  function), then one sweep per gap between them. Gap code is real: the exe has patched-in code in alignment padding
  and library code without symbols. Gap pieces become pseudo functions `sub_XXXXXX`, starting at the gap start, after
  each padding run, and at every direct call target in a gap.
- Switch tables (`jmp [reg*4+T]`, `mov/movzx r8, byte ptr [reg+T]`) are found, sized from the `cmp reg, N` before
  them or from their entries, and skipped as data, also when they lie after the symbol's end (bw1-decomp's sizes
  leave them out). Table `jtables`.
- When a symbol's size cuts the last instruction, that instruction is decoded whole.
- Auto-named `fn_XXXXXXXX` symbols that start inside an instruction of the code before them (their addresses come
  from data that looks like a pointer) stay names only (table `suspects`), and their bytes are decoded as part of the
  code before them.
- Table `decoded(start, end, kind, func)` says how every byte was handled: `func`, `gap`, `padding` (int3/nop),
  `jumptable`, `bytetable`, `invalid`.

Tables: `funcs`, `syms`, `calls` (direct / mem / vt / reg / memind / tail / tailjcc / tailmem, with `vt_off` for
`call [reg+N]`), `refs` (memory operands and immediates pointing into the image: kind `r` / `w` / `rw` / `addr`,
size, `x87` / `sse` / `mmx`, mode `abs` / `indexed` / `imm`, instruction text), view `imms`, `fimms` (32-bit
push/mov immediates read as floats), `consts` (float/double/long double values at referenced addresses), `strings`
(ASCII and UTF-16 in `.rdata`/`.data`, plus every `??_C@` literal), `vtables`, `vtslots`, `dptrs` (aligned dwords
in data pointing into the image), `meta`.

## Queries

Addresses are hex (`0xD20184` or `D20184`); names are an exact symbol or a substring (a list is printed if several
match).

    python xref.py writers 0xD20184                   # writes and read-modify-writes covering that byte
    python xref.py readers 0xD20180-0xD20190          # a half-open range
    python xref.py refs ?g_camera@LH3DTech@@2ULH3DCamera@@A+0x8   # base+offset of a global
    python xref.py refs D20184 --window 0x400         # also indexed accesses whose base is up to 0x400 below
    python xref.py callers ?Draw@GLandscape@@QAEXXZ   # direct calls, tail jumps, import thunks,
                                                      # vtable slots holding it, address taken, data pointers
    python xref.py callers __imp__RegCloseKey@4       # calls through the import table
    python xref.py callees ?Draw@GLandscape@@QAEXXZ
    python xref.py vtcalls 0x8 --class Base           # every call [reg+0x8] site, and slot 0x8 of each vtable
    python xref.py str land1.txt                      # case-insensitive; code refs and data pointers to it
    python xref.py const 0.5                          # loads of 0.5 from memory, and 0x3F000000 immediates
    python xref.py func ?Draw@GLandscape@@QAEXXZ      # range, callers count, callees, globals read/written
    python xref.py show ?Draw@GLandscape@@QAEXXZ      # func + its notes + its disassembly with names

`refs` lists every access kind including `addr` (the address taken by `push imm`, `mov reg, imm`, `mov [m], imm`,
`lea`). `show` prints the disassembly on your screen from your own index; do not paste it into published files.

## Notes: what someone already found

Before analysing a function, look for notes on it; after, leave one, so that nobody reads the same code twice.
Notes live in `notes.db` (not in `xref.db`, so `build` keeps them); SQLite locks it, so several people or agents
can write at once. The author is any short label (a name, a team, an area of the code).

    python xref.py note ?Draw@GLandscape@@QAEXXZ alice "draw order of the land blocks" --text "see docs/bw1-notes/land.md"
    python xref.py note 0xD20184 alice "the camera's height" --file my_notes.md
    python xref.py notes ?Draw@GLandscape@@QAEXXZ     # the notes in that function (or on that data address)
    python xref.py notes --author alice --grep height --full

A note says what the code does and where the evidence is (a wiki page, an oracle run, a test). The wiki
(`docs/bw1-notes`) stays the source of truth; a note is a pointer, not a second copy.

## Limits

- Linear sweep, not flow analysis: data embedded in code other than switch tables is decoded as instructions (a few
  bytes end up `invalid`). A wrong symbol size can still misalign a few instructions at a function start.
- `call reg` and `call [reg+N]` have no static target. `vtcalls` lists the call sites and what each class has in that
  slot; which class the register holds is not known.
- Computed addresses (`[reg*4+base]`, `[reg+base]`) are stored with their constant base as `indexed`; the element
  touched is unknown. `writers` / `readers` print them separately for bases up to `--window` (default 0x100) below
  the queried address.
- Accesses through a pointer held in a register (`mov eax, offset g; mov [eax+8], ecx`) are seen only as the `addr`
  reference of the `mov eax, offset`.
- Access kinds come from our own mnemonic rules, because capstone 5 reports some wrongly (fstp, fistp, cmpxchg,
  SSE/MMX stores, test). String instructions (`rep movs`, `stos`) have no absolute operand and are not indexed.
- `fimms` treats every 32-bit `push`/`mov` immediate with a normal float exponent as a possible float; an integer with
  the same bits shows too.
- Vtable slots stop at the first dword that is not a code pointer (symbol sizes often run into the RTTI locator or
  the constants after the table).
- The project's target is the v1.42 unofficial-patch exe, which differs from bw1-decomp's 1.20 in some patched
  bytes: the index describes your exe, the names come from the symbols.
