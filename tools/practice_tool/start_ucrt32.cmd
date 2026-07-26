@echo off
setlocal

if not defined PRACTICE_MSYS2_ROOT set "PRACTICE_MSYS2_ROOT=C:\msys64"
if not exist "%PRACTICE_MSYS2_ROOT%\ucrt32" (
    echo [practice] No UCRT32 environment was found under %PRACTICE_MSYS2_ROOT%.
    echo [practice] Official current MSYS2 installers provide UCRT64, not UCRT32.
    exit /b 1
)

set "MSYSTEM=UCRT32"
set "CHERE_INVOKING=1"
set "MSYS2_PATH_TYPE=inherit"
set "PRACTICE_WINDOWS_DIR=%~dp0"
"%PRACTICE_MSYS2_ROOT%\usr\bin\bash.exe" --login -c "cd \"$(cygpath -u \"$PRACTICE_WINDOWS_DIR\")\" && exec ./start.sh %*"
set "PRACTICE_EXIT=%ERRORLEVEL%"
endlocal & exit /b %PRACTICE_EXIT%
