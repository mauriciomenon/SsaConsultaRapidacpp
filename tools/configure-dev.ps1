[CmdletBinding()]
param(
    [string]$QtDir,
    [string]$Preset = "dev"
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$QtDetectConfig = Join-Path $ScriptDir "qt-detect.conf"

function Read-QtDetectConfig {
    if (-not (Test-Path $QtDetectConfig)) {
        throw "Missing Qt detection config: $QtDetectConfig"
    }
    $config = @{}
    Get-Content $QtDetectConfig | ForEach-Object {
        if ($_ -match '^\s*([^#\s=]+)\s*=\s*(.*)$') {
            if ($config.ContainsKey($matches[1])) {
                throw "Duplicate key in Qt detection config: $($matches[1])"
            }
            $config[$matches[1]] = $matches[2].Trim()
        } elseif ($_ -notmatch '^\s*(#.*)?$') {
            throw "Malformed line in Qt detection config: $_"
        }
    }
    return $config
}

$DetectConfig = Read-QtDetectConfig

function Test-Command {
    param([string]$Name)
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Test-RequiredCommand {
    param(
        [string]$Name,
        [string]$Hint
    )
    if (-not (Test-Command $Name)) {
        throw "Missing command: $Name`n$Hint"
    }
}

function Test-QtPrefix {
    param([string]$Path)
    if (-not $Path) {
        return $false
    }
    return Test-Path (Join-Path $Path "lib\cmake\Qt6\Qt6Config.cmake")
}

function Find-QtFromKnownPath {
    param([string[]]$Paths)
    foreach ($path in $Paths) {
        if ($path -and (Test-QtPrefix $path)) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }
    return $null
}

function Find-QtFromEnvironment {
    $prefixPath = $null
    if ($env:CMAKE_PREFIX_PATH) {
        $prefixPath = ($env:CMAKE_PREFIX_PATH -split ';')[0]
    }
    return Find-QtFromKnownPath @($env:QT_DIR, $prefixPath)
}

function Find-QtUnderDefaultRoot {
    if (Test-Path "C:\Qt") {
        $candidates = @(Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
            ForEach-Object {
                Join-Path $_.FullName $DetectConfig.WINDOWS_QT_SUBDIR
            } |
            Where-Object { Test-QtPrefix $_ } |
            Sort-Object -Descending)

        if ($candidates.Count -gt 0) {
            return (Resolve-Path -LiteralPath $candidates[0]).Path
        }
    }

    return $null
}

function Find-QtDir {
    param([string]$ExplicitQtDir)

    $preferred = "C:\Qt\$($DetectConfig.QT_VERSION)\$($DetectConfig.WINDOWS_QT_SUBDIR)"
    return Find-QtFromKnownPath @(
        $ExplicitQtDir,
        $preferred,
        (Find-QtUnderDefaultRoot),
        (Find-QtFromEnvironment)
    )
}

function Test-RequiredCompiler {
    Test-RequiredCommand "cl.exe" "Run this script from 'Developer PowerShell for VS 2022' or install Visual Studio 2022 Build Tools with Desktop development with C++."
}

Test-RequiredCommand "cmake" "Install CMake 3.24 or newer and add it to PATH."
Test-RequiredCommand "ninja" "Install Ninja and add it to PATH."
Test-RequiredCompiler
Write-Output "Target Qt version: $($DetectConfig.QT_VERSION)"

$detectedQtDir = Find-QtDir -ExplicitQtDir $QtDir
if (-not $detectedQtDir) {
    throw @"
Qt was not detected.
Expected default path:
  C:\Qt\$($DetectConfig.QT_VERSION)\$($DetectConfig.WINDOWS_QT_SUBDIR)
Fallback search:
  C:\Qt\*\$($DetectConfig.WINDOWS_QT_SUBDIR)
You can run:
  .\tools\configure-dev.ps1 -QtDir "C:\Qt\$($DetectConfig.QT_VERSION)\$($DetectConfig.WINDOWS_QT_SUBDIR)"
"@
}

Write-Output "Using Qt prefix: $detectedQtDir"
$env:QT_DIR = $detectedQtDir

cmake --preset $Preset -DCMAKE_PREFIX_PATH="$detectedQtDir"
