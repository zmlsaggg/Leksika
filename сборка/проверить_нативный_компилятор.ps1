$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
& chcp.com 65001 | Out-Null
[Console]::InputEncoding = [System.Text.UTF8Encoding]::new()
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()

$Root = Split-Path -Parent $PSScriptRoot
$Kernel = Join-Path $Root "ядро"

$GccCommand = Get-Command gcc.exe -ErrorAction SilentlyContinue
if ($GccCommand) {
    $Gcc = $GccCommand.Source
} else {
    $Candidates = @(
        "C:\msys64\ucrt64\bin\gcc.exe",
        "C:\msys64\mingw64\bin\gcc.exe",
        "D:\msys64\ucrt64\bin\gcc.exe",
        "D:\msys64\mingw64\bin\gcc.exe"
    )
    $Gcc = $Candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Gcc) {
    throw "Не найден gcc. Установите MSYS2 UCRT64 и пакет mingw-w64-ucrt-x86_64-gcc. Скрипт не требует make."
}

function Invoke-Gcc {
    param([string[]]$Arguments)
    & $script:Gcc @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "GCC завершился с кодом $LASTEXITCODE."
    }
}

function Invoke-Test {
    param([string]$Path, [string[]]$Arguments = @())
    Write-Host "Проверка: $Path"
    & $Path @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Проверка завершилась с кодом $LASTEXITCODE: $Path"
    }
}

$Common = @("-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-finput-charset=UTF-8", "-fexec-charset=UTF-8", "-I", $Kernel)
$Native = Join-Path $Kernel "лексус-ядро.exe"
$ValuesTest = Join-Path $Kernel "значения-тест.exe"
$DictionaryTest = Join-Path $Kernel "словарь-тест.exe"
$VmTest = Join-Path $Kernel "машина-тест.exe"
$CompilerTest = Join-Path $Kernel "компилятор-тест.exe"

Write-Host "Компилятор: $Gcc"
Write-Host "Сборка нативного ядра..."

$NativeSources = @(
    (Join-Path $Kernel "лексус.c"),
    (Join-Path $Kernel "значения.c"),
    (Join-Path $Kernel "словарь_индекс.c"),
    (Join-Path $Kernel "байткод.c"),
    (Join-Path $Kernel "машина.c"),
    (Join-Path $Kernel "компилятор.c")
)
Invoke-Gcc ($Common + @("-o", $Native) + $NativeSources + @("-lshell32"))

Write-Host "Сборка проверок..."
Invoke-Gcc ($Common + @("-o", $ValuesTest,
    (Join-Path $Kernel "значения.c"),
    (Join-Path $Kernel "значения_проверка.c")))
Invoke-Gcc ($Common + @("-o", $DictionaryTest,
    (Join-Path $Kernel "словарь_индекс.c"),
    (Join-Path $Kernel "словарь_индекс_проверка.c")))
Invoke-Gcc ($Common + @("-o", $VmTest,
    (Join-Path $Kernel "значения.c"),
    (Join-Path $Kernel "байткод.c"),
    (Join-Path $Kernel "машина.c"),
    (Join-Path $Kernel "машина_проверка.c")))
Invoke-Gcc ($Common + @("-o", $CompilerTest,
    (Join-Path $Kernel "значения.c"),
    (Join-Path $Kernel "байткод.c"),
    (Join-Path $Kernel "машина.c"),
    (Join-Path $Kernel "компилятор.c"),
    (Join-Path $Kernel "компилятор_проверка.c")))

Invoke-Test -Path $Native -Arguments @("--self-test")
Invoke-Test -Path $ValuesTest
Invoke-Test -Path $DictionaryTest
Invoke-Test -Path $VmTest
Invoke-Test -Path $CompilerTest

Write-Host "Выполнение примера .lek"
$Example = Join-Path $Root "примеры\нативная_vm.lek"
& $Native "выполнить" $Example
if ($LASTEXITCODE -ne 0) {
    throw "Нативный запуск примера завершился с кодом $LASTEXITCODE."
}

Write-Host "Готово: нативная сборка, проверки и пример завершились успешно."
