@echo off
setlocal
cd /d "%~dp0"
py -3 -m unittest discover -s tests -v
if errorlevel 1 exit /b 1
py -3 scripts\test_engine.py --engine dist\LastLight.exe --autoplay
exit /b %errorlevel%
