#Requires -Version 5.1
$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "scripts\run-windows-smoke-clean.ps1") @args
