[CmdletBinding()]
param(
    [string]$Preset = "dev",
    [string]$QtDir = "",
    [string]$SQLiteRoot = "",
    [string]$ProjectRoot = "",
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = if ($ProjectRoot) { (Resolve-Path $ProjectRoot).Path } else { (Resolve-Path (Join-Path $scriptDir "..\..")).Path }
$baseScript = Join-Path $repoRoot "scripts\build-windows.ps1"

if ($Help) {
    Write-Host @"
Usage:
  .\scripts\lazy_scripts\build-windows.ps1 [-Preset <preset>] [-QtDir <qt-dir>] [-SQLiteRoot <path>] [-ProjectRoot <dir>] [-Help]

Options:
  -Preset       Optional. CMake preset to build. Default: dev.
  -QtDir        Optional. Qt install root to pass to configure.
  -SQLiteRoot   Optional. SQLite root to pass to configure.
  -ProjectRoot  Optional. Override repository root.
"@
    return
}

& $baseScript -Preset $Preset -QtDir $QtDir -SQLiteRoot $SQLiteRoot -ProjectRoot $repoRoot
