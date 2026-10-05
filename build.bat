@echo off
rem MUI build script for CMake. Keep this file ASCII-only for cmd.exe.
rem usage: build.bat        build all
rem         build.bat run    build and run simulator
rem
rem Toolchain lookup order:
rem   1) cmake already on PATH
rem   2) %MUI_SDK%\cmake\bin\cmake.exe  (MUI_SDK is optional; default is a guess)
rem   3) pip-installed cmake resolved through python (cmake.CMAKE_BIN_DIR)
rem %MUI_SDK%\mingw64\bin is prepended only when it exists, so a bundled MinGW-w64 gcc wins.

setlocal

if not defined MUI_SDK set "MUI_SDK=E:\WorkSpace\SDK"
if exist "%MUI_SDK%\mingw64\bin" set "PATH=%MUI_SDK%\mingw64\bin;%PATH%"

set "CMAKE="
for /f "delims=" %%i in ('where cmake 2^>nul') do if not defined CMAKE set "CMAKE=%%i"
if not defined CMAKE if exist "%MUI_SDK%\cmake\bin\cmake.exe" set "CMAKE=%MUI_SDK%\cmake\bin\cmake.exe"
for /f "delims=" %%i in ('python -c "import cmake;print(cmake.CMAKE_BIN_DIR)" 2^>nul') do if not defined CMAKE set "CMAKE=%%i\cmake.exe"
if defined CMAKE if not exist "%CMAKE%" set "CMAKE="
if not defined CMAKE (
    echo [ERROR] cmake not found. Put cmake on PATH, set MUI_SDK, or install it with: python -m pip install cmake
    exit /b 1
)

where gcc >nul 2>nul
if errorlevel 1 (
    echo [ERROR] gcc not found. Put MinGW-w64 bin on PATH, or fix MUI_SDK in this script.
    exit /b 1
)

rem Output artifacts and compile_commands.json to build dir; clangd auto-detects build\compile_commands.json
rem Configure output is NOT suppressed: a configure failure must stay visible.
"%CMAKE%" -S . -B build -G "MinGW Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
if errorlevel 1 (
    echo [ERROR] cmake configure failed
    exit /b 1
)
"%CMAKE%" --build build
if errorlevel 1 (
    echo [ERROR] build failed
    exit /b 1
)

if "%1"=="run" start "" build\mui_sim.exe
endlocal
