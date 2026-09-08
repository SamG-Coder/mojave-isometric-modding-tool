@echo off
if exist "%~dp0Launch Tool.local.cmd" (
  call "%~dp0Launch Tool.local.cmd"
  exit /b
)
if exist "%~dp0.venv\Scripts\pythonw.exe" (
  start "Mojave Isometric Lab" "%~dp0.venv\Scripts\pythonw.exe" "%~dp0desktop\tool.py"
  exit /b
)
py -3 "%~dp0desktop\tool.py"
if errorlevel 1 pause
