[CmdletBinding()]
param(
    [string]$DbPath = "",
    [string]$Preset = "dev",
    [string]$ProjectRoot = "",
    [string]$ConfigDir = "",
    [string]$Screenshot = "",
    [string]$QtDir = "",
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = if ($ProjectRoot) { (Resolve-Path $ProjectRoot).Path } else { (Resolve-Path (Join-Path $scriptDir "..")).Path }
$buildDir = Join-Path $repoRoot "build\$Preset"
. (Join-Path $scriptDir "project-paths.ps1")

if ($Help) {
    Write-Output @"
Usage:
  .\scripts\run-windows.ps1

Run built Windows app using default project DB resolution.

The database path is resolved in this order:
  1) <repo>\data\ssas.db

Defaults:
  Preset: dev
  Project root: directory that contains this script.

Explicit options can be passed directly:
  .\scripts\run-windows.ps1 -DbPath <path> -Preset <preset> -ProjectRoot <dir> -ConfigDir <dir> -Screenshot <file>
"@
    return
}

if (-not $DbPath) {
    $DbPath = Resolve-ProjectDefaultDbPath -Root $repoRoot
}
if (-not $DbPath) {
    Write-ProjectDefaultDbNotFound `
        -Root $repoRoot `
        -ExplicitPathHint "Use .\scripts\run-windows.ps1 -DbPath <path> for an explicit external DB path."
    exit 1
}

if (-not (Test-Path $DbPath)) {
    throw "Database path not found: $DbPath"
}

$executable = Join-Path $buildDir "ssa_consulta_rapida.exe"
if (-not (Test-Path $executable)) {
    throw "Binary not found for preset '$Preset' at: $executable. Run .\scripts\build-windows.ps1 before running .\scripts\run-windows.ps1."
}

$qtBinPath = $null
if ($QtDir) {
    if (Test-Path $QtDir) {
        $qtBinPath = Join-Path (Resolve-Path $QtDir).Path "bin"
    } else {
        Write-Warning "QtDir not found: $QtDir. Continuing without adding Qt bin to PATH."
    }
}

$argsList = @(
    "--project-root", $repoRoot,
    "--db", $DbPath
)
if ($ConfigDir) { $argsList += @("--config-dir", $ConfigDir) }
if ($Screenshot) { $argsList += @("--screenshot", $Screenshot) }

if ($qtBinPath -and (Test-Path $qtBinPath)) {
    $previousPath = $env:Path
    try {
        $env:Path = "$qtBinPath;$previousPath"
        & $executable @argsList
    }
    finally {
        $env:Path = $previousPath
    }
} else {
    & $executable @argsList
}
