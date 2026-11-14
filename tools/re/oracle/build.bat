@echo off
rem Builds oracle_host.exe (32-bit) next to this file. Linked at the fixed base 0x60000000 without ASLR so that the
rem exe's range 0x00400000..+SizeOfImage stays free for runblack.exe's image. Static CRT: no redistributable needed.
rem Needs Visual Studio 2019 or 2022 (or its Build Tools) with the C++ workload. Run it from a plain prompt (it finds
rem vcvars32.bat through vswhere, or through VCVARS32 if you set it), or from an "x86 Native Tools" prompt.
rem ORACLE_BUILD_OUT, if set, is the folder for the exe and the objects instead of this one.
setlocal
cd /d "%~dp0"
set "OUT=%~dp0"
if defined ORACLE_BUILD_OUT set "OUT=%ORACLE_BUILD_OUT%\"
where cl >nul 2>nul && goto :have_cl
if defined VCVARS32 goto :call_vcvars
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -products * -property installationPath`) do set "VCVARS32=%%i\VC\Auxiliary\Build\vcvars32.bat"
:call_vcvars
if not defined VCVARS32 goto :no_vcvars
if not exist "%VCVARS32%" goto :no_vcvars
rem vcvars32.bat itself runs vswhere by name
set "PATH=%PATH%;%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
call "%VCVARS32%" >nul || exit /b 1
:have_cl
if not exist "%OUT%build" mkdir "%OUT%build"
cl /nologo /std:c++17 /O2 /EHsc /W4 /MT /D_CRT_SECURE_NO_WARNINGS /Fo"%OUT%build\\" /Fd"%OUT%build\\" /Fe"%OUT%oracle_host.exe" oracle_host.cpp ^
   /link /BASE:0x60000000 /FIXED /DYNAMICBASE:NO || exit /b 1
echo built %OUT%oracle_host.exe
exit /b 0
:no_vcvars
echo vcvars32.bat not found: install the Visual Studio C++ tools, or set VCVARS32 to its path
exit /b 1
