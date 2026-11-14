param(
    [Parameter(Mandatory=$true)][string]$Installer,
    [Parameter(Mandatory=$true)][string]$Checksums
)
$ErrorActionPreference = 'Stop'
$Installer = (Resolve-Path $Installer).Path
$expected = ((Get-Content -LiteralPath $Checksums -Raw).Trim() -split '\s+')[0]
$actual = (Get-FileHash -LiteralPath $Installer -Algorithm SHA256).Hash
if ($actual -ine $expected) { throw 'SHA-256 установщика не совпадает' }
$version = (Get-Item -LiteralPath $Installer).VersionInfo
if ($version.OriginalFilename -ne 'leksika.exe' -or $version.ProductName -ne 'Leksika' -or
    !$version.FileDescription -or !$version.CompanyName -or !$version.LegalCopyright) {
    throw 'В установщике отсутствуют ожидаемые метаданные версии'
}
$oldLocalAppData = $env:LOCALAPPDATA
$env:LOCALAPPDATA = Join-Path $env:RUNNER_TEMP ('leksika-smoke-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $env:LOCALAPPDATA | Out-Null
$installDir = Join-Path $env:LOCALAPPDATA 'Лексус'

function Run-Installer([string]$Argument) {
    $process = Start-Process -FilePath $Installer -ArgumentList $Argument -PassThru
    if (!$process.WaitForExit(240000)) {
        $process.Kill()
        throw "Установщик завис при $Argument"
    }
    if ($process.ExitCode -ne 0) { throw "Установщик ($Argument) вернул $($process.ExitCode)" }
}
try {
    Run-Installer '--тихо'
    $launcher = Join-Path $installDir 'Leksika.cmd'
    $native = Join-Path $installDir 'лексус-ядро.exe'
    $example = Join-Path $installDir 'примеры/нативная_vm.lek'
    foreach ($path in @($launcher, $native, $example, (Join-Path $installDir 'лексика.cmd'))) {
        if (!(Test-Path -LiteralPath $path)) { throw "Не установлен $path" }
    }
    & $launcher 'проверить' $example
    if ($LASTEXITCODE -ne 0) { throw "Проверка исходника не прошла: $LASTEXITCODE" }
    & $launcher '--нативно' 'запустить' $example
    if ($LASTEXITCODE -ne 0) { throw "Нативный запуск не прошёл: $LASTEXITCODE" }
    $profileProgram = Join-Path $env:LOCALAPPDATA 'profile.lek'
    [System.IO.File]::WriteAllText($profileProgram, 'вкинуть 42.', [System.Text.UTF8Encoding]::new($false))
    & $launcher '--нативно' 'запустить' $profileProgram
    if ($LASTEXITCODE -eq 0) { throw 'Профиль зумер активен без разрешающего флага' }
    $profileOutput = & $launcher '--нативно' 'запустить' $profileProgram '--профиль=зумер'
    if ($LASTEXITCODE -ne 0 -or ($profileOutput -join "`n") -notmatch '(^|\s)42($|\s)') {
        throw 'Не прошёл запуск с профилем зумер'
    }
    Run-Installer '--удалить'
    if (Test-Path -LiteralPath $installDir) { throw 'Каталог установки не удалён' }
} catch {
    $message = $_.Exception.Message -replace "`r|`n", ' '
    Write-Host "::error::Windows smoke test: $message"
    throw
} finally {
    $env:LOCALAPPDATA = $oldLocalAppData
}
