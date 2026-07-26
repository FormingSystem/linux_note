@echo off
setlocal

if not defined PRACTICE_MSYS2_ROOT set "PRACTICE_MSYS2_ROOT=C:\msys64"
if not exist "%PRACTICE_MSYS2_ROOT%\usr\bin\bash.exe" (
    echo [practice] MSYS2 was not found at %PRACTICE_MSYS2_ROOT%.
    echo [practice] Run bootstrap_windows.ps1 from PowerShell first.
    exit /b 1
)

set "MSYSTEM=UCRT64"
set "CHERE_INVOKING=1"
set "MSYS2_PATH_TYPE=inherit"
set "PRACTICE_WINDOWS_DIR=%~dp0"
"%PRACTICE_MSYS2_ROOT%\usr\bin\bash.exe" --login -c "cd \"$(cygpath -u \"$PRACTICE_WINDOWS_DIR\")\" && exec ./start.sh %*"
set "PRACTICE_EXIT=%ERRORLEVEL%"
endlocal & exit /b %PRACTICE_EXIT%
