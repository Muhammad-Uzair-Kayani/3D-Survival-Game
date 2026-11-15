@echo off
setlocal
cd /d "%~dp0"
py -3 scripts\setup.py %*
exit /b %errorlevel%
