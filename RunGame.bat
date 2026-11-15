@echo off
setlocal
cd /d "%~dp0"
if not exist dist\LastLight.exe (
 echo Build the project first using BuildGame.bat.
 exit /b 1
)
start "Last Light" /D "%~dp0dist" "%~dp0dist\LastLight.exe" %*
