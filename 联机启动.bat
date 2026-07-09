@echo off
cd /d %~dp0
start "Server" cmd /c "cd /d backend && server.exe"
timeout /t 3 /nobreak >nul
start "ngrok" cmd /k "ngrok http 8080"
timeout /t 4 /nobreak >nul
start http://127.0.0.1:4040
