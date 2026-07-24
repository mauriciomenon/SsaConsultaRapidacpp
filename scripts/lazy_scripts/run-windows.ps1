[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$DbPath,
    [string]$Preset = "dev",
    [string]$Arch = "",
    [string]$QtDir = "",
    [string]$QtSubdir = "",
    [ValidateSet("auto", "msvc", "mingw", "llvm-mingw")]
    [string]$Toolchain = "auto",
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
    Write-Output @"
Usage:
  .\scripts\lazy_scripts\run-windows.ps1 -DbPath <path-to-ssa-db> [-Toolchain <auto|msvc|mingw|llvm-mingw>] [-Preset <preset>] [-Arch <amd64|arm64>] [-QtDir <qt-dir>] [-QtSubdir <kit>] [-ProjectRoot <dir>] [-ConfigDir <dir>] [-Screenshot <file>] [-Help]

Options:
  -DbPath      Required. Full path to ssa database file.
  -Preset      Optional. CMake preset for build/runtime check. Default: dev.
  -Arch        Optional. Windows target architecture: amd64 or arm64.
  -QtDir       Optional. Qt install root to add to PATH at runtime.
  -QtSubdir    Optional. Qt kit used by the build.
  -Toolchain   Optional. Explicit compiler family. Default: auto.
  -ProjectRoot Optional. Override --project-root.
  -ConfigDir   Optional. Config directory passed to app.
  -Screenshot  Optional. Screenshot output path.
"@
    return
}

if (-not $DbPath) {
    throw "DbPath is required."
}

& $baseScript -DbPath $DbPath -Preset $Preset -Arch $Arch -ProjectRoot $ProjectRoot `
    -ConfigDir $ConfigDir -Screenshot $Screenshot -QtDir $QtDir -QtSubdir $QtSubdir `
    -Toolchain $Toolchain
