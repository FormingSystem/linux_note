@echo off
setlocal
set "LOOP_START_SCRIPT=%~dp0apps\desktop\scripts\start_desktop.ps1"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%LOOP_START_SCRIPT%" %*
set "LOOP_EXIT_CODE=%ERRORLEVEL%"

if not "%LOOP_EXIT_CODE%"=="0" (
  echo.
  echo Loop desktop startup failed. See the message above.
  pause
)

exit /b %LOOP_EXIT_CODE%
