[CmdletBinding()]
param(
    [string]$Preset = "dev",
    [string]$QtDir,
    [string]$SQLiteRoot
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..")).Path
$configureScript = Join-Path $repoRoot "tools\configure-dev.ps1"
$buildDir = Join-Path $repoRoot "build\$Preset"
$cacheFile = Join-Path $buildDir "CMakeCache.txt"

if ($QtDir -or $SQLiteRoot) {
    & $configureScript -Preset $Preset -QtDir $QtDir -SQLiteRoot $SQLiteRoot
} else {
    if (-not (Test-Path $cacheFile)) {
        & $configureScript -Preset $Preset
    }
}

cmake --build --preset $Preset
