@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0configure_windows.ps1" %*
set "configuration_exit_code=%errorlevel%"
if not "%configuration_exit_code%"=="0" (
    echo.
    echo Typora configuration failed. Review the error above; no backup is deleted.
) else (
    echo.
    echo Typora configuration completed. Keep the backup path shown above.
)
pause
exit /b %configuration_exit_code%
