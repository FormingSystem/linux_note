@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "PRACTICE_DIR=%~dp0"
set "PRACTICE_URL=http://127.0.0.1:5173/"
set "PRACTICE_READY=%PRACTICE_DIR%.local\environment-ready-v3-node-compatible"
set "PRACTICE_REQUIRED_NODE=18"
set "PRACTICE_UPGRADE=0"
set "PRACTICE_VITE_ARGS="

for %%A in (%*) do (
    if /I "%%~A"=="--upgrade" (
        set "PRACTICE_UPGRADE=1"
    ) else (
        set "PRACTICE_VITE_ARGS=!PRACTICE_VITE_ARGS! %%A"
    )
)

if not exist "%PRACTICE_DIR%package.json" (
    echo [practice] Cannot find package.json in %PRACTICE_DIR%
    exit /b 1
)

if "%PRACTICE_UPGRADE%"=="1" (
    echo [practice] Upgrade mode: refreshing the best compatible official Node.js and project dependencies...
    del /q "%PRACTICE_READY%" >nul 2>nul
    set "PRACTICE_FORCE_NODE_UPGRADE=1"
    call "%PRACTICE_DIR%scripts\install_environment.cmd"
    if errorlevel 1 exit /b 1
)

if exist "%PRACTICE_DIR%.local\runtime\node\npm.cmd" (
    set "PRACTICE_NPM=%PRACTICE_DIR%.local\runtime\node\npm.cmd"
    set "PRACTICE_NODE_DIR=%PRACTICE_DIR%.local\runtime\node"
)
if not defined PRACTICE_NPM (
    for /f "delims=" %%I in ('where npm.cmd') do if not defined PRACTICE_NPM (
        set "PRACTICE_NPM=%%I"
        set "PRACTICE_NODE_DIR=%%~dpI"
    )
)
if not defined PRACTICE_NPM if exist "E:\node_js\npm.cmd" (
    set "PRACTICE_NPM=E:\node_js\npm.cmd"
    set "PRACTICE_NODE_DIR=E:\node_js"
)
if not defined PRACTICE_NPM if exist "C:\msys64\ucrt64\bin\npm.cmd" (
    set "PRACTICE_NPM=C:\msys64\ucrt64\bin\npm.cmd"
    set "PRACTICE_NODE_DIR=C:\msys64\ucrt64\bin"
)
if not defined PRACTICE_NPM if exist "%ProgramFiles%\nodejs\npm.cmd" (
    set "PRACTICE_NPM=%ProgramFiles%\nodejs\npm.cmd"
    set "PRACTICE_NODE_DIR=%ProgramFiles%\nodejs"
)

if defined PRACTICE_NODE_DIR set "PATH=%PRACTICE_NODE_DIR%;%PATH%"
set "PRACTICE_NODE_SUPPORTED="
for /f "delims=" %%V in ('node -p "Number(process.versions.node.split('.')[0])" 2^>nul') do (
    if %%V GEQ %PRACTICE_REQUIRED_NODE% set "PRACTICE_NODE_SUPPORTED=1"
)
if not defined PRACTICE_NODE_SUPPORTED (
    for /f "delims=" %%V in ('node --version 2^>nul') do set "PRACTICE_CURRENT_NODE=%%V"
    if not defined PRACTICE_CURRENT_NODE set "PRACTICE_CURRENT_NODE=not installed"
    echo [practice] Current Node.js is !PRACTICE_CURRENT_NODE!; v%PRACTICE_REQUIRED_NODE% or newer is required.
    echo [practice] Preparing a compatible Node.js LTS environment...
    call "%PRACTICE_DIR%scripts\install_environment.cmd"
    if errorlevel 1 exit /b 1
    if exist "%PRACTICE_DIR%.local\runtime\node\npm.cmd" (
        set "PRACTICE_NPM=%PRACTICE_DIR%.local\runtime\node\npm.cmd"
        set "PRACTICE_NODE_DIR=%PRACTICE_DIR%.local\runtime\node"
        set "PATH=%PRACTICE_DIR%.local\runtime\node;%PATH%"
    ) else if exist "%ProgramFiles%\nodejs\npm.cmd" (
        set "PRACTICE_NPM=%ProgramFiles%\nodejs\npm.cmd"
        set "PRACTICE_NODE_DIR=%ProgramFiles%\nodejs"
        set "PATH=%ProgramFiles%\nodejs;%PATH%"
    ) else (
        echo [practice] Node.js was installed, but this window cannot locate npm yet.
        echo [practice] Close this window and run start.cmd again.
        exit /b 1
    )
)

set "PRACTICE_NODE_SUPPORTED="
for /f "delims=" %%V in ('node -p "Number(process.versions.node.split('.')[0])" 2^>nul') do (
    if %%V GEQ %PRACTICE_REQUIRED_NODE% set "PRACTICE_NODE_SUPPORTED=1"
)
if not defined PRACTICE_NODE_SUPPORTED (
    echo [practice] Node.js is still incompatible after environment setup.
    exit /b 1
)
if not defined PRACTICE_NPM (
    echo [practice] npm is unavailable after environment setup.
    exit /b 1
)

cd /d "%PRACTICE_DIR%"

if not exist "%PRACTICE_READY%" (
    echo [practice] First run: checking and installing project dependencies...
    echo [practice] The first download may be slow. The browser will open only after the service is ready.
    call "%PRACTICE_NPM%" install --no-audit --no-fund --fetch-retries=2 --fetch-timeout=120000
    if errorlevel 1 (
        echo [practice] Dependency installation failed.
        exit /b 1
    )
    if not exist ".local" mkdir ".local"
    >"%PRACTICE_READY%" echo environment-ready-v3-node-compatible
    echo [practice] Environment is ready. Later starts will skip this step.
)

if not exist "node_modules" (
    del /q "%PRACTICE_READY%" >nul 2>nul
    echo [practice] Dependencies were removed. Preparing them again...
    call "%PRACTICE_NPM%" install --no-audit --no-fund --fetch-retries=2 --fetch-timeout=120000
    if errorlevel 1 exit /b 1
    if not exist ".local" mkdir ".local"
    >"%PRACTICE_READY%" echo environment-ready-v3-node-compatible
)

echo [practice] Starting knowledge practice tool...
echo [practice] Browser address: %PRACTICE_URL%
set "PRACTICE_OPEN_ARG=--open"
if "%PRACTICE_NO_OPEN%"=="1" set "PRACTICE_OPEN_ARG="
call "%PRACTICE_NPM%" run dev -- --host 127.0.0.1 %PRACTICE_OPEN_ARG% %PRACTICE_VITE_ARGS%
set "PRACTICE_EXIT=%ERRORLEVEL%"
endlocal & exit /b %PRACTICE_EXIT%
