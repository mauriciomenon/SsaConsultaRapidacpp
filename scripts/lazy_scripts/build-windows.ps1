[CmdletBinding()]
param(
    [string]$Preset = "dev",
    [string]$Arch = "",
    [string]$QtDir = "",
    [string]$QtRoot = "",
    [string]$QtSubdir = "",
    [ValidateSet("auto", "msvc", "mingw", "llvm-mingw")]
    [string]$Toolchain = "auto",
    [string]$SQLiteRoot = "",
    [string]$ProjectRoot = "",
    [string[]]$CmakeExtraArgs = @(),
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = if ($ProjectRoot) { (Resolve-Path $ProjectRoot).Path } else { (Resolve-Path (Join-Path $scriptDir "..\..")).Path }
$baseScript = (Resolve-Path (Join-Path $scriptDir "..\build-windows.ps1")).Path

if ($Help) {
    Write-Output @"
Usage:
  .\scripts\lazy_scripts\build-windows.ps1 [-Toolchain <auto|msvc|mingw|llvm-mingw>] [-Preset <preset>] [-Arch <amd64|arm64>] [-QtDir <qt-dir>] [-QtRoot <root>] [-QtSubdir <kit>] [-SQLiteRoot <path>] [-ProjectRoot <dir>] [-CmakeExtraArgs <args>] [-Help]

Incremental build entrypoint. Reuses only a compatible canonical build cache.

Options:
  -Preset       Optional. CMake preset to build. Default: dev.
  -Arch         Optional. Windows target architecture: amd64 or arm64.
  -QtDir        Optional. Qt install root to pass to configure.
  -QtRoot       Optional. Root containing versioned Qt installs. Default: C:\Qt.
  -QtSubdir     Optional. Desktop kit such as msvc2022_64 or llvm-mingw_64.
  -Toolchain    Optional. Explicit compiler family. Default: auto.
  -SQLiteRoot   Optional. SQLite root to pass to configure.
  -ProjectRoot  Optional. Override repository root.
  -CmakeExtraArgs Optional. Additional CMake configure arguments.
"@
    return
}

& $baseScript -Preset $Preset -Arch $Arch -QtDir $QtDir -QtRoot $QtRoot `
    -QtSubdir $QtSubdir -Toolchain $Toolchain -SQLiteRoot $SQLiteRoot -ProjectRoot $repoRoot `
    -CmakeExtraArgs $CmakeExtraArgs -ReuseBuild
