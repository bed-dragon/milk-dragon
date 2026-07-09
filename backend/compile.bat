@echo off
chcp 65001 >nul
cd /d %~dp0

echo ========================================
echo  Build backend
echo ========================================

taskkill /F /IM server.exe >nul 2>&1
if not exist data mkdir data

echo [1/2] Compiling sqlite3...
gcc -c libs/sqlite3.c -o sqlite3.o
if %errorlevel% neq 0 (
    echo [FAIL] sqlite3 compile error
    pause
    exit /b 1
)

echo [2/2] Compiling C++ and linking...
g++ -std=c++17 ^
    -Isrc -Ilibs ^
    src/main.cpp ^
    src/db.cpp ^
    src/routes_tasks.cpp ^
    src/routes_auth.cpp ^
    src/routes_material.cpp ^
    src/routes_reminder.cpp ^
    src/routes_checkin.cpp ^
    src/routes_pomodoro.cpp ^
    src/routes_social.cpp ^
    sqlite3.o ^
    -lws2_32 ^
    -o server.exe

if %errorlevel% equ 0 (
    echo [OK] Build success! Starting server...
    server.exe
) else (
    echo [FAIL] Build failed.
)
pause
