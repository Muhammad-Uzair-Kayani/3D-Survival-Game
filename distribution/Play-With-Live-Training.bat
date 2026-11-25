@echo off
setlocal
pushd "%~dp0" || exit /b 1
if not exist "frostworks-policy.llnp" (
    echo This package does not contain a Frostworks checkpoint.
    popd
    exit /b 1
)
"python-runtime\python.exe" "python\frostworks_train.py" live --engine "LastLight.exe" --project "LastLight.pck" --model "frostworks-policy.llnp" --output "%LOCALAPPDATA%\Last Light\policy-training" --rounds 0 --play
set "result=%errorlevel%"
popd
exit /b %result%
