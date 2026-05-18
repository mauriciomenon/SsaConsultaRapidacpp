$ErrorActionPreference = "Stop"

function Show-Help {
    [string]$helpText = @"
Usage:
  .\scripts\lazy_scripts\package-windows.ps1 [base package-windows.ps1 arguments]

Delegates directly to:
  .\scripts\package-windows.ps1

This lazy wrapper keeps no packaging logic. Use it when you want the same
parameters as the base script from the lazy_scripts folder.
"@
    Write-Output $helpText
}

if ($args -contains "-Help" -or $args -contains "--help" -or $args -contains "/?") {
    Show-Help
    return
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$baseScript = (Resolve-Path (Join-Path $scriptDir "..\package-windows.ps1")).Path

& $baseScript @args
