@echo off
setlocal
call "C:\VS2022BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [ERROR] vcvars64.bat tidak ditemukan atau gagal.
    exit /b 1
)

cd /d "%~dp0.."
"C:\VS2022BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ^
    --build build --config Release --parallel %NUMBER_OF_PROCESSORS%
exit /b %ERRORLEVEL%
