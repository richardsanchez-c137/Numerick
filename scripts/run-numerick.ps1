[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

# Resolve the project root from this script's location.
$ProjectRoot = Split-Path -Parent $PSScriptRoot

# NUMERICK executable and local Whisper model.
$Executable = Join-Path $ProjectRoot "build-release\numerick.exe"
$Model = Join-Path $ProjectRoot "models\ggml-base.en.bin"

# MSYS2 UCRT64 runtime dependencies.
$UcrtBin = "C:\msys64\ucrt64\bin"
$CpuBackend = Join-Path $UcrtBin "ggml-cpu-zen4.dll"

# Validate the runtime setup before starting the application.
if (-not (Test-Path $UcrtBin -PathType Container)) {
    throw "MSYS2 UCRT64 was not found at $UcrtBin."
}

if (-not (Test-Path $Executable -PathType Leaf)) {
    throw "NUMERICK executable not found. Run scripts\verify-release.ps1 -Clean first."
}

if (-not (Test-Path $Model -PathType Leaf)) {
    throw "Whisper model not found at $Model. Download the base.en model first."
}

if (-not (Test-Path $CpuBackend -PathType Leaf)) {
    throw "GGML CPU backend not found at $CpuBackend."
}

# Configure the child process without permanently changing Windows settings.
if (-not $env:PATH.StartsWith(
    "$UcrtBin;",
    [System.StringComparison]::OrdinalIgnoreCase
)) {
    $env:PATH = "$UcrtBin;$env:PATH"
}

$env:GGML_BACKEND_PATH = $CpuBackend
$env:NUMERICK_WHISPER_MODEL = $Model
$env:NUMERICK_WHISPER_LANGUAGE = "en"

Write-Host ""
Write-Host "========================================"
Write-Host "   Starting NUMERICK Voice Assistant"
Write-Host "========================================"
Write-Host "Version:       1.2.0"
Write-Host "Speech engine: Whisper (local)"
Write-Host "Speech output: Windows SAPI"
Write-Host "Backend:       GGML CPU"
Write-Host ""
Write-Host "Type 'voice' to speak to NUMERICK."
Write-Host ""

# Run NUMERICK in the current terminal.
& $Executable

if ($LASTEXITCODE -ne 0) {
    Write-Warning "NUMERICK exited with code $LASTEXITCODE."
}