# Research tools

## Summary

How the original game is read and how a port is checked against it. The original's behaviour is found in four
steps: bw1-decomp's C first, then the cross-reference index of `runblack.exe` (xref) for every "only" or "never",
then the oracle, which runs the original's own functions for exact numbers, and the wiki page of the subject, where
the evidence is written down. A change to openblack is checked before it is handed over with precheck, and when a
run's per-turn state hash moves, hashdiff names the first entity that changed. openblack's own command-line tools
(packtool, l3dtool, lndtool and the rest) read the game's formats.

The tools live in `tools\` beside the `openblack` clone (formerly `dev\tools`, as older pages still name it, and
before that `herramientas`), and in the repository under `tools/re/` (xref, oracle, precheck, hashdiff). The copy
beside the clone has its machine's paths built in, and differs from the repository's in a few options, said where
they matter. None of them contains bytes, disassembly or data of the game: they read your own copy at run time, and
what they produce (the index, the vectors, disassembly on screen) stays on your machine and is never committed or
pasted into published files.

## The original

### Which exe the tools read

The target is the v1.42 unofficial-patch `runblack.exe` (sha1 `1e73ba9a96c3d6316e9f8f41a8947a25790d2698`), the
frozen read-only copy in `_gamedata\frozen` beside the `openblack` clone. bw1-decomp is built from Lionhead's 1.20: its
C and its symbol names are the guide, and where the patch changed code the exe's bytes win
([Which exe is the original](reading-the-wiki.md#which-exe-is-the-original)). The xref tests pin facts of the v1.42
exe; with another version some of them fail.

### The typical workflow

1. **Read the decompilation first.** Find the function in bw1-decomp's C (`src/`, a local clone kept up to date)
   and read what it does.
2. **Check every claim of "only" or "never" with xref.** Before analysing a function, `python xref.py show
   <function>` gives its summary, the notes others left on it, and its disassembly with names; `writers`,
   `readers`, `callers`, `callees`, `vtcalls`, `str` and `const` answer who touches what. Afterwards leave a note.
3. **Get the exact numbers from the oracle.** `python oracle.py quick <function> <args>` runs the original
   function; `--stub` replaces or records a callee; `gen_vectors.py` makes thousands of cases from a spec. Pin
   openblack's tests to those values (`vectors_to_cpp.py` prints a few as a C++ table).
4. **Write the port and its tests**, then run `sh precheck/precheck.sh <clone> <base>` before handing the commits
   over.
5. **When a run's state hash moves**, run both builds with `OPENBLACK_STATE_HASH_ENTITIES` and let
   `hashdiff/hashdiff.py` name the first entity and turn that differ, and how.
6. **Write down what you found** in the wiki page of the subject, with the evidence.

### Settings

The repository tools take their paths from environment variables, or from `tools/re/local_config.json`, a flat
`{"NAME": "value"}` file next to their README that is gitignored (copy `local_config.example.json`). An environment
variable wins over the file; a tool that needs a missing setting stops with a message naming it.
`python re_config.py get NAME` prints what a setting resolves to.

| Setting | Used by | What |
|---|---|---|
| `RUNBLACK_EXE` | xref, oracle | your copy of the original `runblack.exe`, opened read only (`XREF_EXE` also works for xref) |
| `BW1_DECOMP` | xref, oracle | a bw1-decomp clone (its `config/BW1W120/symbols.txt` names the exe's functions) |
| `BW1_SYMBOLS` | xref, oracle | the symbols file itself, instead of `BW1_DECOMP` (`XREF_SYMS` also works) |
| `RE_TMP` | all | a folder for temporary files (default: the system's temp folder) |

Install: Python 3.10 or newer and `pip install pefile capstone` (capstone 5) for xref and `oracle/disasm_exe.py`;
the others need only the standard library. Windows with Visual Studio 2022 (2019 also works for the oracle), or the
Build Tools with the C++ workload, for the oracle's host and precheck's standalone compile; precheck's other checks
need a POSIX shell (Git Bash) and clang-format 17. With `python -I` (isolated mode) packages installed with
`pip install --user` are hidden; install them for the interpreter, or use `python -E -P`.

### Disassembly of runblack.exe

The first scripts, kept outside the repository (`bwdis.py`, `callers.py`, `refs.py`, `scan.py`, `vtq.py`,
`anim_vt.py`, `vt.py` and `potinfo.py`; `bwdis.py` needs `capstone` and `pefile`). xref and the oracle's
`disasm_exe.py` have taken over most of their work.

- `python bwdis.py ADDR:SIZE [ADDR:SIZE...]` disassembles `runblack.exe` with capstone. It annotates symbols, floats
  from the data section (`; =0.67`) and strings.
  - Virtual calls `call [reg + off]` are annotated with the vtable of class `VTC` (environment variable, by default
    `Tree`). **The annotated name is only valid if `VTC` is the object's real class**; otherwise, ignore it.
- `python callers.py ADDR` lists who calls a function (searches for `call rel32`).
- `python refs.py ADDR...` lists instructions that reference an address (data or functions).
- `vt.py Clase [regex]` prints a class's vtable; `potinfo.py` reads the pot table from `info.dat`.

Symbols and tricks:

- The symbols come from `bw1-decomp\config\BW1W120\symbols.txt`. Some names are wrong, for example: 0x5B3C70 is
  `HandStateHolding::Update` and 0x5B5E70 is `ObtainRequiredHandPosition`.
- To read tables that are filled in at startup (in `.data` they are zero), look for the `crt_xc_fn_*` initialiser
  that writes to them (for example the interface action state table, 0x5D7960).

#### Reference values for the bit-exact tests

- **Float routines**: an independent float32 model of the x87 at 24-bit precision (PC24), written operand by operand
  from the disassembly. Each fadd / fsub / fsubr / fmul / fdiv / fsqrt is rounded to 24 bits. fpatan (like fsin /
  fcos) is not affected by the precision control, so it runs at 64 bits and is rounded only by the caller's
  `fstp dword` ([Floating point](../engine/floating-point.md#the-x87-arithmetic)). The test then compares the bits.
  Modelled this way: the LH3D angles (0x7FA990, 0x7FAA50, 0x7FAAF0), `PhysOb::SetUpPos` 0x7FC760 and
  AdjustToGroundLevel 0x7FCB80, HandAngularMomentum 0x5D4EB3.., DrawOrigin 0x6473B4.. and CapReleaseVelocity
  0x5D4F5F..; with numpy float32, one rounding per operation: 0x7FAFF0, 0x7FAE60, `LHMatrix::SetInverse` 0x7FB290
  and UpdateWorldToCamera 0x819690; the walking rows and InitStepsXZ 0x60BFA0 (tables 0xC31614 / 0xC31E14 /
  0xC2307C). game_random's values come from a float32 emulation of the same kind. The oracle now gives such values
  directly.
- **Integer pixel routines**: the test copies the original's instructions one by one, keeping register-sized
  variables, and compares the output on every input: 4444 texel 0x8374F0..0x837533 / 0x83767E..0x837687, 555
  0x83776A..0x83778D, raw 565 0x8376E9..0x83770C, diffuse 0x80BFA3..0x80C00B, the 0x80808081 divides 0x7FA6DF /
  0x5E272C, FragMesh face light 0x7F82A8..0x7F8363.
- **Exe code run under Unicorn**: the scripts are kept with each subject's documents (for example the QMixer
  emulation, see [The audio engine](../audio/engine.md), and the one that runs the exe's `_qsort` 0x7C7E64, see
  [Villagers: deciding what to do](../beings/villager-decisions.md)).

### xref: who reads, writes or calls what

`xref.py` disassembles the exe's `.text` once (capstone, 32-bit) and writes an SQLite index, `xref.db`. Queries then
answer "who writes this global", "who calls this", "who uses vtable slot N", "who uses this string or float" in well
under a second, with bw1-decomp's function names. Only `build` and the tests read the exe and the symbols; the
queries read only `xref.db`.

| Setting | What | Default |
|---|---|---|
| `XREF_DB` | where the index is written and read | `xref.db` next to `xref.py` |
| `XREF_NOTES` | the notes database | `notes.db` next to `xref.py` |

    python xref.py build        # about 10-20 s, about 50 MB; rerun any time (writes xref.db.tmp, then swaps)
    python test_xref.py         # checks the index against instructions decoded straight from the exe

How it decodes:

- One linear sweep per symbol function (`type:function`, the size from the symbol comment, else up to the next
  function), then one sweep per gap between them. Gap code is real: the exe has patched-in code in alignment
  padding (for example 0x4017F9) and library code without symbols. Gap pieces become pseudo functions `sub_XXXXXX`,
  starting at the gap start, after each padding run, and at every direct call target in a gap.
- Switch tables (`jmp [reg*4+T]`, `mov/movzx r8, byte ptr [reg+T]`) are found, sized from the `cmp reg, N` before
  them or from their entries, and skipped as data, also when they lie after the symbol's end (bw1-decomp's sizes
  leave them out). Table `jtables`.
- When a symbol's size cuts the last instruction, that instruction is decoded whole.
- Auto-named `fn_XXXXXXXX` symbols that start inside an instruction of the code before them (their addresses come
  from data that looks like a pointer) stay names only (table `suspects`), and their bytes are decoded as part of
  the code before them.
- Table `decoded(start, end, kind, func)` says how every byte was handled: `func`, `gap`, `padding` (int3/nop),
  `jumptable`, `bytetable`, `invalid`.

Tables: `funcs`, `syms`, `calls` (direct / mem / vt / reg / memind / tail / tailjcc / tailmem, with `vt_off` for
`call [reg+N]`), `refs` (memory operands and immediates pointing into the image: kind `r` / `w` / `rw` / `addr`,
size, `x87` / `sse` / `mmx`, mode `abs` / `indexed` / `imm`, instruction text), view `imms`, `fimms` (32-bit
push/mov immediates read as floats), `consts` (float/double/long double values at referenced addresses), `strings`
(ASCII and UTF-16 in `.rdata`/`.data`, plus every `??_C@` literal), `vtables`, `vtslots`, `dptrs` (aligned dwords in
data pointing into the image), `meta`.

Queries (addresses in hex, `0xD20184` or `D20184`; names an exact symbol or a substring, a list printed if several
match):

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
    python xref.py func 0x5E5830                      # range, callers count, callees, globals read/written
    python xref.py show ?Draw@GLandscape@@QAEXXZ      # func + its notes + its disassembly with names

`refs` lists every access kind including `addr` (the address taken by `push imm`, `mov reg, imm`, `mov [m], imm`,
`lea`). `show` prints the disassembly on your screen from your own index; do not paste it into published files.

#### Notes: what someone already found

Before analysing a function, look for notes on it; after, leave one, so that nobody reads the same code twice.
Notes live in `notes.db` (not in `xref.db`, so `build` keeps them); SQLite locks it, so several people or agents can
write at once. The author is a short label (a name, a team, an area of the code).

    python xref.py note 0x5E5903 <author> "writes the land height of the cell" --text "rounded once, see world/terrain"
    python xref.py note 0xD20184 <author> "the camera's height" --file my_notes.md
    python xref.py notes ?Draw@GLandscape@@QAEXXZ     # the notes in that function (or on that data address)
    python xref.py notes --author <author> --grep height --full

The copy beside the clone calls the author a lane and knows only `--lane`, which the repository's copy takes too. A
note says what the code does and where the evidence is (a wiki page, an oracle run, a test). The wiki stays the
source of truth; a note is a pointer, not a second copy.

#### Limits of xref

- Linear sweep, not flow analysis: data embedded in code other than switch tables is decoded as instructions (a few
  bytes end up `invalid`). A wrong symbol size can still misalign a few instructions at a function start.
- `call reg` and `call [reg+N]` have no static target. `vtcalls` lists the call sites and what each class has in
  that slot; which class the register holds is not known.
- Computed addresses (`[reg*4+base]`, `[reg+base]`) are stored with their constant base as `indexed`; the element
  touched is unknown. `writers` / `readers` print them separately for bases up to `--window` (default 0x100) below
  the queried address.
- Accesses through a pointer held in a register (`mov eax, offset g; mov [eax+8], ecx`) are seen only as the `addr`
  reference of the `mov eax, offset`.
- Access kinds come from the tool's own mnemonic rules, because capstone 5 reports some wrongly (fstp, fistp,
  cmpxchg, SSE/MMX stores, test). String instructions (`rep movs`, `stos`) have no absolute operand and are not
  indexed.
- `fimms` treats every 32-bit `push`/`mov` immediate with a normal float exponent as a possible float; an integer
  with the same bits shows too.
- Vtable slots stop at the first dword that is not a code pointer (symbol sizes often run into the RTTI locator or
  the constants after the table).
- The index describes your exe (v1.42); the names come from the 1.20 symbols.

### The oracle: the original's own numbers

The oracle calls functions of the original `runblack.exe` natively (no emulation), at the game's FPU settings, and
records their exact results. It answers "what exact bits does the original give for this input" and "which
arguments does this function pass to its callee", and turns those answers into golden vectors that openblack's
ports are checked against bit for bit. Use it when the answer is a number: a formula, an angle, a rounding, a random
draw, a table the game computes, a constant. Find the function with `python oracle.py sym <regex>`, read its C,
then `python disasm_exe.py <addr|symbol>:<len hex>` to see its callees and globals. Run everything from the
oracle's folder (`tools/re/oracle/` in the repository, `tools\oracle\` beside the clone); a start-up costs about 0.5 s.

| File | What |
|---|---|
| `oracle_host.cpp`, `build.bat` | the 32-bit host: maps the exe at its image base (0x400000) and answers JSON-line requests |
| `oracle.py` | the Python client: starts the host, `call()`, symbol lookup (bw1-decomp's `symbols.txt`), a small CLI |
| `gen_vectors.py`, `specs/*.json` | a spec -> golden vectors in `vectors/<function>.json`; `specs/variants/` hold variants |
| `cmp/compare.cpp`, `cmp/build_cmp.bat` | a 64-bit check of openblack's code against the vectors; `cmp/report.txt` is the last run |
| `vectors_to_cpp.py` | a few cases of a vector file as a C++ constant table for a gtest |
| `disasm_exe.py` | `disasm_exe.py ADDR:LEN` disassembles a range of your exe on screen (capstone) |

| Setting | What | Default |
|---|---|---|
| `ORACLE_HOST` | the built host | `oracle_host.exe` next to `oracle.py` |
| `ORACLE_BUILD_OUT` | where `build.bat` puts the host and its objects | the oracle's folder |
| `VCVARS32` | `vcvars32.bat`, if vswhere cannot find it | found with vswhere |

`oracle.py` passes the exe that `RUNBLACK_EXE` names (the copy beside the clone passes the frozen read-only
`_gamedata\frozen\runblack.exe`); the host only opens it `"rb"`.

    build.bat                                     # oracle_host.exe (vcvars32, cl; /BASE:0x60000000 /FIXED /DYNAMICBASE:NO)
    python oracle.py sym GetYXZ                   # find a symbol
    python oracle.py quick NormaliseAngle f32:7   # one call: the result's bits and value
    python oracle.py call ?SetYXZMatrixOnly@LHMatrix@@QAEXMMM@Z --conv thiscall --alloc m=48 --this @m --read @m:36 f32:0.5 f32:0.25 f32:-1
    python gen_vectors.py --all                   # every specs/*.json -> vectors/ (variants: specs/variants/*.json)
    cmp\build_cmp.bat                             # cmp\compare.exe (vcvars64; compiles openblack's sources read only)
    cmp\compare.exe [--json out.json] [files or dirs]   # default: ..\vectors

`build_cmp.bat` reads openblack's sources (or `OPENBLACK_SRC`) and the vcpkg headers of a normal build of it
(`VCPKG_INC`).

#### How the host works

The host never runs the exe's entry point, CRT start-up or static initialisers. It copies the exe's headers and
sections to their addresses (the exe has no relocations), then resolves every import to a trap that reports its
name. When the exe's range is already taken in a fresh process (NLS mappings, heaps, injected DLLs), so that
`VirtualAlloc(0x400000)` fails, the host starts a suspended copy of itself, reserves the range in it with
`VirtualAllocEx` before its loader runs, and lets the copy load the exe (a kill-on-close job ties the two). Calls
run on a worker thread with a 16 MB stack.

**Machine baseline.** `oracle.py` then runs what the game does once at start-up and snapshots it: `fn_007AC9E0`
(`LH3DP3::CheckCPU`, native `cpuid`). Its dispatcher picks a body by CPU family: a family 6 CPU (every Intel Core)
gets the Pentium III body, so HasSSE2 [0xE83A20] = 0 and `__ftol` 0x7A1400 takes its x87 branch; a family 0xF CPU
(Pentium 4, some AMD) gets HasSSE2 = 1, the SSE2 path. The two differ only for NaN and out-of-range values; `.sse2`
spec variants force the other path. The baseline also sets the game's MXCSR (flush to zero, 0x9F80).
`_has_osfxsr_set` needs the CRT, so its two call sites return 1 while the baseline runs. `Oracle(baseline=False)`
skips all this. The vectors record the result under `"machine"`.

#### Protocol (one JSON object per line)

Request (all keys optional except `addr` for a call):

```
{"id": any, "addr": "0x7FAC10", "conv": "cdecl|stdcall|thiscall|fastcall", "this": "0x...|@buf",
 "args": [{"type": "i32|u32|i16|u16|i8|u8|bool|ptr|f32|f64|i64|u64", "v": 1.5 | "0x..." | "@buf+0x10" | "nan"} or {"type": "f32", "bits": "0x3F800000"}],
 "ret": "void|i32|u32|i16|u16|i8|u8|bool|ptr|i64|u64|f32|f64|st0",
 "alloc": [{"name": "buf", "size": 48, "hex": "...", "fill": 0, "align": 16}],
 "write": [{"addr": "0x...|@buf", "hex": "..."}], "read": [{"addr": "0x...|@buf", "len": 36}],
 "regs": {"eax|ebx|ecx|edx|esi|edi": "0x..."}, "fpu_push": [{"type": "f32|f64|f80", "bits": "0x..."}],
 "fpu_cw": "0x007F", "timeout_ms": 5000, "reset": true | "pristine", "snapshot": true}
{"batch": [request, ...]}        {"cmd": "ping|state|imports|quit"}
```

Order inside a request: reset, alloc, write, call, read, snapshot. Buffers live in a fixed arena (0x20000000), are
freed by the next request and have a guard area checked after the call. Stack arguments are pushed right to left.
For thiscall `this` goes in ECX. For fastcall `this` (if given) and then the first integer arguments go in ECX, EDX;
floats always go on the stack. `regs` overrides registers for hand-written conventions, and `fpu_push` loads x87
inputs (the last one is ST0, for example for `__ftol`). ESP is saved before the call and restored after, so a wrong
convention cannot break the host; the response reports what the callee actually popped (`callee_pop`) and warns on a
mismatch. Before the call: `fninit`, the game's control word (0x007F: 24-bit precision, round to nearest) and the
game's MXCSR; after it the host's are restored.

Response: `{"ok": true, "ret": <value>, "ret_hex": "...", "read": ["hex", ...], "eax", "edx", "callee_pop",
"fpu_sw", "fpu_depth", "st0_hex", "buffers", "warn": [...]}`. Float returns come from ST0. `ret_hex` holds the
returned type's bits (f32: what `fstp dword` stores). For `st0` it holds the 80-bit value, plus `ret_f32_hex` and
`ret_f64_hex`. Failures: `{"ok": false, "error": "...", "trap": {"import", "iat", "from"} | "exception": {"code",
"addr", "access", "target"} | "timeout": {"eip"}}`. Writes to the exe's memory persist until `reset` (back to the
last `snapshot`, that is the baseline; `"pristine"` = the file's image).

#### Recipes

1. **One call.** The convention and the return type come from the decorated name (`--conv`, `--ret` override them).
   `*f32:1,0,2` passes a pointer to a new buffer (an `LHPoint`; a `MapCoords` is `*i32:x,z,0`), `*out:N` a pointer
   to N zero bytes; both are printed back after the call. `f32:0x3F800000` passes raw bits. Float returns print the
   f32 a caller's `fstp dword` stores, the f64 and the 80-bit value.
   ```
   python oracle.py quick GetHeadingFromPoints *f32:1,0,2 *f32:0,0,0
   python oracle.py quick NormaliseAngle f32:7
   python oracle.py quick _LHRand__FlRU --ret u32 u32:100 *u32:0x88F89F     (no type information: say --ret)
   ```
2. **A global or a constant** (after the baseline; types i8..u64, f32, f64, `:count`), for example
   `python oracle.py peek 0x8C36A0 f32` or `python oracle.py peek 0x8D5FB8 f64:2`.
3. **Globals in, globals out.** `--set` writes before the call, `--get` reads after it:
   `python oracle.py quick NormaliseAngle f32:7 --set 0xCADB70=i32:1,2 --get 0xCADB70:i32:2`. In Python:
   `o.call(fn, conv, args, ret, set_globals=[(where, 'u32', v)], get_globals=[(where, 'f32', 3)])` and
   `r['globals']`; `o.poke(where, t, v)` / `o.peek(where, t, n)` persist until `o.reset()`. `where`: `0x..`, a
   symbol, or either `+0xOFF`.
4. **Stub a callee** that would touch the renderer, files, the OS or heap objects: calls to it return a chosen value
   and pop `/N` argument bytes. A function is patched at its entry (a jmp); an import (`DLL!Name`) through its import
   slot. Each stub counts its calls:
   `python oracle.py quick IsTalkingWithDelay --this *out:14320 --stub 0x5BB760=i32:0 --stub KERNEL32.dll!GetTickCount=u32:150`.
   Python: `s = o.stub(target, 'f32', 1.25, pop=0)`, `o.stub_calls(s)`. Stubs live until `o.reset()`;
   `o.snapshot()` keeps them.
5. **Capture a callee's arguments.** Add `@` and one item per stack argument to a stub: a 4-byte type, or `pK:type`
   for a pointer whose K dwords are copied as the call is made (an `LHPoint&` is `p3:f32`). `@N` records N plain
   dwords; ECX is recorded too; the first 16 calls are kept:
   `--stub fn_004ED180=u32:0x11223344@u32,p3:f32 --stub AddLiquidParticle=void@p3:f32,p3:f32,u32,f32,i32`, with
   `--local-seed 0x12345`. Python: `o.stub(target, 'void', capture=['p3:f32', 'u32'], max_calls=16)`, then
   `o.stub_args(s)`.
6. **The game's random numbers.** The game object lives on the heap, so `fake_game` points its pointer at a fake
   holding the synchronised seed (0x8A8FF0: GameRand, GameFloatRand) and the local one (0x8A8FF4: LocalRand,
   LocalFloatRand); `lh` is the generator under all four
   ([Random numbers](../engine/random.md#random-numbers)):
   `python oracle.py rng game --seed 0x88F89F --n 10 --arg 100`, `python oracle.py rng localfloat --seed 0x88F89F
   --n 5 --arg 1.0`, `python oracle.py quick GameRand u32:100 u32:0 u32:0 --seed 0x88F89F` (prints the seeds after).
   Python: `o.rng('gamefloat', 2.5, n=100, seed=0x88F89F)` gives `[(bits, value, seed_after)]`.

#### Golden vectors

Copy the closest spec in `specs/` (with one line of `notes`), try it with `python gen_vectors.py specs/MySpec.json
--out <a scratch folder>`, then write it into `vectors/` without `--out`. Give both `symbol` and `addr`: bw1-decomp
renames symbols (a fixed return type changes the decorated name) while the address never moves, so a vanished
symbol only warns. Examples: `GetHeadingFromPoints` (the camera heading between two points, an x87 result),
`MakeShapeCircle` and its variants (the creature dance shapes' points), `GameRand` and the variant `GameFloatRand`
(2000-draw chains from a fixed seed).

```
{"function": "GetYXZ",                       // selects the openblack port in compare.cpp; also the vector file name
 "symbol": "?GetYXZ@LHMatrix@@QBEXPAM00@Z", "addr": "0x...",   // either or both (checked against each other)
 "conv": "thiscall", "ret": "void", "this": "m",
 "buffers": {"m": [{"name": "m", "type": "f32", "count": 12, "gen": "matrix"}],
             "y": [{"name": "y", "type": "f32", "out": true}]},
 "args": [{"type": "ptr", "buffer": "y"}, {"name": "r", "type": "f32", "gen": {"uniform": [-7, 7]}}],
 "generators": {"matrix": {"mix": [[70, {"rotation": {"angle": {"uniform": [-3.15, 3.15]}}}], [30, {"uniform": [-1, 1]}]]}},
 "edges": {"r": [0, "-0", "nan", "0x00000001"]},
 "cases": [{"m": [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0]}],
 "random_cases": 4000, "seed": 102,
 "setup": [{"addr": "?SetupUtils@GUtils@@SAXXZ"}, {"write": [{"addr": "0x00E83A20", "hex": "01000000"}]}],
 "reset_each": false, "notes": "...",
 "globals": [{"name": "seed", "addr": "0x...", "type": "u32", "gen": {"bits": true}, "out": true}],
 "stubs": [{"target": "0x... | symbol | DLL!Import", "type": "i32|f32|void|...", "value": 0, "pop": 0}],
 "fake_game": true,
 "chain": {"seed": "seed"},
 "extends": "../Base.json", "vectors": "Base.variant"}
```

Stubs and the fake game object live in the unused zero tail of `.text` (0x8A8A00..0x8A9000, `oracle.POOL_*`), so
`reset` covers them and a `snapshot` keeps them. Generators: `uniform [lo, hi]`, `int [lo, hi]`, `loguniform [lo,
hi]` (+ `signed`), `choice [...]`, `bits`, `near` (another input + `delta`), `mix [[weight, gen], ...]`, `rotation`
(9 / 12 cells of YXZ angles, optional `scale`, `translation`). Values may be numbers, `"0x..."` raw bits, `"nan"`,
`"inf"`, `"-inf"`, `"-0"`. Field types: f32 f64 i32 u32 i16 u16 i8 u8 ptr; fields pack in order at natural
alignment unless they give `offset`.

Vector file: `format`, `function`, `symbol`, `addr`, `conv`, `ret`, `exe` (path, size, sha1), `fpu_cw`, `machine`,
`setup`, `seed`, `inputs` / `outputs` (name, type, count), `constants`, and `cases`: one per line,
`{"in": {name: "0x..." | [...]}, "out": {...}}` or `{"in": ..., "error": "..."}` when the exe faulted. Every value
is its exact bits in hex (f80 = 20 digits, sign and exponent first).

One vector has no openblack port yet: `GetHeadingFromPoints`, `GCamera::GetHeadingFromPoints` 0x442860 (the
click's heading), for whoever ports the camera's click-to-fly heading. `compare.exe` lists every other vector
against its port.

#### Limits of the oracle

- Every import is a trap: a function that calls the OS, the game's own DLLs or the CRT's heap / TLS / `signal`
  fails with the import's name and caller. Stub it, or stub the callee that reaches it. Data imports read the stub.
- No static initialisers run: start-up tables are zero until a `setup` builds them (for example
  `GUtils::SetupUtils` for the inverse square root table, used by the `GetDistance` spec through `SetupUtils`). Heap objects (the game,
  creatures, villagers) do not exist: pass `this` buffers of the class's size, fake pointers, or stub the accessors.
  Virtual calls need a real vtable.
- **x87 against SSE2**: see the baseline above. Calls run at the game's control word 0x007F: every fmul / fadd
  rounds to float while fsin / fcos / fpatan results stay extended until the next operation; openblack's x64 SSE
  code must reproduce that step by step.
- One thread; a call stuck in a loop is aborted (`timeout_ms`, 5 s). Memory a faulting call wrote stays until reset.
- `compare.exe` compiles the few openblack files it tests. A new port needs a lambda in `Ports()` and, if it lives in
  another file, that file in `build_cmp.bat`. Vectors with no registered port are listed as such.
- Safety: the exe is opened read only; patches and stubs touch only the in-memory image, which `reset` restores. The
  oracle never runs the game or openblack. Do not publish vector files or disassembly; publish the specs, and in
  tests only the few numbers a check needs.

### Sources

- The READMEs of `tools\` beside the clone (xref, oracle README and GUIDE, precheck, hashdiff) and of `tools/re/` in
  the repository; the old tooling page.

## openblack

### openblack tools

Built in `cmake-build-presets\ninja-multi-vcpkg\bin\Release`.

> **Code rules.** Tools and tests read the game's files through the same loaders and resource caches as the game;
> unit tests live in `test/`, use fakes and never need the original data (a test that does skips itself when the
> data is absent); comments describe behaviour in plain English, with no decompiled names or addresses (those
> belong in this wiki). See [openblack internals](internals.md).

- `packtool -M pack.g3d` lists meshes; `packtool -m N -e out.l3d pack.g3d` extracts mesh N; `packtool -T pack.g3d`
  lists textures (**ids in hexadecimal**); `packtool -t IDX -e out.dds` extracts the texture by index (the id is a
  different field).
- `l3dtool read -H|-m|-P|-V|-I|-s file.l3d`: header, submeshes, primitives (material, skinID), vertices (positions
  with **one decimal place**), indices, embedded skins.
- `lndtool write ... --points "x y z"`: generates the test terrain ([Land and script files](../formats/land-and-script-files.md)).
- `anmtool`, `morphtool`, `glwtool`, `lhvmtool` and `camtool` read and convert the animations, morphs, the GLW, the
  compiled scripts and the camera files; `<tool>.exe --help` lists the options.
- `creaturemindtool [--episodes] [--roundtrip] FILE...` dumps creature mind files; `--roundtrip` checks that each
  writes back byte for byte.
- **Shaders with #include**: each shader depends on the files it includes (`openblack_shader_include_depends` in
  `src/CMakeLists.txt`), so a change to `vs_object.sc` or another included variant rebuilds every variant that
  includes it; no `touch` is needed.
- Replay and profile switches (openblack's own, not the original's;
  [Proving two runs are the same game](../engine/loop.md#proving-two-runs-are-the-same-game)):
  `OPENBLACK_STATE_HASH=<file>` writes a hash of the game state at the end of each turn;
  `OPENBLACK_FIXED_FRAME_MS=<ms>` makes every frame that many ms (game timer, engine timer, audio ticks, the frame's
  delta; not Lightning's 10 s wall-clock timeout, `Particles/Rules/Lightning.cpp`); `OPENBLACK_PROFILE=<s>` ends
  each summary with the memory (RAM working set, peak and private bytes; bgfx textures, render targets, GPU,
  transient buffers).

#### Using oracle vectors in openblack tests

A test reading a whole vector file is an integration test: it skips itself when the file is absent (the CI has no
exe and no vectors; the oracle's README beside the clone puts a local copy in `test/data/oracle/<function>.json`). It
reads the file with a JSON library; none is a dependency of the tree today (nlohmann-json left `vcpkg.json` with
the old mod loader), so the first such test adds one. No test of the tree does so yet, and the tree defines
no `OPENBLACK_TEST_DATA`: the snippet shows the shape such a test takes.

```cpp
TEST(Oracle, GetAngleFromXZ)
{
	std::ifstream f(std::filesystem::path(OPENBLACK_TEST_DATA) / "oracle" / "GetAngleFromXZ.json");
	if (!f)
	{
		GTEST_SKIP() << "no oracle vectors";
	}
	const auto doc = nlohmann::json::parse(f);
	const auto bits = [](const nlohmann::json& v) { return std::stoul(v.get<std::string>(), nullptr, 16); };
	for (const auto& c : doc["cases"])
	{
		if (c.contains("error"))
		{
			continue; // the original faults for this input
		}
		const auto& in = c["in"];
		const map_coords::MapCoords from {static_cast<int32_t>(bits(in["from.x"])),
		                                  static_cast<int32_t>(bits(in["from.z"])), 0.0f};
		const map_coords::MapCoords to {static_cast<int32_t>(bits(in["to.x"])), static_cast<int32_t>(bits(in["to.z"])),
		                                0.0f};
		EXPECT_EQ(gutils::GetAngleFromXZ(from, to), bits(c["out"]["ret"])) << c.dump();
	}
}
```

For the unit tests that run in CI, embed a few cases instead:
`python vectors_to_cpp.py vectors/GameRand.json --n 12 > <scratch>/GameRandCases.inc`, and paste the table into a
test under `test/`:

```cpp
// (the table vectors_to_cpp.py printed: struct GameRandCase {max, seed, ret, seed_after}, k_GameRandCases)
TEST(GameRandomOracle, SeededRandomMatchesTheOriginal)
{
	for (const auto& c : k_GameRandCases)
	{
		uint32_t seed = c.seed;
		const uint32_t r = c.max == 0 ? 0 : openblack::game_random::SeededRandom(c.max, seed);
		EXPECT_EQ(r, c.ret);
		EXPECT_EQ(seed, c.seed_after);
	}
}
```

Floats are compared as bits: `std::bit_cast<float>(c.scale)` in, `std::bit_cast<uint32_t>(result)` against
`c.ret_f32`. Pick the hand-picked edge cases (they come first) plus a spread. The tree's own tables are in
`test/test_oracle_vectors.cpp` (GameRand, GameFloatRand, GetAngleFromXZ, Mul, SetInverse and MakeShapeCircle).
`cmp/compare.cpp` does the same for every vector file and classifies mismatches (1 ulp, more than 1 ulp, signed zero,
NaN bits); it runs openblack's code under SEH, so a case where both the exe and openblack fault counts as agreement.

#### Test names that carried addresses

Each test was renamed for what it checks; the old name kept the address of the routine it was checked against. Three
were renamed again since: the colour test with its suite and for the function it tests, and the two matrix tests
with their suite.

| New name | Old name (address) |
|---|---|
| `Argb4444.PackMatchesTheTexelEmulation` | `PackMatchesFn00837400` |
| `FallingSpell.WorldToCameraTransposesAndNormalisesTheRows` | `WorldToCameraLikeFn00819F50` |
| `GuidanceTable.TypeTableRows` | `TheRowsOf0x980190` (the guidance type table) |
| `ArgbColour.MultiplyArgbShift8MatchesTheDiffuseEmulation` | `MulShr8_4MatchesFn0080BF10` |
| `AffineMatrix.MulMatchesTheReferenceBits` | `MulIsFn007FAFF0` |
| `AffineMatrix.MulPreMatchesTheReferenceBits` | `MulPreIsFn007FAE60` |
| `ParticleDrawPath.QueuedEffectsOrder` | `QueuedOrderIsFn006798B0` |
| `SpookySoundex.LetterCodes` | `TheJumpTableOf0x72E54C` (the letter jump table) |
| `PlacementCellsTest.CoastIsRefusedOnlyByTheScaffoldsTest` | `CoastIsRefusedOnlyBy0x603DC0` (the scaffolds' cell test) |

`test_rgb16.cpp`: the 555 / raw 565 emulations named their variables after the original's registers (edx -> x,
eax -> y, edi -> packed); the steps are the same.

### precheck: the check before handing commits over

`precheck.sh` checks a clone's commits `base..HEAD` the way a reviewer and the CI would, without a full build. The
clone is never modified (clang-format runs with `--dry-run`); logs go to a temp folder
(`<PRECHECK_TMP>/precheck/<date>_<pid>/`).

    sh precheck.sh <clone> <base> [--no-compile]

| Check | What it does | Fails when |
|---|---|---|
| a) clang-format | `clang-format --dry-run --Werror --style=file` on the lines added since base | any file would change |
| b) cmake tests | every test source `test/CMakeLists.txt` names exists, and no test name is listed twice | a source is missing or a name is listed twice |
| c) braces/parens | per changed `.cpp/.h/.hpp/.inl`, `{ }` and `( )` counted outside comments, strings, char and raw-string literals, compared with the base version | the imbalance differs from the base's (a new file must be balanced) |
| d) words/addresses | added lines: the `PRECHECK_DENY_WORDS` words; in comments of `src/` code files, addresses of the original exe (`0x4xxxxx`..`0x9xxxxx`, optionally `0x00`-prefixed) and `fn_XXXXXXXX` names | any hit; the generated `docs/wiki-html` and hex constants such as 0xC90FDA.. and 0xFF000000 are excluded |
| e) standalone compile | `ctu.py` on every changed `.cpp` under `src/` (new files with `src/Game.cpp`'s flags) and `test/` (with `test/test_influence.cpp`'s), one TU at a time | `FAILS` > 0 or any `NO BUILD RULE` |
| f) test groups | maps changed test files (and headers, through the sources that include them) to their group in `test/CMakeLists.txt`; with `PRECHECK_TEST_TREE`, runs each group's executable with `--gtest_brief=1` when that tree holds the clone's HEAD and the exe is newer than every changed file | informative only |

If `src/`, `components/`, `test/support/` or `test/CMakeLists.txt` changed, f) says that only a full build and test
run counts. Exit codes: 0 = a-e pass; 1 = a check failed; 2 = bad arguments; 3 = a-d pass but e) was skipped.

| Setting | What | Default |
|---|---|---|
| `CLANG_FORMAT` | the clang-format binary | on the PATH, then the pip package's (`pip install clang-format==17.*`) |
| `PRECHECK_BUILD_TREE` | the built openblack tree (`ninja-multi-vcpkg` preset) whose flags e) uses | none: required by e) and `ctu.py` |
| `PRECHECK_BUILD_DIR` | its build folder | `<tree>/cmake-build-presets/ninja-multi-vcpkg` |
| `VCVARS64` | `vcvars64.bat` | found with vswhere |
| `PRECHECK_TMP` | logs and objects | `RE_TMP`, else the system's temp folder |
| `PRECHECK_DENY_WORDS` | a regular expression of words no added line may contain (case-insensitive) | none |
| `PRECHECK_TEST_TREE` | a worktree holding a Release build of the same commit | none: f) only lists the groups |
| `PRECHECK_TEST_BIN` | that build's executables folder | `<test tree>/cmake-build-presets/ninja-multi-vcpkg/bin/Release` |
| `PRECHECK_LOCK`, `PRECHECK_SLOT_GLOB`, `PRECHECK_MAX_SLOTS` | on a machine shared by several builds: a lock folder made while a compile batch runs, and a glob of markers of other running builds; e) waits while `PRECHECK_MAX_SLOTS` (default 2) of them exist | no waiting |

`ctu.py <clone> <files...> [--like=src/Game.cpp]` is the standalone compile on its own: single translation units
with the exact flags of an existing build, without touching it. A header is replaced by every `.cpp` that includes
it. The PCH is not used but its header is still force-included. A shader the clone adds or changes
(`assets/shaders/fs_*.sc` / `vs_*.sc`, also through an included `.sh` or `varying.def.sc`) is first generated with the
build's own shaderc commands into the temp folder. It prints `FAILS=<n>`; the exit code counts the failed TUs, any
file with no build rule and any shader shaderc refused.

`joint_dry.sh <base-commit> <setlist-file> [--no-compile]` (local only) rehearses a joint: it re-clones a throw-away
`joint_dry` clone, applies each patch of the setlist in order with `apply_set.sh` (which resolves docs/CMake
keep-both and include-only conflicts), hands a testbed-scenario conflict to `resolve_kinds.py`, stops at any other
code conflict, then runs precheck on what was applied and writes a dated report. Exit codes as precheck's, and 1 when
it stopped. The joints themselves apply with `apply_set2.sh`, which also keeps both sides of a conflict in the
testbed's registry and runner.

Limits: Windows and MSVC only for e) and `ctu.py`; the standalone compile takes about 25 s per TU without the PCH
(a 230-file joint takes about 1.5 h), so use `--no-compile` for a quick pass; a new file is compiled with the
`--like` file's flags; the brace count is lexical; the address pattern misses an address written in decimal or
without `0x`; f) compares file times against the test tree's working copy, so a fresh checkout makes every exe look
older.

### hashdiff: which entity moved first

openblack can write a per-turn hash of the game state (`src/Debug/StateHash.cpp`). When two runs that should
behave the same differ, the hash only says that a part (for example `transform`) differs from some turn on;
`hashdiff.py` (Python 3.8, standard library) compares two traced runs and names the first entity that changed and
what changed in it.

With `OPENBLACK_STATE_HASH=<dir>/<label>_hash.txt`, set next to it
`OPENBLACK_STATE_HASH_ENTITIES=transform,creature` (or `all`), and each turn also writes
`<dir>/<label>_hash_entities.txt`. The `_hash.txt` file is byte-identical with or without it; without
`OPENBLACK_STATE_HASH` the second variable does nothing. Run the base and the changed build once each, the same
deterministic way, then:

    python hashdiff.py <base>_hash_entities.txt <run>_hash_entities.txt [--part transform] [--max 20]
    python hashdiff.py --self-test

Exit code 0 means the compared parts are identical, 1 that they differ. The output gives the first turn that
differs, the entities only one run has, and per entity the values that changed (old -> new). The traced files are
for one investigation: do not commit them or keep them as references (with `all` they are large). Both files are read
whole: the copy beside the clone refuses a pair that passes 1.5 GB without `--part` (a long run's pair once took
11 GB), so cut a window of turns round the first differing one first.

File format: `turn <n>`, then `<n> <part> <key> <value> ...`. `key` is `e<entity id>` (the raw entt id, with its
version) for values folded after `Hasher::Entity`, or `-` for a part that names no entity. Values: `u<decimal>`
(U32), `x<hex>` (U64), `f<8 hex bits>/<shortest decimal>` (Float), `b<hex>` (raw bytes). A turn number going back (a
land load) starts a new segment. The trace is written by the same `Hasher` calls that make the hash, so it cannot
drift; `test/test_state_hash_trace.cpp` checks that the hash is the same traced and not.

With `OPENBLACK_STATE_HASH_SCRIPTS=1` as well (for a single run or a `--scenarios` list alike), a `scripts` part
follows the built-in ones: every script global after the null variable as `e<its place>` (its type, then its value, a
float by its bits), then every task in the order of its number as `e<1000000 + its number>` (its script's id and the
instruction it has reached). Without the switch there is no such part and the hash file is unchanged, so it is a
separate series of references: compare a run with it only against another run with it. It tells two runs of a story
chapter that reach the same quest state apart from two that do not ([Playing a timeline on a land](scenarios.md#playing-a-timeline-on-a-land)).

Limits: entities are named by id only; parts whose owners do not call `Hasher::Entity` (`pools`, `footpaths`,
`town_owner_list`, `wonder_power`, `player_alignment`, `random`, `crt`, `clock`) are one `-` record per turn; field
names are known only for `transform` (pos, rot<column><row>, scale), `creature` and `town_belief` (belief<p>,
pending<p>, recent<p>, boredom<r>). `town_belief` (2026-10-20) is one
record per town, newest first: its belief, pending belief and recent belief per player (8 each), then its boredom
per reaction (41), so a town's belief that differs is seen even when nothing moves.

### The published packages

The source package (openblack's tree with this project's changes and the wiki), the ready-to-play Windows 64-bit
package and the research-reports package each carry a README; none includes a file of the original game. Building
from source on Windows needs Visual Studio 2022 (or the Build Tools, "Desktop development with C++") and Git; the
first `build.bat` has vcpkg build every dependency, which takes 30 to 90 minutes and about 10 GB of disk.

### Test hooks

The tools' own switches are above (`OPENBLACK_STATE_HASH`, `OPENBLACK_STATE_HASH_ENTITIES`,
`OPENBLACK_STATE_HASH_SCRIPTS`, `OPENBLACK_FIXED_FRAME_MS`, `OPENBLACK_PROFILE`); the full list is in
[Debugging openblack](debugging.md).

## Pending

- **Converting the original assets to open formats.** Which assets and which target formats is not written down
  yet; to be decided.
- **Real-ESRGAN on the NPU** (possible, with doubts: many PCs have no NPU). Use the NPU (Intel AI Boost, through
  OpenVINO) to upscale textures ×4 with Real-ESRGAN faster and without loading the CPU. A tool only, it does not
  change the game; for when openblack is complete.
- `test_help_dude_file.cpp` (file scope), unverified: "the clip samplers (src/Help/SpiritAnimClip.h, was CAnim.h)
  against values of fn_00860E00 / fn_00861EE0 / 0x839F10 run from runblack.exe under an x86 emulator." The note does
  not name the script. A grip evaluation script (`grip_eval.py`) mentions those addresses, but it was not checked
  that this is the script; the oracle can now give the same values directly (`oracle.py quick 0x860E00 ...`).
- **hashdiff**: no archetype or type name per entity yet; the parts traced as one record per turn could be split if
  their owners called `Hasher::Entity` before each entity's values.
- **joint_dry**: no fallback yet when `git am --3way` cannot build a fake ancestor (a docs file's preimage that is in
  no repository); such a patch stops the rehearsal and is resolved by hand. The `resolve_kinds.py` it hands a
  testbed-scenario conflict to was a temporary file outside the repository and is gone, so such a conflict stops the
  rehearsal too, even in the testbed's registry and runner, whose both sides `apply_set2.sh` keeps.
- **The share READMEs are stale**: the ready-to-play and source READMEs still describe bundled mods, a Mods menu and
  FFmpeg, all removed ([The minimal mod loader](mod-loader.md)); they need rewriting before the next package.
