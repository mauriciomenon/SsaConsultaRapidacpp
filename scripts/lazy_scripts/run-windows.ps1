[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$DbPath,
    [string]$Preset = "dev",
    [string]$QtDir = "",
    [string]$ProjectRoot = "",
    [string]$ConfigDir = "",
    [string]$Screenshot = "",
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = if ($ProjectRoot) { (Resolve-Path $ProjectRoot).Path } else { (Resolve-Path (Join-Path $scriptDir "..\..")).Path }
$baseScript = Join-Path $repoRoot "scripts\run-windows.ps1"

if ($Help) {
    Write-Host @"
Usage:
  .\scripts\lazy_scripts\run-windows.ps1 -DbPath <path-to-ssa-db> [-Preset <preset>] [-QtDir <qt-dir>] [-ProjectRoot <dir>] [-ConfigDir <dir>] [-Screenshot <file>] [-Help]

Options:
  -DbPath      Required. Full path to ssa database file.
  -Preset      Optional. CMake preset for build/runtime check. Default: dev.
  -QtDir       Optional. Qt install root to add to PATH at runtime.
  -ProjectRoot Optional. Override --project-root.
  -ConfigDir   Optional. Config directory passed to app.
  -Screenshot  Optional. Screenshot output path.
"@
    return
}

if (-not $DbPath) {
    throw "DbPath is required."
}

& $baseScript -DbPath $DbPath -Preset $Preset -ProjectRoot $ProjectRoot -ConfigDir $ConfigDir -Screenshot $Screenshot -QtDir $QtDir
