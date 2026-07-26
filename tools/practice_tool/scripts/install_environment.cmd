@echo off
setlocal

where winget.exe >nul 2>nul
if not errorlevel 1 (
    echo [practice] Trying the newest Node.js LTS through winget...
    winget.exe upgrade --id OpenJS.NodeJS.LTS --exact --silent --accept-package-agreements --accept-source-agreements >nul 2>nul
    winget.exe install --id OpenJS.NodeJS.LTS --exact --silent --accept-package-agreements --accept-source-agreements >nul 2>nul
    if exist "%ProgramFiles%\nodejs\node.exe" (
        for /f "delims=" %%V in ('"%ProgramFiles%\nodejs\node.exe" -p "Number(process.versions.node.split('.')[0])"') do (
            if %%V GEQ 18 exit /b 0
        )
    )
)

echo [practice] winget did not provide a compatible runtime. Trying official Node.js archives...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_node_windows.ps1" -PracticeDir "%~dp0.."
if errorlevel 1 (
    echo [practice] Unable to install a compatible Node.js release.
    exit /b 1
)

exit /b 0
