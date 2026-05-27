@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1"
set _ec=%errorlevel%
echo.
pause
endlocal ^& exit /b %_ec%
