@echo off
cmake -S "%~dp0" -B "%~dp0build" -A Win32
if errorlevel 1 exit /b %errorlevel%
cmake --build "%~dp0build" --config Release
