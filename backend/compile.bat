@echo off
cd /d %~dp0

echo ========================================
echo  Build backend
echo ========================================

:: 第1步：单独编译 sqlite3.c（C代码，用gcc）
echo [1/2] Compiling sqlite3...
gcc -c libs/sqlite3.c -o sqlite3.o
if %errorlevel% neq 0 (
    echo [FAIL] sqlite3 compile error
    pause
    exit /b 1
)

:: 第2步：编译 C++ 源码并链接
echo [2/2] Compiling C++ and linking...
g++ -std=c++17 ^
    -Isrc -Ilibs ^
    -fexec-charset=GBK ^
    src/main.cpp ^
    src/db.cpp ^
    sqlite3.o ^
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
