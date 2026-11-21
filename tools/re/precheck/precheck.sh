#!/bin/sh
# precheck.sh <clone> <base> [--no-compile]
# Read-only checks of an openblack clone's commits base..HEAD before they are handed over. Nothing is written into the
# clone; logs go under PRECHECK_TMP (default: RE_TMP, else the system's temp folder). See README.md for each check
# and the settings.
set -u
here=$(cd "$(dirname "$0")" && pwd)
cfg() { python -I "$here/../re_config.py" get "$@"; }
clone=${1:?usage: precheck.sh <clone> <base> [--no-compile]}
base=${2:?usage: precheck.sh <clone> <base> [--no-compile]}
shift 2
compile=1
for a in "$@"; do
	case "$a" in
	--no-compile) compile=0 ;;
	*) echo "precheck.sh: unknown argument $a" >&2; exit 2 ;;
	esac
done
[ -d "$clone/.git" ] || [ -f "$clone/.git" ] || { echo "precheck.sh: $clone is not a git clone" >&2; exit 2; }
git -C "$clone" cat-file -e "$base^{commit}" 2>/dev/null || { echo "precheck.sh: unknown base $base" >&2; exit 2; }
clone=$(cd "$clone" && pwd)
tmp=$(cfg PRECHECK_TMP)
[ -n "$tmp" ] || tmp=$(cfg RE_TMP)
[ -n "$tmp" ] || tmp=${TMPDIR:-${TEMP:-/tmp}}
py="python -I -X utf8 $here/precheck_lib.py"
stamp=$(date +%Y%m%d_%H%M%S)
out=$tmp/precheck/${stamp}_$$
mkdir -p "$out" || exit 2

[ -z "$(git -C "$clone" status --porcelain --untracked-files=no)" ] ||
	echo "WARNING: the clone has uncommitted changes; the checks read the working tree but only base..HEAD is exported"
echo "precheck $clone base $base ($(git -C "$clone" rev-list --count "$base..HEAD") commits)"

res_a=FAIL res_b=FAIL res_c=FAIL res_d=FAIL res_e=FAIL res_f=info
echo "--- a) clang-format of the added lines"
$py fmt "$clone" "$base" && res_a=ok
echo "--- b) test/CMakeLists.txt registration"
$py cmake "$clone" "$base" && res_b=ok
echo "--- c) brace and parenthesis balance"
$py braces "$clone" "$base" && res_c=ok
echo "--- d) denied words, addresses in src/ comments"
$py names "$clone" "$base" && res_d=ok

echo "--- e) standalone compile of the changed .cpp files"
$py cpps "$clone" "$base" > "$out/cpps.txt"
nsrc=$(grep -c '^src ' "$out/cpps.txt") ntest=$(grep -c '^test ' "$out/cpps.txt")
echo "$nsrc src, $ntest test files (about 25 s each, one at a time)"
if [ $compile = 0 ]; then
	res_e=SKIP
else
	efail=0
	for kind in src test; do
		files=$(sed -n "s/^$kind //p" "$out/cpps.txt")
		[ -n "$files" ] || continue
		like=src/Game.cpp
		[ $kind = test ] && like=test/test_influence.cpp
		# shellcheck disable=SC2086
		python -I "$here/ctu.py" "$clone" $files --like=$like > "$out/compile_$kind.log" 2>&1
		rc=$?
		grep -E "^FAILED |^NO BUILD RULE|^FAILS=| error C|^ctu.py: " "$out/compile_$kind.log" | head -40
		grep -q "NO BUILD RULE" "$out/compile_$kind.log" && efail=1
		grep -q "^FAILS=0" "$out/compile_$kind.log" || efail=1
		[ $rc = 0 ] || efail=1
		echo "$kind: exit $rc (log $out/compile_$kind.log)"
	done
	[ $efail = 0 ] && res_e=ok
fi

echo "--- f) test groups"
$py groups "$clone" "$base" > "$out/groups.txt"
grep -v '^GROUPS=\|^SRC_CHANGED=' "$out/groups.txt"
groups=$(sed -n 's/^GROUPS=//p' "$out/groups.txt")
srcchg=$(sed -n 's/^SRC_CHANGED=//p' "$out/groups.txt")
if [ "$srcchg" = 1 ]; then
	echo "src/, components/ or test support changed: every group links them, so only a full build and test run counts"
fi
# PRECHECK_TEST_TREE: a worktree with a Release build; its group executables are run only when that tree holds
# exactly the clone's HEAD and each exe is newer than every changed file
wt=$(cfg PRECHECK_TEST_TREE)
if [ -n "$groups" ]; then
	echo "test groups to build and run: $groups"
	if [ -z "$wt" ]; then
		echo "(set PRECHECK_TEST_TREE to run the built group executables of a test worktree)"
	else
		bin=$(cfg PRECHECK_TEST_BIN "$wt/cmake-build-presets/ninja-multi-vcpkg/bin/Release")
		for g in $(echo "$groups" | tr ',' ' '); do
			exe=$bin/$g.exe
			if [ ! -f "$exe" ]; then echo "$g: no exe in $bin"; continue; fi
			if [ "$(git -C "$wt" rev-parse HEAD^{tree} 2>/dev/null)" != "$(git -C "$clone" rev-parse HEAD^{tree})" ] ||
				[ -n "$(git -C "$wt" status --porcelain --untracked-files=no --ignore-submodules=all 2>/dev/null)" ]; then
				echo "$g: the test tree is not this clone's tree, exe not run"
				continue
			fi
			newer=1
			for f in $(git -C "$clone" diff --name-only --diff-filter=d "$base..HEAD"); do
				[ "$exe" -nt "$wt/$f" ] || { newer=0; break; }
			done
			if [ $newer = 0 ]; then echo "$g: exe older than a changed file, not run"; continue; fi
			(cd "$bin" && "./$g.exe" --gtest_brief=1) > "$out/$g.log" 2>&1
			rc=$?
			echo "$g: exe run, exit $rc ($(grep -E '^\[  (PASSED|FAILED) ' "$out/$g.log" | tr '\n' ' '))"
			[ $rc = 0 ] && res_f="$res_f $g:pass" || res_f="$res_f $g:FAIL"
		done
	fi
fi

echo
echo "=== precheck summary ($clone, base $base)"
printf '  %-28s %s\n' "a) clang-format" "$res_a" "b) cmake tests" "$res_b" "c) braces/parens" "$res_c" \
	"d) words/addresses" "$res_d" "e) standalone compile" "$res_e" "f) test groups" "${groups:-none} $res_f"
echo "  logs: $out"
for r in $res_a $res_b $res_c $res_d; do [ "$r" = ok ] || exit 1; done
[ "$res_e" = ok ] && exit 0
[ "$res_e" = SKIP ] && exit 3
exit 1
