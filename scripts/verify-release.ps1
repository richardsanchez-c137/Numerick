[CmdletBinding()]
param(
    [string]$BuildDirectory = "build-release",
    [string]$Generator = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildInput = $BuildDirectory

if ([System.IO.Path]::IsPathRooted($buildInput)) {
    throw "BuildDirectory must be a relative path inside the repository."
}

$buildPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $buildInput))
$rootPrefix = $repoRoot + [System.IO.Path]::DirectorySeparatorChar

if (
    $buildPath.Equals($repoRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
    -not $buildPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)
) {
    throw "BuildDirectory must resolve to a child directory of the repository."
}

# Prevent -Clean from deleting source or documentation directories by mistake.
# Only allow conventional build folders directly under the repository root.
$relativeBuildPath = $buildPath.Substring($rootPrefix.Length)
if (
    $relativeBuildPath -match '[\/]' -or
    $relativeBuildPath -notmatch '^(build|build-[A-Za-z0-9._-]+|cmake-build-[A-Za-z0-9._-]+)$'
) {
    throw "BuildDirectory must be a direct child folder named build, build-*, or cmake-build-* ."
}

# Prevent -Clean from deleting source or documentation directories by mistake.
# Only allow conventional build folders directly under the repository root.
$relativeBuildPath = $buildPath.Substring($rootPrefix.Length)
if (
    $relativeBuildPath -match '[\/]' -or
    $relativeBuildPath -notmatch '^(build|build-[A-Za-z0-9._-]+|cmake-build-[A-Za-z0-9._-]+)$'
) {
    throw "BuildDirectory must be a direct child folder named build, build-*, or cmake-build-* ."
}

# Protect source directories from accidental deletion when -Clean is used.
# Only direct child folders with conventional build-directory names are accepted.
$relativeBuildPath = $buildPath.Substring($rootPrefix.Length)
if (
    $relativeBuildPath -match '[\\/]' -or
    $relativeBuildPath -notmatch '^(build|build-[A-Za-z0-9._-]+|cmake-build-[A-Za-z0-9._-]+)$'
) {
    throw "BuildDirectory must be a direct child of the repository named build, build-*, or cmake-build-*."
}

# Fail early if release version strings drift across the build, app, and docs.
$cmakeText = Get-Content -LiteralPath (Join-Path $repoRoot "CMakeLists.txt") -Raw
$readmeText = Get-Content -LiteralPath (Join-Path $repoRoot "README.md") -Raw
$documentationText = Get-Content -LiteralPath (Join-Path $repoRoot "documentation.md") -Raw
$mainText = Get-Content -LiteralPath (Join-Path $repoRoot "src\main.cpp") -Raw

$versionPatterns = @{
    "CMakeLists.txt" = 'project\(\s*Numerick\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)'
    "README.md" = '## Current development status[^\r\n]*?([0-9]+\.[0-9]+\.[0-9]+)'
    "documentation.md" = '\*\*Current version:\*\*\s*([0-9]+\.[0-9]+\.[0-9]+)'
    "src/main.cpp" = 'Core version:\s*([0-9]+\.[0-9]+\.[0-9]+)'
}
$versionTexts = @{
    "CMakeLists.txt" = $cmakeText
    "README.md" = $readmeText
    "documentation.md" = $documentationText
    "src/main.cpp" = $mainText
}
$versions = @{}
foreach ($name in $versionPatterns.Keys) {
    $match = [regex]::Match($versionTexts[$name], $versionPatterns[$name])
    if (-not $match.Success) {
        throw "Could not read a version number from $name."
    }
    $versions[$name] = $match.Groups[1].Value
}
$distinctVersions = @($versions.Values | Select-Object -Unique)
if ($distinctVersions.Count -ne 1) {
    $details = ($versions.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join ", "
    throw "Version mismatch detected across project files: $details"
}
Write-Host "Version consistency check passed: $($distinctVersions[0])"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake was not found on PATH. Install CMake or launch PowerShell from your configured MSYS2 UCRT64 environment."
}
if (-not (Get-Command ctest -ErrorAction SilentlyContinue)) {
    throw "CTest was not found on PATH. Check your CMake installation."
}

if (Test-Path -LiteralPath $buildPath) {
    if (-not $Clean) {
        Write-Host "Using existing build directory: $buildPath"
        Write-Host "For a clean release verification, rerun with -Clean."
    }
    else {
        Write-Host "Removing selected build directory: $buildPath"
        Remove-Item -LiteralPath $buildPath -Recurse -Force
    }
}

$configureArgs = @("-S", $repoRoot, "-B", $buildPath)
if (-not [string]::IsNullOrWhiteSpace($Generator)) {
    $configureArgs += @("-G", $Generator)
}

Write-Host "`n[1/3] Configure CMake"
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE."
}

Write-Host "`n[2/3] Build all targets"
& cmake --build $buildPath --parallel 2
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed with exit code $LASTEXITCODE."
}

Write-Host "`n[3/3] Run all registered CTest suites"
& ctest --test-dir $buildPath --output-on-failure --no-tests=error
if ($LASTEXITCODE -ne 0) {
    throw "CTest failed with exit code $LASTEXITCODE."
}

Write-Host "`nRelease verification passed: configure, build, and CTest all succeeded."
Write-Host "Build directory: $buildPath"
