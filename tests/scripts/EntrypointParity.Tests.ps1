[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$probeDir = Join-Path ([IO.Path]::GetTempPath()) "ssa-entrypoint-parity-$PID"
$entrypoints = @(
    "make_clean.ps1",
    "build-windows.ps1",
    "build-windows-no-clean.ps1",
    "run-windows.ps1",
    "run-windows-smoke-clean.ps1",
    "run-windows-smoke-no-clean.ps1"
)

New-Item -ItemType Directory -Path $probeDir -Force | Out-Null
try {
    Push-Location $probeDir
    try {
        foreach ($entrypoint in $entrypoints) {
            $path = Join-Path $repoRoot $entrypoint
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
                throw "PowerShell entrypoint not found: $path"
            }
            & $path -Help | Out-Null
        }

        $cleanBuild = Join-Path $repoRoot "build-windows.ps1"
        $reuseRejected = $false
        try {
            & $cleanBuild -ReuseBuild
        }
        catch {
            $reuseRejected = $_.Exception.Message -like "ReuseBuild is internal.*"
        }
        if (-not $reuseRejected) {
            throw "Clean Windows entrypoint must reject -ReuseBuild."
        }
    }
    finally {
        Pop-Location
    }
}
finally {
    Remove-Item -LiteralPath $probeDir -Recurse -Force
}

Write-Output "PowerShell entrypoint parity: PASS"
