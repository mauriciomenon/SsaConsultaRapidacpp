[CmdletBinding()]
param(
    [string]$ProjectRoot = "",
    [switch]$DryRun,
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = if ($ProjectRoot) {
    (Resolve-Path $ProjectRoot).Path
} else {
    (Resolve-Path (Join-Path $scriptDir "..")).Path
}
$buildRoot = Join-Path $repoRoot "build"

if ($Help) {
    Write-Output @"
Usage:
  .\scripts\make_clean.ps1 [-DryRun] [-ProjectRoot <path>]
  .\make_clean.ps1 [-DryRun]

Remove generated CMake artifacts under <repo>\build.
Preserves .deps-cache, data, dist, packaging, config, and source files.
"@
    return
}

$presetFile = Join-Path $repoRoot "CMakePresets.json"
$cleanScript = Join-Path $repoRoot "scripts\make_clean.ps1"
if (-not (Test-Path -LiteralPath $presetFile -PathType Leaf) -or
    -not (Test-Path -LiteralPath $cleanScript -PathType Leaf)) {
    throw "ProjectRoot is not an SSA Consulta Rapida repository: $repoRoot"
}

if ($DryRun) {
    Write-Output "Would remove: $buildRoot"
    return
}

if (Test-Path -LiteralPath $buildRoot -PathType Container) {
    Write-Output "Cleaning all CMake build artifacts: $buildRoot"
    Remove-Item -LiteralPath $buildRoot -Recurse -Force
} else {
    Write-Output "Skipping build cleanup: $buildRoot not present."
}
