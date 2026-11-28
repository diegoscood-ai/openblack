# Reverse-engineering tools

openblack must behave exactly like the original Black & White. These are the tools the project uses to find out how
the original behaves and to check that a change in openblack keeps or reaches that behaviour.

| Tool | What it is for |
|---|---|
| [xref](xref/README.md) | a cross-reference index of the original `runblack.exe`: who reads, writes or calls what, who uses a vtable slot, a string or a float constant; and shared notes on what a function does |
| [oracle](oracle/README.md) ([guide](oracle/GUIDE.md)) | runs the original's own functions natively for exact results: the bits of a formula, a random draw, the arguments a function passes to its callee; golden vectors that openblack's ports are checked against |
| [precheck](precheck/README.md) | read-only checks of a clone's commits before they are handed over: formatting, test registration, brace balance, denied words and addresses in code comments, a standalone compile of each changed file, the test groups to run |
| [hashdiff](hashdiff/README.md) | when two runs' per-turn state hashes differ, names the first entity that changed and what changed |

## No game code here

Nothing in this folder contains bytes, disassembly or data from the game. The tools read **your own copy** of the
game at run time, and what they produce from it (the xref index, oracle vectors, disassembly on screen) stays on your
machine: those files are gitignored. Do not commit them, or paste disassembly into published files. A test pins
only the few numbers it checks, and the evidence (addresses, what the code does) goes in the wiki, `docs/bw1-notes`.

The project's target is the v1.42 unofficial-patch `runblack.exe` (sha1 `1e73ba9a96c3d6316e9f8f41a8947a25790d2698`).
The decompilation, [bw1-decomp](https://github.com/openblack/bw1-decomp), is built from Lionhead's 1.20: its C and
its symbol names are the guide, and where the patch changed code the exe's bytes win. The xref tests pin facts of the
v1.42 exe.

## Settings

The tools take their paths from environment variables, or from `tools/re/local_config.json`, a flat
`{"NAME": "value"}` file next to this README that is gitignored (copy `local_config.example.json`). An environment
variable wins over the file. A tool that needs a setting that is missing stops with a message naming it.

| Setting | Used by | What |
|---|---|---|
| `RUNBLACK_EXE` | xref, oracle | your copy of the original `runblack.exe`, opened read only |
| `BW1_DECOMP` | xref, oracle | a bw1-decomp clone (its `config/BW1W120/symbols.txt` names the exe's functions) |
| `BW1_SYMBOLS` | xref, oracle | the symbols file itself, instead of `BW1_DECOMP` |
| `RE_TMP` | all | a folder for temporary files (default: the system's temp folder) |

Each tool's README lists its own optional settings (`XREF_DB`, `ORACLE_HOST`, `PRECHECK_BUILD_TREE`, ...).
`python re_config.py get NAME` prints what a setting resolves to.

## Install

- Python 3.10 or newer; `pip install pefile capstone` (xref, `oracle/disasm_exe.py`). The others need only the
  standard library.
- Windows with Visual Studio 2022 (or its Build Tools, C++ workload) for the oracle's host and precheck's standalone
  compile. xref and hashdiff run anywhere Python runs; precheck's other checks need a POSIX shell (Git Bash on
  Windows) and clang-format 17.

If you run the Python tools with `python -I` (isolated mode), packages installed with `pip install --user` are hidden;
install them for the interpreter, or use `python -E -P`.

## The typical workflow

1. **Read the decompilation first.** Find the function in bw1-decomp's C (`src/`) and read what it does.
2. **Check every claim of "only" or "never" with xref.** Before analysing a function, `python xref/xref.py show
   <function>` gives its summary, the notes others left on it, and its disassembly with names; `writers`, `readers`,
   `callers`, `callees`, `vtcalls`, `str` and `const` answer who touches what. Afterwards leave a note
   (`xref.py note <function> <author> "<what it does>" --text "<where the evidence is>"`).
3. **Get the exact numbers from the oracle.** `python oracle/oracle.py quick <function> <args>` runs the original
   function; `--stub` replaces or records a callee; `gen_vectors.py` makes thousands of cases from a spec. Pin
   openblack's tests to those values (`vectors_to_cpp.py` prints a few as a C++ table).
4. **Write the port and its tests**, then run `sh precheck/precheck.sh <clone> <base>` before handing the commits
   over.
5. **When a run's state hash moves**, run both builds with `OPENBLACK_STATE_HASH_ENTITIES` and let
   `hashdiff/hashdiff.py` name the first entity and turn that differ, and how.
6. **Write down what you found** in the wiki page of the subject (`docs/bw1-notes`), with the evidence.
