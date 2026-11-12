$ErrorActionPreference = "Stop"
$PublicRoot = "D:\Projects\Leksika"
$UpdaterBackup = Join-Path $env:TEMP "Leksika-update_public.ps1"

Set-Location $PublicRoot

# Этот скрипт удаляет файлы публичной ветки перед синхронизацией.
# Сохраняем сам скрипт отдельно, чтобы он не исчез после git rm.
Copy-Item -LiteralPath $PSCommandPath -Destination $UpdaterBackup -Force

function Invoke-Git {
    $GitArgs = @($args)
    & git @GitArgs
    if ($LASTEXITCODE -ne 0) {
        throw ("Команда git {0} завершилась с кодом {1}" -f ($GitArgs -join " "), $LASTEXITCODE)
    }
}

try {
    Write-Host "1. Обновляем локальный публичный репозиторий..." -ForegroundColor Cyan
    Invoke-Git pull --ff-only origin main

    Write-Host "2. Очищаем файлы публичной ветки..." -ForegroundColor Cyan
    Invoke-Git rm -rf -- .

    Write-Host "3. Скачиваем актуальный код из Leksikadev..." -ForegroundColor Cyan
    if (Test-Path "temp_dev") {
        Remove-Item -Recurse -Force "temp_dev"
    }

    Invoke-Git clone --depth 1 --branch main --recurse-submodules https://github.com/zmlsaggg/Leksikadev temp_dev

    Write-Host "4. Переносим файлы, включая скрытые..." -ForegroundColor Cyan
    Remove-Item -Recurse -Force "temp_dev\.git"
    Get-ChildItem -Path "temp_dev" -Force | Copy-Item -Destination "." -Recurse -Force
    Remove-Item -Recurse -Force "temp_dev"

    Write-Host "5. Восстанавливаем настройки публичного репозитория..." -ForegroundColor Cyan
    Copy-Item -LiteralPath $UpdaterBackup -Destination (Join-Path $PublicRoot "update_public.ps1") -Force
    [System.IO.File]::WriteAllText(
        (Join-Path $PublicRoot "CNAME"),
        ("leksika.gleeze.com" + [Environment]::NewLine),
        [System.Text.Encoding]::ASCII
    )

    Write-Host "6. Отправляем обновления на GitHub..." -ForegroundColor Cyan
    Invoke-Git add -A

    & git diff --cached --quiet
    $DiffExitCode = $LASTEXITCODE
    if ($DiffExitCode -eq 1) {
        Invoke-Git commit -m "Обновление кодовой базы"
        Invoke-Git push origin main
    }
    elseif ($DiffExitCode -eq 0) {
        Write-Host "Изменений для отправки нет." -ForegroundColor Yellow
    }
    else {
        throw ("git diff завершился с кодом {0}" -f $DiffExitCode)
    }

    Write-Host "Готово! Код обновлён, CNAME и скрипт синхронизации сохранены." -ForegroundColor Green
}
finally {
    if (Test-Path $UpdaterBackup) {
        Remove-Item -Force $UpdaterBackup
    }
}

Pause
