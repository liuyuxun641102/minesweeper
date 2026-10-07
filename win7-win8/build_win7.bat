@echo off
chcp 65001 >nul

rem ============================================================
rem  本脚本已改为调用 build_win7_msvcrt.bat
rem
rem  原因: 用 UCRT 版工具链编出来的 exe 仍然依赖 api-ms-win-crt-*.dll,
rem        裸的 Win7 / 8.1 上跑不起来 —— 名字叫 Win7 版, 其实不是。
rem        只有 MSVCRT 版工具链编出来的才干净。
rem
rem  要编 Win7 / Win8 能用的版本, 双击 build_win7_msvcrt.bat。
rem ============================================================

echo 提示: 本脚本已改为调用 build_win7_msvcrt.bat
echo        (用 MSVCRT 工具链, 编出来的 exe 老系统才能跑)
echo.

call "%~dp0build_win7_msvcrt.bat"