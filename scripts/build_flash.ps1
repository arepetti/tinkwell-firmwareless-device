#
# build_flash.ps1 -- Build and flash main + optional factory app (Windows).
#
# Usage:
#   .\scripts\build_flash.ps1 -Example thermostat [-Factory] [-Port COM3] [-Target esp32c3]
#
# SPDX-License-Identifier: MIT

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$Example,

    [switch]$Factory,

    [string]$Port = "COM3",

    [string]$Target = "esp32c3"
)

$ErrorActionPreference = "Stop"

$DeviceDir  = Split-Path -Parent $PSScriptRoot
$FactoryDir = Join-Path $DeviceDir "factory"
$ExamplesDir = Join-Path $DeviceDir "examples"
$ExampleDir = Join-Path $ExamplesDir "$Example\esp-idf"

if (-not (Test-Path $ExampleDir)) {
    Write-Error "Example not found: $ExampleDir"
    exit 1
}

$env:IDF_TARGET = $Target

Write-Host "=== TW Device Build ==="
Write-Host "  Example : $Example"
Write-Host "  Target  : $Target"
Write-Host "  Port    : $Port"
Write-Host "  Factory : $($Factory -eq $true)"
Write-Host ""

if ($Factory) {
    Write-Host "--- Building factory app ---"
    Push-Location $FactoryDir
    idf.py set-target $Target
    idf.py build
    Pop-Location

    $FactoryBin = Join-Path $FactoryDir "build\tw_factory.bin"
    $FactoryAddr = "0x20000"

    Write-Host "`n--- Building main app ($Example) ---"
    Push-Location $ExampleDir

    Copy-Item (Join-Path $DeviceDir "partitions_factory.csv") "partitions.csv"
    Add-Content "sdkconfig.defaults" "CONFIG_PARTITION_TABLE_CUSTOM=y"
    Add-Content "sdkconfig.defaults" 'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"'

    idf.py set-target $Target
    idf.py build

    $MainBin = Join-Path $ExampleDir "build\$Example.bin"
    $MainAddr = "0x60000"

    Write-Host "`n--- Merging binaries ---"
    $Merged = Join-Path $ExampleDir "build\merged.bin"
    $Bootloader = Join-Path $ExampleDir "build\bootloader\bootloader.bin"
    $PartTable  = Join-Path $ExampleDir "build\partition_table\partition-table.bin"

    esptool.py --chip $Target merge_bin `
        --output $Merged `
        --flash_mode dio --flash_size 4MB `
        0x0000 $Bootloader `
        0x8000 $PartTable `
        $FactoryAddr $FactoryBin `
        $MainAddr $MainBin

    Write-Host "`n--- Flashing merged image ---"
    esptool.py --chip $Target --port $Port write_flash 0x0 $Merged

    Remove-Item "partitions.csv" -ErrorAction SilentlyContinue
    Pop-Location
}
else {
    Write-Host "--- Building main app ($Example) ---"
    Push-Location $ExampleDir
    idf.py set-target $Target
    idf.py build

    Write-Host "`n--- Flashing ---"
    idf.py --port $Port flash
    Pop-Location
}

Write-Host "`n=== Done ==="
