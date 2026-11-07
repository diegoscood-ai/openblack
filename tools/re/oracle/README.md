# oracle: run the original game's functions for exact numbers

The oracle calls functions of the original `runblack.exe` natively (no emulation), at the game's FPU settings, and
records their exact results. It answers "what exact bits does the original give for this input", "which arguments
does this function pass to its callee", and it turns those answers into golden vectors that openblack's ports are
checked against bit for bit. Daily recipes: [GUIDE.md](GUIDE.md).

Nothing from the game is stored in this repository: the host maps your own copy of the exe at run time, and the
vectors it writes stay on your machine (`vectors/` is gitignored; a test embeds only the few numbers it needs).

| File | What |
|---|---|
| `oracle_host.cpp`, `build.bat` | the 32-bit host: maps the exe at its image base and answers JSON-line requests |
| `oracle.py` | the Python client: starts the host, `call()`, symbol lookup (bw1-decomp's `symbols.txt`), a small CLI |
| `gen_vectors.py`, `specs/*.json` | a spec -> golden vectors in `vectors/<function>.json`; `specs/variants/` hold variants |
| `cmp/compare.cpp`, `cmp/build_cmp.bat` | a 64-bit check of openblack's code against the vectors |
| `vectors_to_cpp.py` | a few cases of a vector file as a C++ constant table for a gtest |
| `disasm_exe.py` | `disasm_exe.py ADDR:LEN` disassembles a range of your exe on screen (capstone) |

## Install

- Windows (the host is a Win32 program; it also runs the exe's code, so it must be x86 or x64 Windows).
- Visual Studio 2019 or 2022, or its Build Tools, with the C++ workload.
- Python 3.8 or newer; `pip install capstone` for `disasm_exe.py` only.
- Your copy of `runblack.exe`: the project targets the v1.42 unofficial patch. Keep a read-only copy for the oracle;
  the host only ever opens it for reading.
- A clone of [bw1-decomp](https://github.com/openblack/bw1-decomp) for the symbol names.

Settings (environment variables, or `tools/re/local_config.json`; see [../README.md](../README.md)):

| Setting | What | Default |
|---|---|---|
| `RUNBLACK_EXE` | your `runblack.exe` | none: required |
| `BW1_DECOMP` or `BW1_SYMBOLS` | bw1-decomp's clone, or its `config/BW1W120/symbols.txt` | none: required |
| `ORACLE_HOST` | the built host | `oracle_host.exe` next to `oracle.py` |
| `ORACLE_BUILD_OUT` | where `build.bat` puts the host and its objects | this folder |
| `VCVARS32` | `vcvars32.bat`, if vswhere cannot find it | found with vswhere |

## Build and run

    build.bat                                     # oracle_host.exe (vcvars32, cl; fixed base, no ASLR)
    python oracle.py sym GetYXZ                   # find a symbol
    python oracle.py quick NormaliseAngle f32:7   # one call: the result's bits and value
    python oracle.py call ?SetYXZMatrixOnly@LHMatrix@@QAEXMMM@Z --conv thiscall --alloc m=48 --this @m --read @m:36 f32:0.5 f32:0.25 f32:-1
    python gen_vectors.py --all                   # every specs/*.json -> vectors/ (variants: specs/variants/*.json)
    cmp\build_cmp.bat                             # cmp\compare.exe (vcvars64; compiles openblack's sources read only)
    cmp\compare.exe [--json out.json] [files or dirs]   # default: ..\vectors

`build_cmp.bat` reads openblack's sources from this repository (or `OPENBLACK_SRC`) and the vcpkg headers of a normal
build of it (`VCPKG_INC`); see the comments at its top.

## How the host works

The host never runs the exe's entry point, CRT start-up or static initialisers. It copies the exe's headers and
sections to their addresses (the exe has no relocations), then resolves every import to a trap that reports its name.
When the exe's address range is already taken in a fresh process (system mappings, injected DLLs), the host starts a
suspended copy of itself, reserves the range in it before its loader runs, and lets the copy load the exe (a
kill-on-close job ties the two). Calls run on a worker thread with a 16 MB stack.

**Machine baseline.** `oracle.py` then runs what the game does once at start-up and snapshots it: the game's CPU
check (native `cpuid`). Its dispatcher picks a code path by CPU family: a family 6 CPU (every Intel Core) gets the
Pentium III path, so the float-to-integer conversion takes its x87 branch; a family 0xF CPU (Pentium 4, some AMD) gets
the SSE2 one. The two differ only for NaN and out-of-range values; `.sse2` spec variants force the other path. The
baseline also sets the game's MXCSR (flush to zero). `Oracle(baseline=False)` skips all this. The vectors record the
result under `"machine"`.

## Protocol (one JSON object per line)

Request (all keys optional except `addr` for a call):

```
{"id": any, "addr": "0x...", "conv": "cdecl|stdcall|thiscall|fastcall", "this": "0x...|@buf",
 "args": [{"type": "i32|u32|i16|u16|i8|u8|bool|ptr|f32|f64|i64|u64", "v": 1.5 | "0x..." | "@buf+0x10" | "nan"} or {"type": "f32", "bits": "0x3F800000"}],
 "ret": "void|i32|u32|i16|u16|i8|u8|bool|ptr|i64|u64|f32|f64|st0",
 "alloc": [{"name": "buf", "size": 48, "hex": "...", "fill": 0, "align": 16}],
 "write": [{"addr": "0x...|@buf", "hex": "..."}], "read": [{"addr": "0x...|@buf", "len": 36}],
 "regs": {"eax|ebx|ecx|edx|esi|edi": "0x..."}, "fpu_push": [{"type": "f32|f64|f80", "bits": "0x..."}],
 "fpu_cw": "0x007F", "timeout_ms": 5000, "reset": true | "pristine", "snapshot": true}
{"batch": [request, ...]}        {"cmd": "ping|state|imports|quit"}
```

Order inside a request: reset, alloc, write, call, read, snapshot. Buffers live in a fixed arena, are freed by the next
request and have a guard area checked after the call. Stack arguments are pushed right to left. For thiscall `this`
goes in ECX. For fastcall `this` (if given) and then the first integer arguments go in ECX, EDX; floats always go on
the stack. `regs` overrides registers for hand-written conventions, and `fpu_push` loads x87 inputs (the last one is
ST0). ESP is saved before the call and restored after, so a wrong convention cannot break the host; the response
reports what the callee actually popped (`callee_pop`) and warns on a mismatch. Before the call: `fninit`, the game's
control word (0x007F: 24-bit precision, round to nearest) and the game's MXCSR; after it the host's are restored.

Response: `{"ok": true, "ret": <value>, "ret_hex": "...", "read": ["hex", ...], "eax", "edx", "callee_pop",
"fpu_sw", "fpu_depth", "st0_hex", "buffers", "warn": [...]}`. Float returns come from ST0. `ret_hex` holds the
returned type's bits (f32: what `fstp dword` stores). For `st0` it holds the 80-bit value, plus `ret_f32_hex` and
`ret_f64_hex`. Failures: `{"ok": false, "error": "...", "trap": {"import", "iat", "from"} | "exception": {"code",
"addr", "access", "target"} | "timeout": {"eip"}}`. Writes to the exe's memory persist until `reset` (back to the last
`snapshot`, that is the baseline; `"pristine"` = the file's image).

## Spec format (`specs/*.json`)

```
{"function": "GetYXZ",                       // selects the openblack port in compare.cpp; also the vector file name
 "symbol": "?GetYXZ@LHMatrix@@QBEXPAM00@Z", "addr": "0x...",   // either or both (checked against each other)
 "conv": "thiscall", "ret": "void", "this": "m",
 "buffers": {"m": [{"name": "m", "type": "f32", "count": 12, "gen": "matrix"}],          // input
             "y": [{"name": "y", "type": "f32", "out": true}]},                          // output (gen + out = in/out)
 "args": [{"type": "ptr", "buffer": "y"}, {"name": "r", "type": "f32", "gen": {"uniform": [-7, 7]}}],
 "generators": {"matrix": {"mix": [[70, {"rotation": {"angle": {"uniform": [-3.15, 3.15]}}}], [30, {"uniform": [-1, 1]}]]}},
 "edges": {"r": [0, "-0", "nan", "0x00000001"]},      // each value once, the other inputs random
 "cases": [{"m": [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0]}],
 "random_cases": 4000, "seed": 102,
 "setup": [{"addr": "<a set-up function>"}, {"write": [{"addr": "0x...", "hex": "01000000"}]}],
 "reset_each": false, "notes": "...",
 "globals": [{"name": "seed", "addr": "0x...", "type": "u32", "gen": {"bits": true}, "out": true}],  // fixed
             // addresses (symbol[+off] too): gen / value written before each call, out read after it
 "stubs": [{"target": "0x... | symbol | DLL!Import", "type": "i32|f32|void|...", "value": 0, "pop": 0}],
 "fake_game": true,                // the game object's pointer -> a fake holding the two random seeds
 "chain": {"seed": "seed"},        // input <- the previous case's output: draw sequences
 "extends": "../Base.json", "vectors": "Base.variant"}   // a variant: its keys over the base's, its own file name
```

Stubs and the fake game object live in the unused zero tail of the code section, so `reset` covers them and a
`snapshot` keeps them.

Generators: `uniform [lo, hi]`, `int [lo, hi]`, `loguniform [lo, hi]` (+ `signed`), `choice [...]`, `bits`
(random bits), `near` (another input + `delta`), `mix [[weight, gen], ...]` (one branch per field), `rotation`
(9 / 12 cells of YXZ angles, optional `scale`, `translation`). Values may be numbers, `"0x..."` raw bits, `"nan"`,
`"inf"`, `"-inf"`, `"-0"`. Field types: f32 f64 i32 u32 i16 u16 i8 u8 ptr; fields pack in order at natural alignment
unless they give `offset`.

Vector file: `format`, `function`, `symbol`, `addr`, `conv`, `ret`, `exe` (path, size, sha1), `fpu_cw`, `machine`,
`setup`, `seed`, `inputs` / `outputs` (name, type, count), `constants`, and `cases`: one per line,
`{"in": {name: "0x..." | [...]}, "out": {...}}` or `{"in": ..., "error": "..."}` when the exe faulted. Every value is
its exact bits in hex (f80 = 20 digits, sign and exponent first).

## Limits

- Every import is a trap: a function that calls the OS, the game's own DLLs or the CRT's heap / TLS / `signal` fails
  with the import's name and caller. Data imports (variables read through the import table) read the stub instead.
- No static initialisers run: tables built at start-up are zero until a `setup` builds them. Functions with heavy
  global state need `write` / `setup` (or a `snapshot` after building it).
- thiscall / fastcall member functions need a `this` buffer of the class's size; virtual calls need a real vtable.
- Single thread. The watchdog aborts calls stuck in exe code (`timeout_ms`). A fault inside a call is reported, and
  memory the call wrote before it stays written (use `reset`).
- `compare.exe` compiles the few openblack files it tests. A new port needs a lambda in `Ports()` and, if it lives in
  another file, that file in `build_cmp.bat`. Vectors with no registered port are listed as such.
- Do not publish vector files or disassembly: they are made from the game's code. Publish the specs, and in tests
  only the handful of input and output numbers a check needs.

## Using the vectors in openblack tests

A test reading a whole vector file is an integration test: it skips itself when the file is absent (the CI has no
exe and no vectors). It reads the file with nlohmann-json (already a vcpkg dependency):

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
		const MapCoords from {static_cast<int32_t>(bits(in["from.x"])), static_cast<int32_t>(bits(in["from.z"])), 0.0f};
		const MapCoords to {static_cast<int32_t>(bits(in["to.x"])), static_cast<int32_t>(bits(in["to.z"])), 0.0f};
		EXPECT_EQ(gutils::GetAngleFromXZ(from, to), bits(c["out"]["ret"])) << c.dump();
	}
}
```

For the unit tests that run in CI, embed a few cases instead (`vectors_to_cpp.py`, see GUIDE.md). `cmp/compare.cpp`
does the same for every vector file and classifies mismatches (1 ulp, more than 1 ulp, signed zero, NaN bits). It
also runs openblack's code under SEH, so a case where both the exe and openblack fault counts as agreement.
