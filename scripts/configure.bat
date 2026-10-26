@echo off
setlocal
call "C:\VS2022BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [ERROR] vcvars64.bat tidak ditemukan atau gagal.
    exit /b 1
)

cd /d "%~dp0.."
if not exist "build" mkdir build

"C:\VS2022BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ^
    -S . -B build -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DRRLIVE_BUILD_TESTS=ON
exit /b %ERRORLEVEL%
