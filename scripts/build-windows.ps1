[CmdletBinding()]
param(
    [string]$Preset = "dev",
    [string]$QtDir = "",
    [string]$QtRoot = "",
    [string]$QtSubdir = "",
    [string]$SQLiteRoot = "",
    [string]$ProjectRoot = "",
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = if ($ProjectRoot) { (Resolve-Path $ProjectRoot).Path } else { (Resolve-Path (Join-Path $scriptDir "..")).Path }
$configureScript = Join-Path $repoRoot "tools\configure-dev.ps1"
$preset = if ($Preset) { $Preset } else { "dev" }
$buildDir = Join-Path $repoRoot "build\$preset"
$cacheFile = Join-Path $buildDir "CMakeCache.txt"

if ($Help) {
    Write-Output @"
Usage:
  .\scripts\build-windows.ps1

Build the Windows target (default preset: dev).

Defaults:
  Preset: dev
  Repository root: directory that contains this script.

Explicit options can be used through:
  .\scripts\build-windows.ps1 -Preset <preset> [-QtDir <qt-dir>] [-QtRoot <root>] [-QtSubdir <kit>] [-SQLiteRoot <path>]

Qt kit examples:
  .\scripts\build-windows.ps1
  .\scripts\build-windows.ps1 -QtSubdir llvm-mingw_64
  .\scripts\build-windows.ps1 -QtSubdir mingw_64
"@
    return
}

if ($QtDir -or $QtRoot -or $QtSubdir -or $SQLiteRoot) {
    & $configureScript -Preset $preset -QtDir $QtDir -QtRoot $QtRoot -QtSubdir $QtSubdir -SQLiteRoot $SQLiteRoot
} else {
    if (-not (Test-Path $cacheFile)) {
        & $configureScript -Preset $preset
    }
}

cmake --build --preset $preset
