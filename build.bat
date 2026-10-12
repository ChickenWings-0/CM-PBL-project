@echo off
REM Build the Smart Energy Consumption Prediction & Analysis System.
REM Needs GCC (MinGW) on the PATH. Run this from the project folder.
gcc -std=c99 -Wall -Wextra -O2 src\main.c src\data_io.c src\load_model.c src\interpolation.c src\integration.c src\root_finding.c src\analysis.c src\report.c src\tests.c src\web_export.c -o energy.exe -lm
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)
echo Build OK: energy.exe
