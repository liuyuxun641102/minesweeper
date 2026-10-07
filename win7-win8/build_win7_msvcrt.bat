@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

rem ============================================================
rem  用 MSVCRT 版工具链编译 Win7 / Win8 能用的扫雷
rem  这样编出来的 exe 不依赖 UCRT，裸的 Win7 SP1 / Win8.1 也能跑
rem
rem  工具链换位置了？用记事本改下面这一行 MINGW= 就行
rem ============================================================

set "MINGW=C:\mingw64-16.2.0-msvcrt"

if not exist "%MINGW%\bin\g++.exe" (
    echo [错误] 没找到 "%MINGW%\bin\g++.exe"
    echo.
    echo 请用记事本打开本文件，把 MINGW= 那一行改成工具链的实际位置。
    echo.
    pause
    exit /b 1
)

echo ==============================================
echo   编译 Win7 / Win8 版扫雷（MSVCRT 工具链）
echo ==============================================
echo.
echo 工具链: %MINGW%
echo 注意: 只在本次窗口内临时生效，不会修改你的系统环境变量。
echo.

rem 只在这一个黑窗口里，把 msvcrt 工具链排到最前面
set "PATH=%MINGW%\bin;%PATH%"

echo 实际调用的编译器:
g++ --version | findstr /i "g++"
echo.

g++ --version | findstr /i "msvcrt" >nul
if errorlevel 1 (
    echo [警告] 版本行里没有 "msvcrt" 字样，可能拿错包了。
    echo.
)

echo 开始编译 ...
echo.

g++ -std=c++17 -O2 -mwindows -static -static-libgcc -static-libstdc++ ^
    -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 ^
    -o minesweeper_gui_win7.exe minesweeper_gui_win7.cpp -lgdi32 -luser32

if not exist minesweeper_gui_win7.exe (
    echo.
    echo [失败] 没有产出 exe，请看上面的报错。
    echo.
    pause
    exit /b 1
)

echo.
echo [成功] minesweeper_gui_win7.exe 已生成
echo.
echo ----------------------------------------------
echo  它依赖的 DLL:
echo ----------------------------------------------
"%MINGW%\bin\objdump.exe" -p minesweeper_gui_win7.exe | findstr /i "DLL Name"
echo.
echo ----------------------------------------------
echo  验收标准:
echo    只应该出现 KERNEL32 / USER32 / GDI32 / msvcrt
echo    如果出现任何 api-ms-win-crt-* 说明还是 UCRT, 没成功
echo ----------------------------------------------
echo.

choice /c YN /n /m "现在就运行看看? (Y/N) "
if errorlevel 2 goto :end
start "" minesweeper_gui_win7.exe

:end
echo.
pause
endlocal
