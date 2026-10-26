@echo off
setlocal
cd /d "%~dp0.."
"build\RrLiveStudioTests_artefacts\Release\RrLiveStudioTests.exe"
exit /b %ERRORLEVEL%
