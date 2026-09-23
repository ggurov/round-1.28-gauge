<#
.SYNOPSIS
    Flash the round gauge without touching the BOOT button.

.DESCRIPTION
    The Waveshare ESP32-S3-LCD-1.28 cannot be reset into the ROM bootloader
    over USB - the CH343P's DTR/RTS lines are not wired to EN/GPIO0. So this
    script asks the *running firmware* to do it instead:

      1. open the serial port and type the `bootloader` console command
      2. the app sets RTC_CNTL_FORCE_DOWNLOAD_BOOT and resets itself
      3. wait for the chip to come back up in download mode
      4. run `idf.py -p <port> flash monitor`

    After flashing you may still need to press RESET once: esptool has no
    working reset line either, so it cannot start the new image for you.

.EXAMPLE
    tools\flash.ps1
    tools\flash.ps1 -Port COM7
    tools\flash.ps1 -NoMonitor
#>
[CmdletBinding()]
param(
    [string] $Port = 'COM6',
    [int]    $Baud = 115200,
    [switch] $NoMonitor,
    [int]    $BootTimeoutSec = 10
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

function Write-Step($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }

# --- 1. tell the app to drop into the ROM bootloader -------------------------
Write-Step "asking the running firmware to enter download mode on $Port"
try {
    $sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
    $sp.DtrEnable = $false
    $sp.RtsEnable = $false
    $sp.ReadTimeout = 500
    $sp.WriteTimeout = 1000
    $sp.Open()
    Start-Sleep -Milliseconds 150
    $sp.DiscardInBuffer()
    $sp.Write("bootloader`r`n")
    $sp.Flush()
    Start-Sleep -Milliseconds 700
    try {
        $reply = $sp.ReadExisting()
        if ($reply) { Write-Host ($reply.Trim()) -ForegroundColor DarkGray }
    } catch { }
    $sp.Close()
} catch {
    Write-Warning "could not talk to $Port ($($_.Exception.Message))."
    Write-Warning "If the board is already in download mode that is fine - continuing."
}

# --- 2. wait for the ROM bootloader to answer --------------------------------
Write-Step "waiting for the ROM bootloader (up to $BootTimeoutSec s)"
$deadline = (Get-Date).AddSeconds($BootTimeoutSec)
$ready = $false
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 400
    & python -m esptool --port $Port --before no-reset --after no-reset `
        -- connect-attempts 1 read-mac 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) { $ready = $true; break }
}
if ($ready) {
    Write-Step 'ROM bootloader is up'
} else {
    Write-Warning 'no answer from the ROM bootloader.'
    Write-Warning 'Press BOOT + RESET on the board, then re-run this script.'
}

# --- 3. flash -----------------------------------------------------------------
$idfArgs = @('-p', $Port, 'flash')
if (-not $NoMonitor) { $idfArgs += 'monitor' }

Write-Step "idf.py $($idfArgs -join ' ')"
& (Join-Path $PSScriptRoot 'idf.bat') @idfArgs

if (-not $NoMonitor) {
    Write-Host ''
    Write-Host 'If the app does not start: press RESET once.' -ForegroundColor Yellow
}
