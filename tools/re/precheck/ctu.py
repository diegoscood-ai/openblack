"""Compile single translation units of a clone with the exact flags of an existing openblack build (Windows, MSVC).

Usage: python ctu.py <clone> <rel-path>... [--like=<rel-path>]   (paths like src/ECS/MapCells.cpp)
Headers are replaced by every .cpp that includes them by name. A file with no rule in the build (a new file) takes
the rule of --like, else of src/Game.cpp or test/test_influence.cpp.
The PCH is not used (/Yu, /Fp dropped) but the PCH header is still force-included, so the TU sees the same headers as
in the real build. Objects go to a temp folder; nothing is written to the build tree. Exit code = number of failed
TUs. Settings: see README.md (PRECHECK_BUILD_TREE is required).
"""
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import re_config  # noqa: E402

# the tree whose configured Release build gives the rules: any openblack checkout built with the ninja-multi-vcpkg
# preset; it is only read
SHARED = os.path.normpath(re_config.require(
    "PRECHECK_BUILD_TREE", "an openblack tree built with the ninja-multi-vcpkg preset, whose flags are used"))
BUILD = os.path.normpath(re_config.get("PRECHECK_BUILD_DIR", os.path.join(SHARED, "cmake-build-presets",
                                                                           "ninja-multi-vcpkg")))
NINJA = os.path.join(BUILD, "CMakeFiles", "impl-Release.ninja")
if not os.path.isfile(NINJA):
    sys.exit("ctu.py: no %s: configure and build %s once (or set PRECHECK_BUILD_DIR)" % (NINJA, SHARED))


def find_vcvars():
    v = re_config.get("VCVARS64")
    if not v:
        vswhere = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"), "Microsoft Visual Studio",
                               "Installer", "vswhere.exe")
        if os.path.isfile(vswhere):
            root = subprocess.run([vswhere, "-latest", "-products", "*", "-property", "installationPath"],
                                  capture_output=True, text=True).stdout.strip()
            v = os.path.join(root, "VC", "Auxiliary", "Build", "vcvars64.bat") if root else None
    if not v or not os.path.isfile(v):
        sys.exit("ctu.py: vcvars64.bat not found: install the Visual Studio C++ tools, or set VCVARS64")
    return v


def norm(p):
    return os.path.normcase(os.path.normpath(p.replace("$:", ":")))


def parse_blocks():
    blocks = {}
    cur = None
    with open(NINJA, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"build (\S+\.obj): (CXX_COMPILER__\S+) (\S+)", line)
            if m:
                cur = {"rule": m.group(2), "src": m.group(3)}
                blocks[norm(m.group(3))] = cur
                continue
            if cur is not None and line.startswith("  "):
                k, _, v = line.strip().partition(" = ")
                cur[k] = v
            else:
                cur = None
    return blocks


def generate_shaders(clone, tmp):
    """Shaders the clone adds or changes have no generated header in the shared build: make them with shaderc, as the
    build's own commands do (taken from the fs_object / vs_object ones), into tmp\\gen. Returns (include dir, fails)."""
    import glob
    gen = os.path.join(tmp, "gen")
    shaders = os.path.join(clone, "assets", "shaders")
    shared_shaders = os.path.join(SHARED, "assets", "shaders")
    if not os.path.isdir(shaders):
        return None, 0

    def text(p):
        try:
            with open(p, "rb") as f:
                return f.read()
        except OSError:
            return None

    changed_includes = [n for n in os.listdir(shaders) if n.endswith(".sh")
                        and text(os.path.join(shaders, n)) != text(os.path.join(shared_shaders, n))]
    varying_same = text(os.path.join(shaders, "varying.def.sc")) == text(os.path.join(shared_shaders, "varying.def.sc"))
    todo = []
    for n in sorted(os.listdir(shaders)):
        if not n.endswith(".sc") or n == "varying.def.sc" or not n.startswith(("fs_", "vs_")):
            continue
        mine = text(os.path.join(shaders, n))
        if mine == text(os.path.join(shared_shaders, n)) and varying_same and \
                not any(inc.encode() in mine for inc in changed_includes):
            continue
        todo.append(n[:-3])
    if not todo:
        return None, 0
    fails = 0
    shared_fwd = SHARED.replace(chr(92), "/")
    clone_fwd = clone.replace(chr(92), "/")
    build_fwd = BUILD.replace(chr(92), "/")
    gen_fwd = gen.replace(chr(92), "/")
    for name in todo:
        kind = "fs_object" if name.startswith("fs_") else "vs_object"
        templ = glob.glob(os.path.join(BUILD, "src", "CMakeFiles", "Release", kind + ".sc.bin.h-*.bat"))
        if not templ:
            print("SHADER", name, ": no template command in the shared build", flush=True)
            fails += 1
            continue
        with open(templ[0], encoding="utf-8", errors="replace") as f:
            cmds = [l for l in f.read().splitlines() if "shaderc" in l]
        print("=== shader", name, flush=True)
        for c in cmds:
            c = c.split(" || ")[0]
            c = c.replace(build_fwd + "/include/generated", gen_fwd + "/generated")
            c = c.replace(shared_fwd + "/assets", clone_fwd + "/assets")
            c = c.replace("/" + kind + ".sc ", "/" + name + ".sc ").replace("/" + kind + ".sc.bin.h", "/" + name + ".sc.bin.h")
            c = c.replace("--bin2c " + kind + "_", "--bin2c " + name + "_")
            out = re.search(r"-o (\S+)", c).group(1)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            r = subprocess.run(c, shell=True, capture_output=True, text=True)
            if r.returncode != 0:
                print("SHADER FAILED", name, out, (r.stdout + r.stderr)[-800:], flush=True)
                fails += 1
                break
    return gen, fails


def compile_lock():
    """Optional, for machines shared by several builds: PRECHECK_LOCK is a folder made while this batch compiles (one
    batch at a time per lock), and PRECHECK_SLOT_GLOB a glob of the markers of builds running elsewhere; the batch
    waits while PRECHECK_MAX_SLOTS (default 2) of them exist. A lock left by a dead run (older than 30 minutes) is
    taken over. Returns the lock folder, or None when PRECHECK_LOCK is unset."""
    import glob
    import time
    lock = re_config.get("PRECHECK_LOCK")
    slots = re_config.get("PRECHECK_SLOT_GLOB")
    max_slots = int(re_config.get("PRECHECK_MAX_SLOTS", "2"))
    if not lock and not slots:
        return None
    waited = False
    while True:
        busy = bool(slots) and len(glob.glob(slots)) >= max_slots
        if not busy and not lock:
            return None
        if not busy:
            try:
                os.mkdir(lock)
                return lock
            except FileExistsError:
                if time.time() - os.path.getmtime(lock) > 1800:
                    os.rmdir(lock)
                    continue
        if not waited:
            print("waiting:", "the build slots are taken" if busy else "another compile batch holds " + lock, flush=True)
            waited = True
        time.sleep(10)


def users_of(clone, header):
    name = os.path.basename(header)
    out = subprocess.run(["git", "-C", clone, "grep", "-l", "-E", r'#include [<"](.*/)?' + re.escape(name) + r'[>"]',
                          "--", "*.cpp"], capture_output=True, text=True).stdout.split()
    own = os.path.splitext(header)[0] + ".cpp"
    if os.path.exists(os.path.join(clone, own)):
        out.append(own)
    return sorted(set(out))


def main():
    clone = os.path.abspath(sys.argv[1])
    wanted = []
    # --like=<src>: a new file with no build rule yet is compiled with that file's rule
    like = next((a.split("=", 1)[1] for a in sys.argv if a.startswith("--like=")), None)
    for p in (a for a in sys.argv[2:] if not a.startswith("--")):
        p = p.replace("\\", "/")
        if p.endswith(".h") or p.endswith(".hpp"):
            wanted += users_of(clone, p)
        else:
            wanted.append(p)
    wanted = sorted(set(wanted))
    blocks = parse_blocks()
    # objects under PRECHECK_TMP (else RE_TMP, else the system's temp folder)
    obj_root = os.path.join(re_config.get("PRECHECK_TMP") or re_config.get("RE_TMP") or tempfile.gettempdir(), "tmp_obj")
    os.makedirs(obj_root, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="tu_", dir=obj_root)
    # vcvars64.bat runs vswhere by name, so its folder goes on the PATH first
    lines = ["@echo off", 'set "PATH=%PATH%;%ProgramFiles(x86)%' + chr(92) + 'Microsoft Visual Studio' + chr(92) + 'Installer"',
             f'call "{find_vcvars()}" >nul', "set FAILS=0"]
    missing = []
    # new or changed shaders: their generated headers, made here, come before the shared build's
    gen, shader_fails = generate_shaders(clone, tmp)
    for rel in wanted:
        b = blocks.get(norm(os.path.join(SHARED, rel)))
        if b is None and like is not None:
            b = blocks.get(norm(os.path.join(SHARED, like)))
        if b is None:
            # a file newer than the build tree: the rule of a file in the same target
            fallback = "src/Game.cpp" if rel.startswith("src/") else "test/test_influence.cpp" if rel.startswith("test/") else None
            b = blocks.get(norm(os.path.join(SHARED, fallback))) if fallback else None
        if b is None:
            missing.append(rel)
            continue
        flags = re.sub(r"/Yu\S+|/Fp\S+", "", b["FLAGS"])
        incs = b["INCLUDES"]
        # the clone's sources and components, the shared build dir for generated and vcpkg files
        for sub in ("src", "components", "externals", "test"):
            incs = incs.replace(SHARED + "\\" + sub, clone + "\\" + sub)
        # the build tree's rules may be older than the clone: every component include the clone has
        for comp in sorted(os.listdir(os.path.join(clone, "components"))):
            inc = os.path.join(clone, "components", comp, "include")
            if os.path.isdir(inc) and inc.lower() not in incs.lower():
                incs += f' -I"{inc}"'
        if gen:
            incs = f'-I"{gen}" ' + incs
        obj = os.path.join(tmp, re.sub(r"[\\/]", "_", rel) + ".obj")
        src = os.path.join(clone, rel.replace("/", "\\"))
        lines.append(f"echo === {rel}")
        lines.append(f'cl.exe /nologo /TP {b["DEFINES"]} {incs} {flags} /Fo"{obj}" /Fd"{tmp}\\tu.pdb" /FS -c "{src}"')
        lines.append("if errorlevel 1 (set /a FAILS+=1 & echo FAILED {0})".format(rel))
    lines.append("echo FAILS=%FAILS%")
    lines.append("exit /b %FAILS%")
    bat = os.path.join(tmp, "compile.bat")
    with open(bat, "w") as f:
        f.write("\r\n".join(lines) + "\r\n")
    for m in missing:
        print("NO BUILD RULE (not compiled):", m)
    print("TUs:", len(wanted) - len(missing), "bat:", bat, flush=True)
    # optional: one compile batch at a time per lock, and none while the build slots are taken (see compile_lock)
    lock = compile_lock()
    try:
        rc = subprocess.run(["cmd", "/c", bat]).returncode
    finally:
        if lock:
            os.rmdir(lock)
    # a file with no build rule was not compiled: it counts as a failure, never as a pass
    if missing:
        print("FAILS include", len(missing), "file(s) with no build rule", flush=True)
    if shader_fails:
        print("FAILS include", shader_fails, "shader(s) that shaderc refused", flush=True)
    sys.exit(rc + len(missing) + shader_fails)


if __name__ == "__main__":
    main()
