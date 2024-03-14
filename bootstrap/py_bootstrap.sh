#!/bin/bash

# Check if Python is installed
if ! command -v python3 &>/dev/null; then
    echo "Python is not installed"
    exit 1
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
VENV_DIR="$SCRIPT_DIR/venv"

set -e

if [ -d "$VENV_DIR" ]; then
    source "$VENV_DIR/bin/activate"
else
    echo "Creating virtualenv"
    if ! python3 -m venv "$VENV_DIR"; then
        echo "Creating virtualenv failed. Is venv installed?"
        exit 1
    fi
    source "$VENV_DIR/bin/activate"
    pip install -r "$SCRIPT_DIR/requirements.txt"
fi

python "$SCRIPT_DIR/main.py"
