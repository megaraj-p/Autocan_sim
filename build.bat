@echo off
g++ -std=c++11 -Wall -Wextra -Wpedantic -Iinclude src\*.cpp -pthread -o autocan.exe
if errorlevel 1 exit /b %errorlevel%
echo Build successful: autocan.exe
