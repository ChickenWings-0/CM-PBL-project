#!/usr/bin/env bash
# Build the Smart Energy Consumption Prediction & Analysis System on Linux/Mac.
# Needs GCC on the PATH. Run this from the project folder.
set -e
gcc -std=c99 -Wall -Wextra -O2 src/main.c src/data_io.c src/load_model.c src/interpolation.c src/integration.c src/root_finding.c src/analysis.c src/report.c src/tests.c src/web_export.c -o energy -lm
chmod +x energy
echo "Build OK: energy"
