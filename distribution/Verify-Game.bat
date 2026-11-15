@echo off
setlocal
pushd "%~dp0" || exit /b 1
"%~dp0python-runtime\python.exe" "%~dp0Verify-Game.py" %*
set "result=%errorlevel%"
echo.
if "%result%"=="0" (
 echo All game checks passed. Run Play.bat to test the graphics and controls.
) else (
 echo Verification failed. See the diagnostics folder for details.
)
popd
pause
exit /b %result%
