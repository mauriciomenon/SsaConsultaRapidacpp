$ErrorActionPreference = "Stop"

function Invoke-WindowsSmoke {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$DbPath,
        [Parameter(Mandatory = $true)][bool]$Clean,
        [string]$Preset = "dev",
        [string]$Arch = "",
        [string]$QtDir = "",
        [string]$QtRoot = "",
        [string]$QtSubdir = "",
        [string]$SQLiteRoot = "",
        [switch]$Open
    )

    if (-not (Test-Path -LiteralPath $DbPath -PathType Leaf)) {
        throw "Database file not found: $DbPath"
    }

    . (Join-Path $PSScriptRoot "lib\windows_build_layout.ps1")
    $layout = Resolve-WindowsBuildLayout -RepoRoot $RepoRoot -Preset $Preset -Arch $Arch `
        -QtDir $QtDir -QtSubdir $QtSubdir
    $buildParams = @{
        Preset = $Preset
        Arch = $layout.Arch
        ProjectRoot = $RepoRoot
    }
    if ($QtDir) { $buildParams.QtDir = $QtDir }
    if ($QtRoot) { $buildParams.QtRoot = $QtRoot }
    if ($QtSubdir) { $buildParams.QtSubdir = $QtSubdir }
    if ($SQLiteRoot) { $buildParams.SQLiteRoot = $SQLiteRoot }

    $buildScript = if ($Clean) {
        Join-Path $RepoRoot "scripts\build-windows.ps1"
    } else {
        Join-Path $RepoRoot "scripts\lazy_scripts\build-windows.ps1"
    }
    & $buildScript @buildParams

    & ctest --test-dir $layout.BuildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "Windows tests failed with exit code $LASTEXITCODE."
    }

    $runtimeDir = Join-Path $RepoRoot "build\runtime\windows\$($layout.Arch)"
    $configDir = Join-Path $runtimeDir "config"
    $runtimeDb = Join-Path $runtimeDir "ssas.db"
    $screenshot = Join-Path $runtimeDir "main.png"
    $preferencesSource = Join-Path $RepoRoot "config\ssa_cpp_preferences.json.example"
    if (-not (Test-Path -LiteralPath $preferencesSource -PathType Leaf)) {
        throw "Smoke preferences template not found: $preferencesSource"
    }

    New-Item -ItemType Directory -Path $runtimeDir, $configDir -Force | Out-Null
    Copy-Item -LiteralPath $DbPath -Destination $runtimeDb -Force
    Copy-Item -LiteralPath $preferencesSource `
        -Destination (Join-Path $configDir "ssa_cpp_preferences.json") -Force
    if (Test-Path -LiteralPath $screenshot) {
        Remove-Item -LiteralPath $screenshot -Force
    }

    $runParams = @{
        DbPath = $runtimeDb
        Preset = $Preset
        Arch = $layout.Arch
        ProjectRoot = $RepoRoot
        ConfigDir = $configDir
        Screenshot = $screenshot
    }
    if ($QtDir) { $runParams.QtDir = $QtDir }
    if ($QtSubdir) { $runParams.QtSubdir = $QtSubdir }

    $platformWasSet = Test-Path Env:\QT_QPA_PLATFORM
    $previousPlatform = $env:QT_QPA_PLATFORM
    try {
        $env:QT_QPA_PLATFORM = "offscreen"
        & (Join-Path $RepoRoot "scripts\run-windows.ps1") @runParams
        if ($LASTEXITCODE -ne 0) {
            throw "Windows offscreen smoke failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        if ($platformWasSet) {
            $env:QT_QPA_PLATFORM = $previousPlatform
        } else {
            if (Test-Path Env:\QT_QPA_PLATFORM) {
                Remove-Item Env:\QT_QPA_PLATFORM
            }
        }
    }

    if (-not (Test-Path -LiteralPath $screenshot -PathType Leaf) -or
        (Get-Item -LiteralPath $screenshot).Length -eq 0) {
        throw "Smoke screenshot was not produced: $screenshot"
    }

    Write-Output "Windows smoke screenshot: $screenshot"
    if ($Open) {
        $runParams.Remove("Screenshot")
        & (Join-Path $RepoRoot "scripts\run-windows.ps1") @runParams
        if ($LASTEXITCODE -ne 0) {
            throw "Windows visible launch failed with exit code $LASTEXITCODE."
        }
    }
}
