$ErrorActionPreference = "Stop"

# Shared project path contract for PowerShell entrypoints.
# This file only resolves stable repository paths; it does not build or launch.

function Resolve-ProjectDefaultDbPath {
    param([Parameter(Mandatory = $true)][string]$Root)

    $candidate = Join-Path $Root "data\ssas.db"
    if (Test-Path $candidate) {
        return $candidate
    }

    return ""
}

function Write-ProjectDefaultDbNotFound {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$ExplicitPathHint
    )

    Write-Host "Database file not found in the default location." -ForegroundColor Red
    Write-Host "Place a valid SQLite file at:"
    Write-Host "  $Root\data\ssas.db"
    Write-Host $ExplicitPathHint
}
