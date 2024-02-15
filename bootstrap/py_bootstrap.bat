@echo off

REM Check if Python is installed
where python > nul 2>&1
if %errorlevel% neq 0 (
    echo Python is not installed
    exit /b 1
)

REM Check if venv module is available
python -m venv --help > nul 2>&1
if %errorlevel% neq 0 (
    echo venv module is not available
    exit /b 1
)

set "SCRIPT_DIR=%~dp0"
set "VENV_DIR=%SCRIPT_DIR%venv"

if exist "%VENV_DIR%" (
    call "%VENV_DIR%\Scripts\activate"
) else (
    echo Creating virtualenv
    python -m venv "%VENV_DIR%"
    call "%VENV_DIR%\Scripts\activate"
    pip install -r "%SCRIPT_DIR%requirements.txt"
)

python "%SCRIPT_DIR%main.py"
