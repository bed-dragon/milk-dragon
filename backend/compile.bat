@echo off
cd /d %~dp0

echo ========================================
echo  Build backend
echo ========================================

:: -fexec-charset=GBK  解决中文乱码（UTF-8源码 -> GBK可执行文件）
g++ -std=c++17 ^
    -Isrc -Ilibs ^
    -fexec-charset=GBK ^
    src/main.cpp ^
    src/db.cpp ^
    libs/sqlite3.c ^
    -lws2_32 ^
    -o server.exe

if %errorlevel% equ 0 (
    echo.
    echo [OK] Build success! Starting server...
    echo.
    server.exe
) else (
    echo.
    echo [FAIL] Build failed. Check error messages above.
)
pause
