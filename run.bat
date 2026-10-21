@echo off
cd /d "%~dp0"

if not exist server.exe g++ -std=c++17 -O2 -o server.exe backend/src/main.cpp -lws2_32
if not exist dp_server.exe g++ -std=c++17 -O2 -o dp_server.exe backend/src/dp_server.cpp -lws2_32

start "Greedy :8080" server.exe 8080
start "DP :8081" dp_server.exe 8081
timeout /t 1 /nobreak >nul
start "" http://localhost:8080
