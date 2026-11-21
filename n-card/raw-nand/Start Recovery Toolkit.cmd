@echo off
where py >nul 2>nul
if errorlevel 1 goto use_python
py -3 "%~dp0recovery_tool.py"
if errorlevel 1 pause
exit /b

:use_python
python "%~dp0recovery_tool.py"
if errorlevel 1 pause
