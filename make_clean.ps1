#Requires -Version 5.1
$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "scripts\make_clean.ps1") @args
