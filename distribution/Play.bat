@echo off
setlocal
pushd "%~dp0" || exit /b 1
start "Last Light" /D "%~dp0" "%~dp0LastLight.exe" --resolution 960x540 --max-fps 30
set "result=%errorlevel%"
popd
exit /b %result%
