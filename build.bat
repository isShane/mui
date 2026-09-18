@echo off
rem MUI build script (CMake). Keep this file ASCII-only for cmd.exe.
rem usage: build.bat       build all
rem        build.bat run   build and run simulator

setlocal
set CMAKE=%LOCALAPPDATA%\Packages\PythonSoftwareFoundation.Python.3.13_qbz5n2kfra8p0\LocalCache\local-packages\Python313\Scripts\cmake.exe

"%CMAKE%" -S . -B build\cmake -G "MinGW Makefiles" >nul
if errorlevel 1 (
    echo [ERROR] cmake configure failed
    exit /b 1
)
"%CMAKE%" --build build\cmake
if errorlevel 1 (
    echo [ERROR] build failed
    exit /b 1
)

if "%1"=="run" start "" build\mui_sim.exe
endlocal
