[CmdletBinding()]
param(
    [string]$ProjectRoot = (Get-Location).Path
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path $ProjectRoot).Path
$PackageRoot = $PSScriptRoot
$Launcher = Join-Path $ProjectRoot "scripts\run-numerick.ps1"

if (-not (Test-Path $Launcher -PathType Leaf)) {
    throw "Could not find scripts\run-numerick.ps1 under $ProjectRoot. Pass -ProjectRoot with the NUMERICK repository root."
}

# Install only the new, isolated worker source and its build script.
$SourceWorker = Join-Path $PackageRoot "voice-native"
$TargetWorker = Join-Path $ProjectRoot "voice-native"
if (Test-Path $TargetWorker) {
    Copy-Item -Path (Join-Path $SourceWorker "*") -Destination $TargetWorker -Recurse -Force
}
else {
    Copy-Item -Path $SourceWorker -Destination $ProjectRoot -Recurse -Force
}

$BuildScriptSource = Join-Path $PackageRoot "scripts\build-native-voice-worker.ps1"
$BuildScriptTarget = Join-Path $ProjectRoot "scripts\build-native-voice-worker.ps1"
Copy-Item -Path $BuildScriptSource -Destination $BuildScriptTarget -Force

$contents = [System.IO.File]::ReadAllText($Launcher)
$lineEnding = if ($contents.Contains("`r`n")) { "`r`n" } else { "`n" }
$normalized = $contents.Replace("`r`n", "`n")

$alreadyInstalled = $normalized.Contains('$NativeVoiceService = Join-Path $ProjectRoot "build-native-voice-msvc\numerick-nvigi-voice-worker.exe"')
if (-not $alreadyInstalled) {
    $pattern = '(?m)# Rust voice-service executable built from this repository\.' + "`n" +
        '\$VoiceService\s*=\s*Join-Path\s+\$ProjectRoot\s+`' + "`n" +
        '\s*"voice-service\\target\\release\\numerick-voice-service\.exe"'

    $replacement = @'
# Prefer the standalone MSVC NVIGI worker. Keep Rust as the compatibility fallback.
$NativeVoiceService = Join-Path $ProjectRoot "build-native-voice-msvc\numerick-nvigi-voice-worker.exe"
$RustVoiceService = Join-Path $ProjectRoot `
    "voice-service\target\release\numerick-voice-service.exe"

if (Test-Path $NativeVoiceService -PathType Leaf) {
    $VoiceService = $NativeVoiceService
    $VoiceServiceLabel = "native NVIGI worker"
}
else {
    $VoiceService = $RustVoiceService
    $VoiceServiceLabel = "Rust voice service (fallback)"
}
'@
    $replacement = $replacement.TrimEnd("`r", "`n")
    $updated = [regex]::Replace($normalized, $pattern, [System.Text.RegularExpressions.MatchEvaluator]{ param($match) $replacement }, 1)
    if ($updated -eq $normalized) {
        throw "The launcher did not match the expected Rust worker block. Source files were copied, but the launcher was left unchanged. No automatic broad rewrite was attempted."
    }
    $normalized = $updated
}

$normalized = $normalized.Replace(
    'Write-Host "Speech output: NVIDIA Qwen3-TTS (custom voice)"',
    'Write-Host ("Speech output: NVIDIA Qwen3-TTS (" + $VoiceServiceLabel + ")")'
)
$normalized = $normalized.Replace(
    'Write-Warning "Custom voice assets or Rust service are unavailable."',
    'Write-Warning "Custom voice assets or configured voice worker are unavailable."'
)

if (-not (Test-Path ($Launcher + ".before-native-worker.bak") -PathType Leaf)) {
    Copy-Item -Path $Launcher -Destination ($Launcher + ".before-native-worker.bak")
}

$finalContents = $normalized.Replace("`n", $lineEnding)
[System.IO.File]::WriteAllText(
    $Launcher,
    $finalContents,
    [System.Text.UTF8Encoding]::new($false)
)

Write-Host "Installed native NVIGI worker source under: $TargetWorker"
Write-Host "Updated launcher: $Launcher"
Write-Host "Backup: $Launcher.before-native-worker.bak"
Write-Host ""
Write-Host "Next: build it with:"
Write-Host "  powershell -NoProfile -ExecutionPolicy Bypass -File `"$BuildScriptTarget`" -ProjectRoot `"$ProjectRoot`""
