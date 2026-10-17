@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo vswhere not found. Install Visual Studio with the C++ workload.
  exit /b 1
)
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
if not defined VSINSTALL (
  echo No Visual Studio C++ installation found.
  exit /b 1
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "BUILD=%LOCALAPPDATA%\utool-build"
if not exist "%BUILD%\build.ninja" (
  echo No build tree at %BUILD%. Run cmake\build-release.cmd first.
  exit /b 1
)
cmake --build "%BUILD%" || exit /b 1
if not exist "%LOCALAPPDATA%\utool" mkdir "%LOCALAPPDATA%\utool"
copy /Y "%BUILD%\utool.exe" "%LOCALAPPDATA%\utool\utool.exe" >nul || exit /b 1
exit /b 0
