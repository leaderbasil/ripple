@echo off
REM Quick MSVC build without CMake (Windows).
cd /d "%~dp0.."
if not exist build mkdir build
set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if exist %VCVARS% call %VCVARS% x64 >nul 2>&1
cl.exe /nologo /EHsc /O2 /std:c++17 /W4 /utf-8 /Iinclude /Fobuild\ /Fe:build\ripple.exe src\*.cpp
if errorlevel 1 (
    echo.
    echo Build failed. If cl.exe is not found, open a "Developer Command Prompt
    echo for VS 2022" and run this script again, or use CMake.
    exit /b 1
)
echo.
echo Built .\build\ripple.exe
