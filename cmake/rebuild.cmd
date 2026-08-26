@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cmake --build F:\Data\personal\utool-build || exit /b 1
if not exist "%LOCALAPPDATA%\utool" mkdir "%LOCALAPPDATA%\utool"
copy /Y "F:\Data\personal\utool-build\utool.exe" "%LOCALAPPDATA%\utool\utool.exe" >nul
exit /b %ERRORLEVEL%
