@echo off
setlocal
pushd "%~dp0" || exit /b 1
if exist "%~dp0frostworks-policy.llnp" (
    start "Last Light" /D "%~dp0" "%~dp0LastLight.exe" --resolution 960x540 --max-fps 30 -- --controller native --checkpoint "%~dp0frostworks-policy.llnp"
) else (
    start "Last Light" /D "%~dp0" "%~dp0LastLight.exe" --resolution 960x540 --max-fps 30
)
set "result=%errorlevel%"
popd
exit /b %result%
