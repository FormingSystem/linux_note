@echo off
setlocal

set "PRACTICE_DIR=%~dp0"
set "PRACTICE_URL=http://127.0.0.1:5173/"
set "PRACTICE_READY=%PRACTICE_DIR%.local\environment-ready-v1"

if not exist "%PRACTICE_DIR%package.json" (
    echo [practice] Cannot find package.json in %PRACTICE_DIR%
    exit /b 1
)

where npm.cmd >nul 2>nul
if %errorlevel% equ 0 (
    for /f "delims=" %%I in ('where npm.cmd') do if not defined PRACTICE_NPM set "PRACTICE_NPM=%%I"
) else if exist "E:\node_js\npm.cmd" (
    set "PRACTICE_NPM=E:\node_js\npm.cmd"
    set "PRACTICE_NODE_DIR=E:\node_js"
) else if exist "C:\msys64\ucrt64\bin\npm.cmd" (
    set "PRACTICE_NPM=C:\msys64\ucrt64\bin\npm.cmd"
    set "PRACTICE_NODE_DIR=C:\msys64\ucrt64\bin"
) else if exist "%ProgramFiles%\nodejs\npm.cmd" (
    set "PRACTICE_NPM=%ProgramFiles%\nodejs\npm.cmd"
    set "PRACTICE_NODE_DIR=%ProgramFiles%\nodejs"
) else (
    echo [practice] First run: preparing Node.js...
    call "%PRACTICE_DIR%scripts\install_environment.cmd"
    if errorlevel 1 exit /b 1
    if exist "%ProgramFiles%\nodejs\npm.cmd" (
        set "PRACTICE_NPM=%ProgramFiles%\nodejs\npm.cmd"
        set "PRACTICE_NODE_DIR=%ProgramFiles%\nodejs"
    ) else (
        echo [practice] Node.js was installed, but this window cannot locate npm yet.
        echo [practice] Close this window and run start.cmd again.
        exit /b 1
    )
)

if defined PRACTICE_NODE_DIR set "PATH=%PRACTICE_NODE_DIR%;%PATH%"

cd /d "%PRACTICE_DIR%"

if not exist "%PRACTICE_READY%" (
    echo [practice] First run: checking and installing project dependencies...
    call "%PRACTICE_NPM%" install
    if errorlevel 1 (
        echo [practice] Dependency installation failed.
        exit /b 1
    )
    if not exist ".local" mkdir ".local"
    >"%PRACTICE_READY%" echo environment-ready-v1
    echo [practice] Environment is ready. Later starts will skip this step.
)

if not exist "node_modules" (
    del /q "%PRACTICE_READY%" >nul 2>nul
    echo [practice] Dependencies were removed. Preparing them again...
    call "%PRACTICE_NPM%" install
    if errorlevel 1 exit /b 1
    if not exist ".local" mkdir ".local"
    >"%PRACTICE_READY%" echo environment-ready-v1
)

echo [practice] Starting knowledge practice tool...
echo [practice] Browser address: %PRACTICE_URL%
if not "%PRACTICE_NO_OPEN%"=="1" (
    start "" /b powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Seconds 2; Start-Process '%PRACTICE_URL%'"
)
call "%PRACTICE_NPM%" run dev -- --host 127.0.0.1 %*

endlocal
