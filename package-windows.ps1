#Requires -Version 5.1
$ErrorActionPreference = "Stop"

# Compatibility wrapper for Windows checkouts where Git symlink support is disabled.
$scriptPath = Join-Path $PSScriptRoot "scripts\package-windows.ps1"
if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) {
    throw "Package script not found: $scriptPath"
}

& $scriptPath @args
