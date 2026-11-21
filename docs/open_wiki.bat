@echo off
rem Opens the wiki in the default browser through a local server on 127.0.0.1.
rem The server stops when this window is closed. Launching again reuses it.
setlocal
title Wiki server
set "PY="
python -c "import sys; sys.exit(sys.version_info < (3, 8))" >nul 2>nul && set "PY=python"
if not defined PY (
    py -3 -c "import sys; sys.exit(sys.version_info < (3, 8))" >nul 2>nul && set "PY=py -3"
)
if not defined PY (
    echo Python 3.8 or newer is needed: https://www.python.org/downloads/
    pause
    exit /b 1
)
%PY% "%~dp0serve.py" %*
if errorlevel 1 pause
