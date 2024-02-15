#!/bin/bash

# Check if Python is installed
if ! command -v python3 &>/dev/null; then
    echo "Python is not installed"
    exit 1
fi

# Check if virtualenv is installed
if ! command -v virtualenv &>/dev/null; then
    echo "virtualenv is not installed"
    exit 1
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
VENV_DIR="$SCRIPT_DIR/venv"

if [ -d "$VENV_DIR" ]; then
    source "$VENV_DIR/bin/activate"
else
    echo "Creating virtualenv"
    python3 -m venv "$VENV_DIR"
    source "$VENV_DIR/bin/activate"
    pip install -r "$SCRIPT_DIR/requirements.txt"
fi

python "$SCRIPT_DIR/main.py"
