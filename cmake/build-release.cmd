@echo off
setlocal
REM NMake breaks when the repo path contains '#'. Build outside the repo.
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
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
set "NINJA=%VSINSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not exist "%NINJA%" (
  where ninja >nul 2>&1
  if errorlevel 1 (
    echo Ninja not found next to Visual Studio or on PATH.
    exit /b 1
  )
  set "NINJA=ninja"
)
set "SRC=%~dp0.."
set "BUILD=%LOCALAPPDATA%\utool-build"
set "INSTALL=%LOCALAPPDATA%\utool"
if not exist "%BUILD%" mkdir "%BUILD%"
cmake -S "%SRC%" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="%NINJA%" || exit /b 1
cmake --build "%BUILD%" || exit /b 1
if not exist "%INSTALL%" mkdir "%INSTALL%"
copy /Y "%BUILD%\utool.exe" "%INSTALL%\utool.exe" >nul || exit /b 1
echo.
echo Built:   %BUILD%\utool.exe
echo Install: %INSTALL%\utool.exe
echo Add to PATH: %INSTALL%
