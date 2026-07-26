@echo off
setlocal

where winget.exe >nul 2>nul
if errorlevel 1 (
    echo [practice] Automatic Node.js installation requires winget.
    echo [practice] Install Node.js LTS from https://nodejs.org/ and run practice.cmd again.
    exit /b 1
)

echo [practice] Installing Node.js LTS with winget...
winget.exe install --id OpenJS.NodeJS.LTS --exact --accept-package-agreements --accept-source-agreements
if errorlevel 1 (
    echo [practice] Node.js installation failed.
    exit /b 1
)

echo [practice] Node.js installation completed.
exit /b 0
