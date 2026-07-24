[CmdletBinding()]
param(
    [string]$DbPath = "",
    [string]$Preset = "dev",
    [string]$Arch = "",
    [string]$ProjectRoot = "",
    [string]$ConfigDir = "",
    [string]$Screenshot = "",
    [string]$QtDir = "",
    [string]$QtSubdir = "",
    [ValidateSet("auto", "msvc", "mingw", "llvm-mingw")]
    [string]$Toolchain = "auto",
    [switch]$AllowMissingDb,
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = if ($ProjectRoot) { (Resolve-Path $ProjectRoot).Path } else { (Resolve-Path (Join-Path $scriptDir "..")).Path }
. (Join-Path $scriptDir "lib\windows_build_layout.ps1")
$layout = Resolve-WindowsBuildLayout -RepoRoot $repoRoot -Preset $Preset -Arch $Arch `
    -QtDir $QtDir -QtSubdir $QtSubdir -Toolchain $Toolchain
$buildDir = $layout.BuildDir
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
  .\scripts\run-windows.ps1 -Toolchain <auto|msvc|mingw|llvm-mingw> [-DbPath <path>] [-Preset <preset>] [-Arch <amd64|arm64>] [-QtDir <qt-dir>] [-QtSubdir <kit>] [-ProjectRoot <dir>] [-ConfigDir <dir>] [-Screenshot <file>] [-AllowMissingDb]
"@
    return
}

$executable = Join-Path $buildDir "ssa_consulta_rapida.exe"
if (-not (Test-Path $executable)) {
    $buildHint = Join-Path $scriptDir "build-windows.ps1"
    throw "Binary not found for preset '$Preset' at: $executable. Run: & '$buildHint' -Preset '$Preset'"
}

$usingDefaultDb = [string]::IsNullOrWhiteSpace($DbPath)
if ($usingDefaultDb) {
    $DbPath = Join-Path $repoRoot "data\ssas.db"
}

if (-not (Test-Path -LiteralPath $DbPath -PathType Leaf)) {
    if (-not $usingDefaultDb -and -not $AllowMissingDb) {
        throw "Database path not found: $DbPath"
    }

    $dbDirectory = Split-Path -Parent $DbPath
    if (-not (Test-Path -LiteralPath $dbDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $dbDirectory -Force | Out-Null
    }
    Write-Warning "Database file not found at '$DbPath'. The application will open so you can load data and create it."
}

$qtBinPath = $null
if ($QtDir) {
    if (Test-Path $QtDir) {
        $qtBinPath = Join-Path (Resolve-Path $QtDir).Path "bin"
    } else {
        Write-Warning "QtDir not found: $QtDir. Continuing without adding Qt bin to PATH."
    }
} else {
    $cacheFile = Join-Path $buildDir "CMakeCache.txt"
    $qtDirectoryLine = Select-String -LiteralPath $cacheFile `
        -Pattern '^Qt6_DIR:[^=]+=(.+)$' -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($qtDirectoryLine) {
        $qtCmakeDirectory = $qtDirectoryLine.Matches[0].Groups[1].Value.Trim()
        $qtPrefix = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $qtCmakeDirectory))
        $candidateQtBin = Join-Path $qtPrefix "bin"
        if (Test-Path -LiteralPath $candidateQtBin -PathType Container) {
            $qtBinPath = $candidateQtBin
        }
    }
}

$argsList = @(
    "--project-root", $repoRoot,
    "--db", $DbPath
)
if ($ConfigDir) { $argsList += @("--config-dir", $ConfigDir) }
if ($Screenshot) { $argsList += @("--screenshot", $Screenshot) }

$previousPath = $null
if ($qtBinPath -and (Test-Path $qtBinPath)) {
    $previousPath = $env:Path
    $env:Path = "$qtBinPath;$previousPath"
}

try {
    $quotedArgs = $argsList | ForEach-Object {
        '"' + $_.Replace('"', '\"') + '"'
    }
    $process = Start-Process -FilePath $executable -ArgumentList $quotedArgs `
        -NoNewWindow -PassThru -Wait
    if ($process.ExitCode -ne 0) {
        throw "Windows application failed with exit code $($process.ExitCode)."
    }
}
finally {
    if ($null -ne $previousPath) {
        $env:Path = $previousPath
    }
}
