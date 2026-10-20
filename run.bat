@echo off
cd /d "%~dp0"

if not exist server.exe (
  echo Building server.exe...
  g++ -std=c++17 -O2 -o server.exe backend/src/main.cpp -lws2_32
  if errorlevel 1 (
    echo Build failed. Make sure g++ is installed: g++ --version
    pause
    exit /b 1
  )
)

start "" server.exe
timeout /t 1 /nobreak >nul
start "" http://localhost:8080
