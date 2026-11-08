[CmdletBinding()]
param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path $ProjectRoot).Path
$SourceDir = Join-Path $ProjectRoot "voice-native"
$BuildDir = Join-Path $ProjectRoot "build-native-voice-msvc"
$VoiceLab = Join-Path $env:USERPROFILE "NumerickVoiceLab"
$TtsPack = Join-Path $VoiceLab "NVIDIA-Qwen3-TTS\_qwen_tts_pack"
$ExpectedExe = Join-Path $BuildDir "numerick-nvigi-voice-worker.exe"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake was not found on PATH. Install CMake and reopen PowerShell."
}

if (-not (Test-Path (Join-Path $TtsPack "include\nvigi_tts_qwen3.h") -PathType Leaf)) {
    throw "The NVIDIA Qwen3-TTS public header was not found under $TtsPack."
}

if (-not (Test-Path (Join-Path $TtsPack "nvigi_core\include\nvigi.h") -PathType Leaf)) {
    throw "The NVIGI core headers were not found under $TtsPack."
}

if (-not (Test-Path $SourceDir -PathType Container)) {
    throw "Native worker source directory not found: $SourceDir"
}

$VsDevCmd = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path $VsDevCmd -PathType Leaf)) {
    throw "Visual Studio 2026 Build Tools environment script was not found: $VsDevCmd"
}

Write-Host "Project:       $ProjectRoot"
Write-Host "NVIGI pack:    $TtsPack"
Write-Host "Build folder:  $BuildDir"
Write-Host "Toolchain:     Visual Studio 2026 MSVC x64 + CMake NMake generator"
Write-Host ""

# Enter the VS developer environment and run CMake/NMake in the same cmd.exe
# process. This avoids relying on CMake having a dedicated VS 18 generator.
$Command = 'call "' + $VsDevCmd + '" -arch=x64 -host_arch=x64 && ' +
    'cmake -S "' + $SourceDir + '" -B "' + $BuildDir + '" -G "NMake Makefiles" ' +
    '-DCMAKE_BUILD_TYPE=Release "-DNVIGI_PACK_ROOT=' + $TtsPack + '" && ' +
    'cmake --build "' + $BuildDir + '" --parallel'

& $env:ComSpec /d /s /c $Command
if ($LASTEXITCODE -ne 0) {
    throw "The native NVIGI worker build failed. Review the compiler output above. NUMERICK's existing MinGW build was not touched."
}

if (-not (Test-Path $ExpectedExe -PathType Leaf)) {
    throw "Build completed but the expected worker executable was not found: $ExpectedExe"
}

Write-Host ""
Write-Host "Native NVIGI worker built successfully:"
Write-Host $ExpectedExe
