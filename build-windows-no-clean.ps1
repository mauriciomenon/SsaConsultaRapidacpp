#Requires -Version 5.1
$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "scripts\lazy_scripts\build-windows.ps1") @args
