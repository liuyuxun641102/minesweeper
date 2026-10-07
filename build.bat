
pause

@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo === 编译 扫雷 ===

set "CXX="
set "MODE="

where g++ >nul 2>nul
if %errorlevel%==0 ( set "CXX=g++" & set "MODE=gcc" )

if not defined CXX (
    where clang++ >nul 2>nul
    if !errorlevel!==0 ( set "CXX=clang++" & set "MODE=gcc" )
)
if not defined CXX (
    where cl >nul 2>nul
    if !errorlevel!==0 ( set "CXX=cl" & set "MODE=msvc" )
)
if not defined CXX (
    echo 没找到 C++ 编译器。
    echo 装一个 MinGW-w64 或 Visual Studio Build Tools，把编译器加进 PATH 再运行本脚本。
    pause
    exit /b 1
)

echo 使用编译器: %CXX%
echo.

if "%MODE%"=="gcc" (
    echo [1/2] 图形版 ...
    %CXX% -std=c++17 -O2 -mwindows -o minesweeper_gui.exe minesweeper_gui.cpp -lgdi32 -luser32
    echo [2/2] 控制台版 ...
    %CXX% -std=c++17 -O2 -o minesweeper.exe minesweeper.cpp
) else (
    echo [1/2] 图形版 ...
    cl /nologo /std:c++17 /utf-8 /EHsc /O2 /DUNICODE /D_UNICODE /Fe:minesweeper_gui.exe minesweeper_gui.cpp user32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS
    echo [2/2] 控制台版 ...
    cl /nologo /std:c++17 /utf-8 /EHsc /O2 /Fe:minesweeper.exe minesweeper.cpp
)

echo.
if exist minesweeper_gui.exe (
    echo 编译成功！
) else (
    echo 编译失败，请看上面的报错。
    pause
    exit /b 1
)

echo.
echo   1 = 运行图形版 minesweeper_gui.exe
echo   2 = 运行控制台版 minesweeper.exe
echo   0 = 不运行，直接退出
echo.
choice /c 120 /n /m "选一个: "
if errorlevel 3 exit /b 0
if errorlevel 2 (
    minesweeper.exe
    exit /b 0
)
start "" minesweeper_gui.exe
endlocal
