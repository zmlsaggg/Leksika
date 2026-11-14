@echo off
setlocal
set "LEK_ROOT=%~dp0"
if "%~1"=="--нативно" goto native
set "PYTHONUTF8="
set "PYTHONIOENCODING=utf-8"

if exist "%LEK_ROOT%питон\python.exe" goto embedded

where py >nul 2>nul
if errorlevel 1 goto system_python
py -3 -X utf8 -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 8) else 1)" >nul 2>nul
if errorlevel 1 goto system_python
py -3 -X utf8 "%LEK_ROOT%лексус.lek" %*
exit /b %ERRORLEVEL%

:system_python
where python >nul 2>nul
if errorlevel 1 goto missing_python
python -X utf8 -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 8) else 1)" >nul 2>nul
if errorlevel 1 goto missing_python
python -X utf8 "%LEK_ROOT%лексус.lek" %*
exit /b %ERRORLEVEL%

:embedded
"%LEK_ROOT%питон\python.exe" -X utf8 "%LEK_ROOT%лексус.lek" %*
exit /b %ERRORLEVEL%

:missing_python
echo Лексика: Python 3.8+ не найден. Запустите установщик или установите Python 3.8+.
exit /b 127

:native
if exist "%LEK_ROOT%лексус-ядро.exe" goto native_installed
if exist "%LEK_ROOT%ядро\лексус-ядро.exe" goto native_source
echo Лексика: нативное ядро не найдено. 1>&2
exit /b 127

:native_installed
"%LEK_ROOT%лексус-ядро.exe" %*
exit /b %ERRORLEVEL%

:native_source
"%LEK_ROOT%ядро\лексус-ядро.exe" %*
exit /b %ERRORLEVEL%
