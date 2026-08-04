[CmdletBinding()]
param(
    [string]$DbPath = "",
    [string]$Preset = "dev",
    [string]$Arch = "",
    [string]$QtDir = "",
    [string]$QtRoot = "",
    [string]$QtSubdir = "",
    [ValidateSet("auto", "msvc", "llvm", "mingw", "llvm-mingw")]
    [string]$Toolchain = "auto",
    [string]$SQLiteRoot = "",
    [string]$ProjectRoot = "",
    [switch]$Open,
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = if ($ProjectRoot) { (Resolve-Path $ProjectRoot).Path } else { (Resolve-Path (Join-Path $scriptDir "..")).Path }
. (Join-Path $scriptDir 'lib\native_host_guard.ps1')
Assert-SsaWindowsHost -RepoRoot $repoRoot -ExpectedRoot (Get-SsaWindowsRepoRoot)

if ($Help) {
    Write-Output "Usage: .\run-windows-smoke-clean.ps1 [-Toolchain <auto|msvc|llvm|mingw|llvm-mingw>] [-DbPath <path>] [-Preset <preset>] [-Arch <amd64|arm64>] [-Open]"
    return
}

$dbPathExplicit = -not [string]::IsNullOrWhiteSpace($DbPath)
if (-not $dbPathExplicit) {
    $DbPath = Join-Path $repoRoot "data\ssas.db"
}

. (Join-Path $scriptDir "smoke-windows-core.ps1")
Invoke-WindowsSmoke -RepoRoot $repoRoot -DbPath $DbPath -DbPathExplicit $dbPathExplicit `
    -Clean $true -Preset $Preset `
    -Arch $Arch -QtDir $QtDir -QtRoot $QtRoot -QtSubdir $QtSubdir -Toolchain $Toolchain `
    -SQLiteRoot $SQLiteRoot -Open:$Open
