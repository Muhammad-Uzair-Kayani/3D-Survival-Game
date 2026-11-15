@echo off
setlocal
pushd "%~dp0" || exit /b 1
start "Last Light - Python AI" /D "%~dp0" "%~dp0LastLight.exe" --resolution 960x540 --max-fps 30 -- --python "%~dp0python-runtime\python.exe"
set "result=%errorlevel%"
popd
exit /b %result%
