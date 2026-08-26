@echo off
REM NMake breaks on '#' in path (c#). Use Ninja + build dir without '#'.
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
set "NINJA=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "SRC=%~dp0.."
set "BUILD=F:\Data\personal\utool-build"
set "INSTALL=%LOCALAPPDATA%\utool"
if not exist "%BUILD%" mkdir "%BUILD%"
cmake -S "%SRC%" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="%NINJA%" || exit /b 1
cmake --build "%BUILD%" || exit /b 1
if not exist "%INSTALL%" mkdir "%INSTALL%"
copy /Y "%BUILD%\utool.exe" "%INSTALL%\utool.exe" >nul || exit /b 1
echo.
echo Built:  %BUILD%\utool.exe
echo Install: %INSTALL%\utool.exe
echo Add to PATH: %INSTALL%
