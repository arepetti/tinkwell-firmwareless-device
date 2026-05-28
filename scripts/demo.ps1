<#
.SYNOPSIS
    Run the complete Tinkwell Firmwareless device demo.
.DESCRIPTION
    Builds the thermostat example (POSIX/WSL or native), starts the hub,
    runs the device, and shows heartbeats arriving.
#>

$ErrorActionPreference = "Stop"

$DeviceDir = Split-Path -Parent $PSScriptRoot
$ThermostatRoot = Join-Path $DeviceDir "examples\thermostat"
$ThermostatBuild = Join-Path $ThermostatRoot "build"
$ThermostatBin = Join-Path $ThermostatBuild "thermostat.exe"
if (-not (Test-Path $ThermostatBin)) {
    $ThermostatBin = Join-Path $ThermostatBuild "thermostat"
}

function Write-DemoInfo([string]$Message) {
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Write-DemoOk([string]$Message) {
    Write-Host "ok: $Message" -ForegroundColor Green
}

function Write-DemoWarn([string]$Message) {
    Write-Host "warn: $Message" -ForegroundColor Yellow
}

function Stop-DemoProcesses {
    param([System.Diagnostics.Process]$HubProc, [System.Diagnostics.Process]$ThermoProc)
    if ($null -ne $ThermoProc -and -not $ThermoProc.HasExited) {
        try { Stop-Process -Id $ThermoProc.Id -Force -ErrorAction SilentlyContinue } catch {}
    }
    if ($null -ne $HubProc -and -not $HubProc.HasExited) {
        try { Stop-Process -Id $HubProc.Id -Force -ErrorAction SilentlyContinue } catch {}
    }
}

function Send-CoapProbe {
    & tw coap send get /tw/info
    if ($LASTEXITCODE -ne 0) {
        Write-DemoWarn "CoAP probe returned non-zero (device may still be fine)"
    }
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake not found in PATH"
}
if (-not (Get-Command tw -ErrorAction SilentlyContinue)) {
    throw "'tw' CLI not found in PATH (needed for firmwareless-hub)"
}

$hubProc = $null
$thermoProc = $null

try {
    Write-DemoInfo "building thermostat (PAL_BACKEND=posix)..."
    $cmakeArgs = @(
        "-B", $ThermostatBuild
        "-S", $ThermostatRoot
        "-DPAL_BACKEND=posix"
    )
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    & cmake --build $ThermostatBuild
    if ($LASTEXITCODE -ne 0) { throw "cmake --build failed" }

    if (-not (Test-Path -LiteralPath $ThermostatBin)) {
        throw "thermostat binary not found: $ThermostatBuild\thermostat(.exe)"
    }
    Write-DemoOk "built $ThermostatBin"

    Write-DemoInfo "starting firmwareless-hub..."
    $hubProc = Start-Process -FilePath "tw" -ArgumentList @("firmwareless-hub", "start") -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds 2
    $hubProc.Refresh()
    if ($hubProc.HasExited) {
        throw "hub process exited immediately (pid $($hubProc.Id)); check 'tw firmwareless-hub start'"
    }

    Write-DemoInfo "starting thermostat device..."
    $thermoProc = Start-Process -FilePath $ThermostatBin -WorkingDirectory $ThermostatBuild -PassThru -WindowStyle Hidden

    Write-DemoInfo "waiting 3s for first heartbeat..."
    Start-Sleep -Seconds 3
    $thermoProc.Refresh()
    if ($thermoProc.HasExited) {
        throw "thermostat exited during startup (pid $($thermoProc.Id))"
    }

    Write-DemoInfo "sending test query (GET /tw/info)..."
    Send-CoapProbe

    Write-Host ""
    Write-Host "Demo running. Press Ctrl-C to stop." -ForegroundColor Green
    Write-Host ""

    while ($true) {
        $hubProc.Refresh()
        $thermoProc.Refresh()
        if ($hubProc.HasExited -or $thermoProc.HasExited) {
            break
        }
        Start-Sleep -Seconds 60
    }
}
finally {
    Stop-DemoProcesses -HubProc $hubProc -ThermoProc $thermoProc
}
