$ErrorActionPreference = "Stop"
Set-Location "D:\Projects\Leksika"

Write-Host "1. Обновляем локальный публичный репозиторий..." -ForegroundColor Cyan
git pull origin main

Write-Host "2. Очищаем текущие файлы..." -ForegroundColor Cyan
git rm -rf .

Write-Host "3. Скачиваем актуальный код из Leksikadev..." -ForegroundColor Cyan
if (Test-Path "temp_dev") { Remove-Item -Recurse -Force "temp_dev" }
git clone --depth 1 --recurse-submodules https://github.com/zmlsaggg/Leksikadev temp_dev

Write-Host "4. Переносим файлы (включая скрытые)..." -ForegroundColor Cyan
Remove-Item -Recurse -Force "temp_dev\.git"
Get-ChildItem -Path "temp_dev" -Force | Copy-Item -Destination "." -Recurse -Force
Remove-Item -Recurse -Force "temp_dev"

Write-Host "5. Отправляем изменения на GitHub..." -ForegroundColor Cyan
git add .
git commit -m "Обновление кодовой базы"
git push origin main

Write-Host "Готово! Публичный репозиторий успешно обновлен." -ForegroundColor Green
Pause
