@echo off
rem Builds the C++ server (first time only), starts it, and opens the page.
cd /d "%~dp0"

if not exist server.exe (
  echo Building server.exe ...
  g++ -std=c++17 -O2 -o server.exe backend/src/main.cpp -lws2_32
  if errorlevel 1 (
    echo.
    echo Build failed. Check that g++ is installed: run  g++ --version
    pause
    exit /b 1
  )
)

start "Traffic simulator server - close this window to stop" server.exe
timeout /t 1 /nobreak >nul
start "" http://localhost:8080
