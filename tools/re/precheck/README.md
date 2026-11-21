# precheck: read-only checks before handing over commits

`precheck.sh` checks an openblack clone's commits `base..HEAD` the way a reviewer and the CI would, without a full
build: formatting of the added lines, test registration, brace balance, words that must not be published, a
standalone compile of every changed `.cpp`, and which test groups the change reaches. The clone is never modified
(clang-format runs with `--dry-run`); logs go to a temp folder.

`ctu.py` is the standalone compile on its own: it compiles single translation units of a clone with the exact flags
of an existing openblack build, without touching that build.

## Install

- A POSIX shell (Git Bash on Windows), git and Python 3.8 or newer (standard library only).
- clang-format 17 (the CI's version): on the PATH, `pip install clang-format==17.*`, or `CLANG_FORMAT`.
- For e) only (Windows): Visual Studio 2022 or its Build Tools with the C++ workload, and one openblack tree that has
  been configured and built with the `ninja-multi-vcpkg` preset. Its ninja files give the flags and its build folder
  the generated and vcpkg headers. It is only read.

Settings (environment variables, or `tools/re/local_config.json`; see [../README.md](../README.md)):

| Setting | What | Default |
|---|---|---|
| `CLANG_FORMAT` | the clang-format binary | clang-format on the PATH, then the pip package's |
| `PRECHECK_BUILD_TREE` | the built openblack tree whose flags e) uses | none: required by e) and `ctu.py` |
| `PRECHECK_BUILD_DIR` | its build folder | `<tree>/cmake-build-presets/ninja-multi-vcpkg` |
| `VCVARS64` | `vcvars64.bat` | found with vswhere |
| `PRECHECK_TMP` | logs and objects | `RE_TMP`, else the system's temp folder |
| `PRECHECK_DENY_WORDS` | a regular expression of words no added line may contain (case-insensitive), for example the names of private work areas or a user name | none: d) checks addresses only |
| `PRECHECK_TEST_TREE` | a worktree holding a Release build of the same commit, whose test group executables f) runs | none: f) only lists the groups |
| `PRECHECK_TEST_BIN` | that build's executables folder | `<test tree>/cmake-build-presets/ninja-multi-vcpkg/bin/Release` |
| `PRECHECK_LOCK`, `PRECHECK_SLOT_GLOB`, `PRECHECK_MAX_SLOTS` | for a machine shared by several builds: a lock folder made while a compile batch runs, and a glob of markers of other running builds; e) waits while `PRECHECK_MAX_SLOTS` (default 2) of them exist | no waiting |

## precheck.sh

    sh precheck.sh <clone> <base> [--no-compile]
    sh tools/re/precheck/precheck.sh . origin/master

| Check | What it does | Fails when |
|---|---|---|
| a) clang-format | `clang-format --dry-run --Werror --style=file` on the lines added since base | any file would change; it is listed |
| b) cmake tests | every test source `test/CMakeLists.txt` names exists, and no test name is listed twice | a source is missing or a name is listed twice |
| c) braces/parens | per changed `.cpp/.h/.hpp/.inl`, `{ }` and `( )` counted outside comments, strings, char and raw-string literals, compared with the base version | the imbalance differs from the base's (a new file must be balanced) |
| d) words/addresses | added lines: the `PRECHECK_DENY_WORDS` words; in comments of `src/` code files, addresses of the original exe (`0x4xxxxx`..`0x9xxxxx`, optionally `0x00`-prefixed) and decompiled names `fn_XXXXXXXX` (the project keeps those in the wiki, not in code) | any hit. `docs/wiki-html` (generated) is not searched |
| e) standalone compile | `ctu.py` on every changed `.cpp` under `src/` (new files with `src/Game.cpp`'s flags) and `test/` (with `test/test_influence.cpp`'s), one TU at a time | `FAILS` > 0 or any `NO BUILD RULE` |
| f) test groups | maps changed test files (and test headers, through the sources that include them) to their group in `test/CMakeLists.txt`. With `PRECHECK_TEST_TREE`, runs each group's executable with `--gtest_brief=1` when that tree holds exactly the clone's HEAD and the exe is newer than every changed file | informative only |

If `src/`, `components/`, `test/support/` or `test/CMakeLists.txt` changed, f) says that only a full build and test
run counts (every group links them).

Exit codes: 0 = a-e pass; 1 = a check failed; 2 = bad arguments; 3 = a-d pass but e) was skipped (`--no-compile`).
The logs (compile logs, test output) are in `<PRECHECK_TMP>/precheck/<date>_<pid>/`.

## ctu.py

    python ctu.py <clone> <files...> [--like=src/Game.cpp]
    python tools/re/precheck/ctu.py . src/Common/GameRandom.cpp src/3D/AffineMatrix.h

A header is replaced by every `.cpp` that includes it by name. The PCH is not used (`/Yu`, `/Fp` dropped) but its
header is still force-included, so the TU sees the same headers as in the real build. A shader the clone adds or
changes (`assets/shaders/fs_*.sc` / `vs_*.sc`, also when an included `.sh` or `varying.def.sc` changed) has no
generated header in the build tree, so ctu generates it first with the build's own shaderc commands into the temp
folder. It prints `FAILS=<n>`; the exit code is the number of failed TUs, plus any file with no build rule (never a
silent pass) and any shader shaderc refused.

## Limits

- Windows and MSVC only for e) and `ctu.py` (they replay the ninja build's `cl` rules).
- The standalone compile takes about 25 s per TU without the PCH; use `--no-compile` for a quick pass.
- A new file is compiled with the `--like` file's flags, so a file with special flags (no PCH, extra defines) may
  differ from the real build.
- The brace count is lexical; a macro that opens a brace in one file and closes it in another shows as a difference
  only if the file is new or its count moved.
- The address pattern covers the exe's code range; an address written another way (decimal, without `0x`) is missed,
  and an unrelated hex constant in that range in a comment is flagged.
- f) compares file times against the test tree's working copy, so a fresh checkout makes every exe look older.
