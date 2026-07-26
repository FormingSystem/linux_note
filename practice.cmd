@echo off
setlocal

set "PRACTICE_START=%~dp0tools\practice_tool\start.cmd"
if not defined PRACTICE_SOURCE_CONFIG set "PRACTICE_SOURCE_CONFIG=%~dp0practice.sources.json"
if not exist "%PRACTICE_START%" (
    echo [practice] Cannot find tools\practice_tool\start.cmd
    exit /b 1
)

call "%PRACTICE_START%" %*
set "PRACTICE_EXIT=%ERRORLEVEL%"
endlocal & exit /b %PRACTICE_EXIT%
