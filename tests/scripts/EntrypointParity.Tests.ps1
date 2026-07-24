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

        $smokeScript = Join-Path $repoRoot "scripts\run-windows-smoke-clean.ps1"
        $defaultDbAccepted = $false
        try {
            & $smokeScript -ProjectRoot $probeDir -Preset "invalid/preset"
        }
        catch {
            $defaultDbAccepted = $_.Exception.Message -eq "Invalid CMake preset name: invalid/preset"
        }
        if (-not $defaultDbAccepted) {
            throw "Windows smoke must allow the missing default database on first run."
        }

        $explicitDbRejected = $false
        try {
            & $smokeScript -ProjectRoot $probeDir `
                -DbPath (Join-Path $probeDir "missing.db") -Preset "invalid/preset"
        }
        catch {
            $explicitDbRejected = $_.Exception.Message -like "Database file not found:*"
        }
        if (-not $explicitDbRejected) {
            throw "Windows smoke must reject an explicit missing database."
        }

        . (Join-Path $repoRoot "scripts\lib\windows_build_layout.ps1")
        $defaultLayout = Resolve-WindowsBuildLayout `
            -RepoRoot $probeDir -Preset "dev" -Arch "amd64"
        if ($defaultLayout.QtKit -ne "mingw_64") {
            throw "Windows amd64 default Qt kit must be mingw_64."
        }
        $expectedKits = @{
            "msvc" = "msvc2022_64"
            "mingw" = "mingw_64"
            "llvm-mingw" = "llvm-mingw_64"
        }
        foreach ($toolchain in $expectedKits.Keys) {
            $layout = Resolve-WindowsBuildLayout -RepoRoot $probeDir -Preset "dev" `
                -Arch "amd64" -Toolchain $toolchain
            if ($layout.QtKit -ne $expectedKits[$toolchain]) {
                throw "Toolchain '$toolchain' resolved unexpected Qt kit '$($layout.QtKit)'."
            }
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
