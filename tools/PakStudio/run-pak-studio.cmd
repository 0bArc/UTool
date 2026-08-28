@echo off
setlocal
cd /d "%~dp0"

if not defined UTOOL_EXE (
  if exist "%~dp0..\..\dist\utool\utool.exe" set "UTOOL_EXE=%~dp0..\..\dist\utool\utool.exe"
)
if not defined UTOOL_EXE (
  if exist "%~dp0..\..\..\..\utool-build\utool.exe" set "UTOOL_EXE=%~dp0..\..\..\..\utool-build\utool.exe"
)

node scripts\open-studio.mjs
