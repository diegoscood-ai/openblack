"""Local static server for the wiki viewer.

Serves the folder this file is in (or --root) on 127.0.0.1 only, and opens the
default browser at it. It never outlives the window that started it:

* it watches its parent process (and the parent's parent when the parent is
  only the ``py`` launcher) every few seconds and exits when one has gone;
* a console close is handled too, since the server shares that console.

A lock file in the temporary folder records the port and PID. When a live
server for the same folder already answers, a new launch only opens the
browser at it.
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import hashlib
import http.server
import json
import os
import sys
import tempfile
import threading
import time
import urllib.request
import webbrowser
from pathlib import Path

HOST = "127.0.0.1"
DEFAULT_PORT = 8765
PORT_TRIES = 50
WATCH_SECONDS = 2.0
MARKER = "/__wiki_server__"

IS_WINDOWS = os.name == "nt"

# ---------- Windows process helpers ----------

if IS_WINDOWS:
    _k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
    SYNCHRONIZE = 0x00100000
    STILL_ACTIVE = 259
    TH32CS_SNAPPROCESS = 0x2

    class PROCESSENTRY32W(ctypes.Structure):
        _fields_ = [
            ("dwSize", wt.DWORD),
            ("cntUsage", wt.DWORD),
            ("th32ProcessID", wt.DWORD),
            ("th32DefaultHeapID", ctypes.c_size_t),
            ("th32ModuleID", wt.DWORD),
            ("cntThreads", wt.DWORD),
            ("th32ParentProcessID", wt.DWORD),
            ("pcPriClassBase", ctypes.c_long),
            ("dwFlags", wt.DWORD),
            ("szExeFile", wt.WCHAR * 260),
        ]

    _k32.OpenProcess.restype = wt.HANDLE
    _k32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
    _k32.GetExitCodeProcess.argtypes = [wt.HANDLE, ctypes.POINTER(wt.DWORD)]
    _k32.GetProcessTimes.argtypes = [wt.HANDLE] + [ctypes.POINTER(wt.FILETIME)] * 4
    _k32.CloseHandle.argtypes = [wt.HANDLE]
    _k32.CreateToolhelp32Snapshot.restype = wt.HANDLE
    _k32.CreateToolhelp32Snapshot.argtypes = [wt.DWORD, wt.DWORD]
    _k32.Process32FirstW.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    _k32.Process32NextW.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]


def process_identity(pid: int) -> int | None:
    """Return the creation time of a running process, or None if it is not running.

    The creation time tells a live process apart from a later one that reused its PID.
    """
    if pid <= 0:
        return None
    if not IS_WINDOWS:
        try:
            os.kill(pid, 0)
        except OSError:
            return None
        return 0
    handle = _k32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, False, pid)
    if not handle:
        return None
    try:
        code = wt.DWORD()
        if not _k32.GetExitCodeProcess(handle, ctypes.byref(code)) or code.value != STILL_ACTIVE:
            return None
        times = [wt.FILETIME() for _ in range(4)]
        if not _k32.GetProcessTimes(handle, *[ctypes.byref(t) for t in times]):
            return 0
        return (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
    finally:
        _k32.CloseHandle(handle)


def process_table() -> dict[int, tuple[int, str]]:
    """pid -> (parent pid, exe name), from a Toolhelp snapshot."""
    table: dict[int, tuple[int, str]] = {}
    if not IS_WINDOWS:
        return table
    snap = _k32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if not snap or snap == wt.HANDLE(-1).value:
        return table
    try:
        entry = PROCESSENTRY32W()
        entry.dwSize = ctypes.sizeof(PROCESSENTRY32W)
        ok = _k32.Process32FirstW(snap, ctypes.byref(entry))
        while ok:
            table[entry.th32ProcessID] = (entry.th32ParentProcessID, entry.szExeFile.lower())
            ok = _k32.Process32NextW(snap, ctypes.byref(entry))
    finally:
        _k32.CloseHandle(snap)
    return table


def processes_to_watch(parent_pid: int) -> list[tuple[int, int]]:
    """The parent, plus its own parent while the parent is only a Python launcher."""
    watched: list[tuple[int, int]] = []
    table = process_table()
    pid = parent_pid
    for _ in range(4):
        ident = process_identity(pid)
        if ident is None:
            break
        watched.append((pid, ident))
        name = table.get(pid, (0, ""))[1]
        if name not in ("py.exe", "pyw.exe", "python.exe", "pythonw.exe"):
            break
        pid = table.get(pid, (0, ""))[0]
    return watched


# ---------- lock file ----------


def lock_path(root: Path) -> Path:
    digest = hashlib.sha1(str(root).lower().encode("utf-8")).hexdigest()[:12]
    return Path(tempfile.gettempdir()) / f"wiki_viewer_{digest}.lock"


def running_server(root: Path) -> int | None:
    """Port of a live server for this folder, or None."""
    path = lock_path(root)
    try:
        info = json.loads(path.read_text(encoding="utf-8"))
        port, pid = int(info["port"]), int(info["pid"])
    except (OSError, ValueError, KeyError, TypeError):
        return None
    if process_identity(pid) is None:
        return None
    try:
        with urllib.request.urlopen(f"http://{HOST}:{port}{MARKER}", timeout=2) as reply:
            answer = json.loads(reply.read().decode("utf-8"))
        if answer.get("pid") == pid and Path(answer.get("root", "")) == root:
            return port
    except (OSError, ValueError):
        pass
    return None


# ---------- server ----------


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".md": "text/markdown; charset=utf-8",
        ".js": "text/javascript; charset=utf-8",
        ".html": "text/html; charset=utf-8",
        ".css": "text/css; charset=utf-8",
        ".svg": "image/svg+xml",
    }

    def do_GET(self) -> None:
        if self.path == MARKER:
            body = json.dumps({"pid": os.getpid(), "root": str(self.server.root)}).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        super().do_GET()

    def end_headers(self) -> None:
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def log_message(self, format: str, *args) -> None:  # noqa: A002 - base class name
        pass


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = False  # on Windows reuse would let two servers share a port

    def __init__(self, port: int, root: Path) -> None:
        self.root = root
        super().__init__((HOST, port), lambda *a, **k: Handler(*a, directory=str(root), **k))


def bind(root: Path, first_port: int) -> Server:
    last_error: OSError | None = None
    for port in range(first_port, first_port + PORT_TRIES):
        try:
            return Server(port, root)
        except OSError as err:
            last_error = err
    raise SystemExit(f"No free port between {first_port} and {first_port + PORT_TRIES - 1}: {last_error}")


def main() -> int:
    here = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description="Serve the wiki on 127.0.0.1 and open the browser.")
    parser.add_argument("--root", type=Path, default=here, help="folder to serve (default: this file's folder)")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="first port to try (default: 8765)")
    parser.add_argument("--parent-pid", type=int, default=os.getppid(), help="process whose end stops the server")
    parser.add_argument("--no-browser", action="store_true", help="do not open the browser")
    args = parser.parse_args()

    root = args.root.resolve()
    if not (root / "index.html").is_file():
        print(f"{root} has no index.html", file=sys.stderr)
        return 1

    port = running_server(root)
    if port is not None:
        url = f"http://{HOST}:{port}/"
        print(f"The wiki is already being served at {url}")
        if not args.no_browser:
            webbrowser.open(url)
        return 0

    watched = processes_to_watch(args.parent_pid)
    if not watched:
        print(f"Parent process {args.parent_pid} is not running", file=sys.stderr)
        return 1

    server = bind(root, args.port)
    port = server.server_address[1]
    lock = lock_path(root)
    lock.write_text(json.dumps({"port": port, "pid": os.getpid(), "root": str(root)}), encoding="utf-8")

    def remove_lock() -> None:
        try:
            if json.loads(lock.read_text(encoding="utf-8")).get("pid") == os.getpid():
                lock.unlink()
        except (OSError, ValueError):
            pass

    if IS_WINDOWS:
        # Console closed, logoff or shutdown: drop the lock before Windows ends the process.
        handler_type = ctypes.WINFUNCTYPE(wt.BOOL, wt.DWORD)

        def on_console_event(_event: int) -> bool:
            remove_lock()
            return False

        main.console_handler = handler_type(on_console_event)  # keep a reference alive
        _k32.SetConsoleCtrlHandler(main.console_handler, True)

    def watch() -> None:
        while True:
            time.sleep(WATCH_SECONDS)
            for pid, ident in watched:
                if process_identity(pid) != ident:
                    print(f"Process {pid} has ended; stopping the server.")
                    server.shutdown()
                    return

    threading.Thread(target=watch, name="parent-watch", daemon=True).start()

    url = f"http://{HOST}:{port}/"
    print(f"Wiki: {url}  (folder {root})")
    print("Close this window to stop the server.")
    sys.stdout.flush()
    if not args.no_browser:
        webbrowser.open(url)
    try:
        server.serve_forever(poll_interval=0.5)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        remove_lock()
    return 0


if __name__ == "__main__":
    sys.exit(main())
