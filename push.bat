@echo off
setlocal
cd /d "%~dp0" || exit /b 1

git status || exit /b 1
set /p "continue=Press Enter to continue, or Ctrl+C to cancel: "

set "branch="
for /f "delims=" %%B in ('git branch --show-current') do set "branch=%%B"
if not "%branch%"=="main" (
    echo Switch to main before running this script.
    exit /b 1
)

powershell.exe -NoProfile -Command "$message = Read-Host 'Commit message'; if ([string]::IsNullOrWhiteSpace($message)) { Write-Host 'Commit message cannot be empty.'; exit 1 }; $messageFile = [IO.Path]::GetTempFileName(); try { [IO.File]::WriteAllText($messageFile, $message, [Text.UTF8Encoding]::new($false)); git add -A; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; git commit -F $messageFile; exit $LASTEXITCODE } finally { Remove-Item -LiteralPath $messageFile -Force }"
if errorlevel 1 exit /b 1

git push origin main
exit /b %errorlevel%
