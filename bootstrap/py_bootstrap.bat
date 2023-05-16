@echo off

set SCRIPT_DIR=%~dp0
set VENV_DIR=%SCRIPT_DIR%venv

if exist %VENV_DIR% (
    call %VENV_DIR%\Scripts\activate
) else (
    echo Creating virtualenv
    python -m venv %VENV_DIR%
    call %VENV_DIR%\Scripts\activate
    pip install -r %SCRIPT_DIR%requirements.txt
)

python %SCRIPT_DIR%main.py
