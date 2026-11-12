@echo off
setlocal
set "LEK_ROOT=%~dp0"
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"

if exist "%LEK_ROOT%питон\python.exe" goto embedded

where py >nul 2>nul
if errorlevel 1 goto system_python
py -3 -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 8) else 1)" >nul 2>nul
if errorlevel 1 goto system_python
py -3 "%LEK_ROOT%лексус.lek" %*
exit /b %ERRORLEVEL%

:system_python
where python >nul 2>nul
if errorlevel 1 goto missing_python
python -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 8) else 1)" >nul 2>nul
if errorlevel 1 goto missing_python
python "%LEK_ROOT%лексус.lek" %*
exit /b %ERRORLEVEL%

:embedded
"%LEK_ROOT%питон\python.exe" "%LEK_ROOT%лексус.lek" %*
exit /b %ERRORLEVEL%

:missing_python
echo Лексика: Python 3.8+ не найден. Запустите установщик или установите Python 3.8+.
exit /b 127
