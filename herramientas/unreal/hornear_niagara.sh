#!/usr/bin/env bash
# Igual que hornear_niagara.bat, desde Git Bash en Windows. Uso: bash hornear_niagara.sh [NS_Fire ... texturas]
python -I "$(dirname "$0")/hornear_todo.py" "$@"
