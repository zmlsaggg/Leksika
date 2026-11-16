@echo off
rem Обновление публичной ветки из основного репозитория.
rem Сам скрипт лежит в инструменты\публикация, чтобы корень не разрастался.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0инструменты\публикация\update_public.ps1"
