@echo off
setlocal
cd /d "%~dp0"
if not exist .tools\venv\Scripts\python.exe (
 py -3 scripts\setup.py
 if errorlevel 1 exit /b 1
)
".tools\venv\Scripts\python.exe" scripts\build.py %*
exit /b %errorlevel%
