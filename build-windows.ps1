#Requires -Version 5.1
$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "scripts\build-windows.ps1") @args
