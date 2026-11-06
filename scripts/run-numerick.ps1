
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

# NVIDIA Qwen3-TTS and private voice assets.
$VoiceLab = Join-Path $env:USERPROFILE "NumerickVoiceLab"
$TtsPack = Join-Path $VoiceLab "NVIDIA-Qwen3-TTS\_qwen_tts_pack"
$TtsRuntime = Join-Path $TtsPack "bin\x64\Release"

$TtsExecutable = Join-Path $TtsRuntime "nvigi.tts.qwen.exe"
$TtsModels = Join-Path $TtsPack "data\nvigi.models"
$SpeakerProfile = Join-Path $VoiceLab "reference.json"
$TtsOutput = Join-Path $VoiceLab "generated"

# Rust voice-service executable built from this repository.
$VoiceService = Join-Path $ProjectRoot `
    "voice-service\target\release\numerick-voice-service.exe"

# Validate the core runtime setup.
if (-not (Test-Path $UcrtBin -PathType Container)) {
    throw "MSYS2 UCRT64 was not found at $UcrtBin."
}

if (-not (Test-Path $Executable -PathType Leaf)) {
    throw "NUMERICK executable not found. Build the Release configuration first."
}

if (-not (Test-Path $Model -PathType Leaf)) {
    throw "Whisper model not found at $Model."
}

if (-not (Test-Path $CpuBackend -PathType Leaf)) {
    throw "GGML CPU backend not found at $CpuBackend."
}

# Check whether every custom voice component is available.
$VoiceReady = (
    (Test-Path $VoiceService -PathType Leaf) -and
    (Test-Path $TtsExecutable -PathType Leaf) -and
    (Test-Path $TtsModels -PathType Container) -and
    (Test-Path $SpeakerProfile -PathType Leaf) -and
    (Test-Path $TtsRuntime -PathType Container) -and
    (Test-Path (Join-Path $TtsRuntime "nvigi.core.framework.dll") -PathType Leaf) -and
    (Test-Path (Join-Path $TtsRuntime "nvigi.plugin.hwi.cuda.dll") -PathType Leaf) -and
    (Test-Path (Join-Path $TtsRuntime "nvigi.plugin.tts.qwen3.cuda.dll") -PathType Leaf)
)

# Configure local Whisper.
$env:GGML_BACKEND_PATH = $CpuBackend
$env:NUMERICK_WHISPER_MODEL = $Model
$env:NUMERICK_WHISPER_LANGUAGE = "en"

# Configure NVIDIA voice generation when all private assets are present.
if ($VoiceReady) {
    New-Item -ItemType Directory -Force -Path $TtsOutput |
        Out-Null

    $env:NUMERICK_VOICE_SERVICE_EXE = $VoiceService
    $env:NUMERICK_TTS_EXE = $TtsExecutable
    $env:NUMERICK_TTS_MODELS = $TtsModels
    $env:NUMERICK_TTS_SPEAKER = $SpeakerProfile
    $env:NUMERICK_TTS_OUTPUT_DIR = $TtsOutput
    $env:NUMERICK_TTS_BACKEND = "cuda"
}
else {
    # Remove stale settings so C++ uses its SAPI fallback.
    $env:NUMERICK_VOICE_SERVICE_EXE = $null
    $env:NUMERICK_TTS_EXE = $null
    $env:NUMERICK_TTS_MODELS = $null
    $env:NUMERICK_TTS_SPEAKER = $null
    $env:NUMERICK_TTS_OUTPUT_DIR = $null
    $env:NUMERICK_TTS_BACKEND = $null
}

# Add runtime directories without duplicating existing PATH entries.
$PathEntries = @()

if ($env:PATH) {
    $PathEntries = $env:PATH -split ';'
}

$PathEntries = @(
    $PathEntries | Where-Object {
        $_ -and
        $_ -ne $UcrtBin -and
        (-not $VoiceReady -or $_ -ne $TtsRuntime)
    }
)

if ($VoiceReady) {
    $env:PATH = (@($TtsRuntime, $UcrtBin) + $PathEntries) -join ';'
}
else {
    $env:PATH = (@($UcrtBin) + $PathEntries) -join ';'
}

Write-Host ""
Write-Host "========================================"
Write-Host "   Starting NUMERICK Voice Assistant"
Write-Host "========================================"
Write-Host "Version:       1.2.0"
Write-Host "Speech input:  Whisper (local)"

if ($VoiceReady) {
    Write-Host "Speech output: NVIDIA Qwen3-TTS (custom voice)"
    Write-Host "Fallback:      Windows SAPI"
    Write-Host "TTS backend:   CUDA"
}
else {
    Write-Host "Speech output: Windows SAPI"
    Write-Warning "Custom voice assets or Rust service are unavailable."
}

Write-Host "Backend:       GGML CPU (Whisper)"
Write-Host ""
Write-Host "Type 'voice' to speak to NUMERICK."
Write-Host ""

# Run NUMERICK in the current terminal.
& $Executable

if ($LASTEXITCODE -ne 0) {
    Write-Warning "NUMERICK exited with code $LASTEXITCODE."
}
