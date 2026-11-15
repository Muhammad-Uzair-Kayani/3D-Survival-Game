@echo off
setlocal
pushd "%~dp0" || exit /b 1
if /I not "%~1"=="--vcvars" goto :direct
if "%~2"=="" (
    echo Visual Studio did not provide the C++ tools script path.
    set "result=1"
    goto :finished
)
if not exist "%~2" (
    echo C++ tools script was not found: "%~2"
    set "result=1"
    goto :finished
)
rem Initialize in this process. Python inherits the actual environment; the
rem Visual Studio build path never needs to capture or parse cmd.exe output.
call "%~2"
if errorlevel 1 (
    set "result=1"
    goto :finished
)
call BuildGame.bat --from-ide %*
set "result=%errorlevel%"
goto :finished
:direct
call BuildGame.bat %*
set "result=%errorlevel%"
:finished
popd
exit /b %result%
