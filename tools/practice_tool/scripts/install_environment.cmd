@echo off
setlocal

if not defined PRACTICE_INSTALL_MODE set "PRACTICE_INSTALL_MODE=auto"
if "%PRACTICE_INSTALL_MODE%"=="auto" if not "%PRACTICE_NONINTERACTIVE%"=="1" (
    echo [practice] Select the Node.js installation method:
    echo   [A] Automatic installation ^(default after 5 seconds^)
    echo   [M] Manually specify one offline package
    echo   [T] Read from the offline package table
    echo ----------------------------------------
    choice /C AMT /N /T 5 /D A /M "[practice] Enter A, M, or T: "
    if errorlevel 3 (
        set "PRACTICE_INSTALL_MODE=table"
    ) else if errorlevel 2 (
        set "PRACTICE_INSTALL_MODE=manual"
    )
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_node_windows.ps1" -PracticeDir "%~dp0.." -Mode "%PRACTICE_INSTALL_MODE%"
if not errorlevel 1 exit /b 0

where winget.exe >nul 2>nul
if errorlevel 1 (
    echo [practice] Official archive installation failed and winget is unavailable.
    exit /b 1
)

echo [practice] Trying Node.js LTS through winget as the final fallback...
winget.exe upgrade --id OpenJS.NodeJS.LTS --exact --silent --accept-package-agreements --accept-source-agreements >nul 2>nul
winget.exe install --id OpenJS.NodeJS.LTS --exact --silent --accept-package-agreements --accept-source-agreements >nul 2>nul
if exist "%ProgramFiles%\nodejs\node.exe" (
    for /f "delims=" %%V in ('"%ProgramFiles%\nodejs\node.exe" -p "Number(process.versions.node.split('.')[0])"') do (
        if %%V GEQ 18 exit /b 0
    )
)

echo [practice] Unable to install a compatible Node.js release.
exit /b 1
