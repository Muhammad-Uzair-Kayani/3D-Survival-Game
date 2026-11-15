@echo off
setlocal
pushd "%~dp0" || exit /b 1
if not exist premake5.exe (
    where premake5.exe >nul 2>&1
    if errorlevel 1 (
        echo Put your working premake5.exe beside GenerateProjects.bat or add it to PATH.
        popd
        exit /b 1
    )
)
if "%~1"=="" (
    premake5.exe vs2022
) else (
    premake5.exe %*
)
set "result=%errorlevel%"
popd
exit /b %result%
