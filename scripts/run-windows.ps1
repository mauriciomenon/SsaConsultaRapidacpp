[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$DbPath,
    [string]$Preset = "dev",
    [string]$QtDir,
    [string]$ProjectRoot = "",
    [string]$ExecutablePath,
    [string]$ConfigDir,
    [string]$Screenshot
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..")).Path
$buildDir = Join-Path $repoRoot "build\$Preset"
if ($ExecutablePath) {
    $executable = $ExecutablePath
} else {
    $executableCandidates = @(Join-Path $buildDir "ssa_consulta_rapida.exe")
    $executable = $executableCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if (-not $executable) {
    throw "Binary not found for preset '$Preset' at: $buildDir\ssa_consulta_rapida.exe. Run .\scripts\build-windows.ps1 -Preset $Preset or pass -ExecutablePath."
}

$qtBinPath = $null
if ($QtDir) {
    if (Test-Path $QtDir) {
        $qtBinPath = Join-Path (Resolve-Path $QtDir).Path "bin"
    } else {
        Write-Warning "QtDir not found: $QtDir. Continuing without adding Qt bin to PATH."
    }
} elseif ($env:QT_DIR) {
    $qtBinPath = Join-Path $env:QT_DIR "bin"
} else {
    Write-Warning "Qt bin path not provided. If the app fails to start with missing DLL errors, run from a Developer Command Prompt with Qt bin in PATH or pass -QtDir."
}

$argsList = @()
if ($ProjectRoot) {
    $argsList += @("--project-root", $ProjectRoot)
}
$argsList += @("--db", $DbPath)
if ($ConfigDir) { $argsList += @("--config-dir", $ConfigDir) }
if ($Screenshot) { $argsList += @("--screenshot", $Screenshot) }

if ($qtBinPath -and (Test-Path $qtBinPath)) {
    $previousPath = $env:Path
    try {
        $env:Path = "$qtBinPath;$previousPath"
        if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue) -and -not (Get-Command link.exe -ErrorAction SilentlyContinue)) {
            Write-Warning "Developer toolchain commands (cl.exe/link.exe) not found. Run this script from a Developer PowerShell for Visual Studio."
        }
        & $executable @argsList
    }
    finally {
        $env:Path = $previousPath
    }
} else {
    & $executable @argsList
}
