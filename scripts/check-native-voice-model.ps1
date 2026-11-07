[CmdletBinding()]
param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$VoiceLab = Join-Path $env:USERPROFILE "NumerickVoiceLab"
$TtsPack = Join-Path $VoiceLab "NVIDIA-Qwen3-TTS\_qwen_tts_pack"
$TtsRuntime = Join-Path $TtsPack "bin\x64\Release"
$Worker = Join-Path $ProjectRoot "build-native-voice-msvc\numerick-nvigi-voice-worker.exe"

$env:NUMERICK_TTS_EXE = Join-Path $TtsRuntime "nvigi.tts.qwen.exe"
$env:NUMERICK_TTS_MODELS = Join-Path $TtsPack "data\nvigi.models"
$env:NUMERICK_TTS_SPEAKER = Join-Path $VoiceLab "reference.json"
$env:NUMERICK_TTS_OUTPUT_DIR = Join-Path $VoiceLab "generated"
$env:NUMERICK_TTS_BACKEND = "cuda"

foreach ($Path in @($Worker, $env:NUMERICK_TTS_EXE, $env:NUMERICK_TTS_SPEAKER)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file not found: $Path"
    }
}
if (-not (Test-Path -LiteralPath $env:NUMERICK_TTS_MODELS -PathType Container)) {
    throw "Qwen model repository not found: $($env:NUMERICK_TTS_MODELS)"
}

New-Item -ItemType Directory -Force -Path $env:NUMERICK_TTS_OUTPUT_DIR | Out-Null

Write-Host "Checking installed Qwen model selection..."
& $Worker --check-model
if ($LASTEXITCODE -ne 0) {
    throw "Native worker model check failed with exit code $LASTEXITCODE."
}
