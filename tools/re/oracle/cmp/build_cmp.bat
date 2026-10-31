@echo off
rem Builds compare.exe (64-bit) next to this file, with openblack's own sources read from the openblack tree (nothing
rem is written there) and the flags of openblack's Release build (/O2 /Ob2, default /fp:precise).
rem   OPENBLACK_SRC  the openblack tree (default: this repository, four folders up)
rem   VCPKG_INC      vcpkg's include folder with glm, nlohmann-json and fmt (default: the tree's
rem                  cmake-build-presets\ninja-multi-vcpkg\vcpkg_installed\x64-windows\include, made by a normal build)
rem   VCVARS64       vcvars64.bat, if vswhere cannot find it
rem   CMP_BUILD_OUT  the folder for compare.exe and the objects instead of this one
setlocal
cd /d "%~dp0"
set "OUT=%~dp0"
if defined CMP_BUILD_OUT set "OUT=%CMP_BUILD_OUT%\"
if not defined OPENBLACK_SRC for %%i in ("%~dp0..\..\..\..") do set "OPENBLACK_SRC=%%~fi"
set "OB=%OPENBLACK_SRC%"
if not exist "%OB%\src\3D\ObjectMatrix.cpp" (echo no openblack sources under %OB%: set OPENBLACK_SRC & exit /b 1)
if not defined VCPKG_INC set "VCPKG_INC=%OB%\cmake-build-presets\ninja-multi-vcpkg\vcpkg_installed\x64-windows\include"
if not exist "%VCPKG_INC%\glm\glm.hpp" (echo glm not found under %VCPKG_INC%: build openblack once, or set VCPKG_INC & exit /b 1)
if not exist "%VCPKG_INC%\nlohmann\json.hpp" (echo nlohmann json not found under %VCPKG_INC% & exit /b 1)
where cl >nul 2>nul && goto :have_cl
if defined VCVARS64 goto :call_vcvars
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -products * -property installationPath`) do set "VCVARS64=%%i\VC\Auxiliary\Build\vcvars64.bat"
:call_vcvars
if not defined VCVARS64 goto :no_vcvars
if not exist "%VCVARS64%" goto :no_vcvars
rem vcvars64.bat itself runs vswhere by name
set "PATH=%PATH%;%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
call "%VCVARS64%" >nul || exit /b 1
:have_cl
if not exist "%OUT%build" mkdir "%OUT%build"
cl /nologo /O2 /Ob2 /DNDEBUG /EHsc /std:c++latest /MD /W3 /permissive- /Zc:__cplusplus /utf-8 /Zc:preprocessor ^
   /DGLM_FORCE_LEFT_HANDED /DGLM_FORCE_RADIANS /DGLM_ENABLE_EXPERIMENTAL /DNOMINMAX /DWIN32_LEAN_AND_MEAN ^
   /D_CRT_SECURE_NO_WARNINGS /I"%OB%\src" /external:I"%VCPKG_INC%" /external:W0 /Fo"%OUT%build\\" /Fd"%OUT%build\\" ^
   /Fe"%OUT%compare.exe" compare.cpp "%OB%\src\3D\ObjectMatrix.cpp" "%OB%\src\3D\AffineMatrix.cpp" ^
   "%OB%\src\Common\GUtilsAngle.cpp" "%OB%\src\Common\GUtilsDistance.cpp" "%OB%\src\Common\ModelInverseSquareRoot.cpp" ^
   "%OB%\src\Common\GameRandom.cpp" "%OB%\src\ECS\DanceShapes.cpp" ^
   /link /LIBPATH:"%VCPKG_INC%\..\lib" fmt.lib || exit /b 1
rem GameRandom.cpp logs through spdlog, so fmt's DLL sits beside compare.exe
copy /y "%VCPKG_INC%\..\bin\fmt.dll" "%OUT%" >nul
echo built %OUT%compare.exe
exit /b 0
:no_vcvars
echo vcvars64.bat not found: install the Visual Studio C++ tools, or set VCVARS64 to its path
exit /b 1
