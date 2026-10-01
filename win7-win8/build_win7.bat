@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo === 编译 Win7 / Win8 版扫雷 ===
echo.

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
    echo [MinGW / clang] 目标 Windows 7, 静态链接运行时 ...
    %CXX% -std=c++17 -O2 -mwindows -static -static-libgcc -static-libstdc++ ^
        -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 ^
        -o minesweeper_gui_win7.exe minesweeper_gui_win7.cpp -lgdi32 -luser32
) else (
    echo [MSVC] 目标 Windows 7, /MT 静态链接 UCRT ...
    cl /nologo /std:c++17 /utf-8 /EHsc /MT /O2 /DUNICODE /D_UNICODE ^
       /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 ^
       /Fe:minesweeper_gui_win7.exe minesweeper_gui_win7.cpp user32.lib gdi32.lib ^
       /link /SUBSYSTEM:WINDOWS
)

echo.
if not exist minesweeper_gui_win7.exe (
    echo 编译失败，请看上面的报错。
    pause
    exit /b 1
)

echo 编译成功: minesweeper_gui_win7.exe
echo.
echo 关于老系统能不能直接跑，看 exe 依赖了哪些 DLL:
echo   * 用 MSVC 的 /MT 编译: UCRT 静态链进去，裸的 Win7 SP1 也能直接跑，无需补丁。
echo   * 用 UCRT 版 MinGW 编译: 仍会依赖 api-ms-win-crt-*.dll，Win7/8.1 上需要
echo     先装 KB2999226 (通用 C 运行时更新)，或者改用 msvcrt 版的 MinGW-w64 重编。
echo   用 objdump -p minesweeper_gui_win7.exe ^| findstr /i "DLL Name" 可以自己确认。
echo.
pause
endlocal
