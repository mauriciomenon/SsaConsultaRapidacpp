[CmdletBinding()]
param(
    [string]$QtDir,
    [string]$SQLiteRoot,
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
foreach ($requiredKey in @("QT_VERSION", "WINDOWS_QT_SUBDIR")) {
    if (-not $DetectConfig.ContainsKey($requiredKey) -or -not $DetectConfig[$requiredKey]) {
        throw "Missing required key in Qt detection config: $requiredKey"
    }
}

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

function Find-FirstValidPrefix {
    param(
        [string[]]$Paths,
        [scriptblock]$IsValid
    )
    foreach ($path in $Paths) {
        if ($path -and (& $IsValid $path)) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }
    return $null
}

function Find-QtFromKnownPath {
    param([string[]]$Paths)
    return Find-FirstValidPrefix -Paths $Paths -IsValid { param([string]$Path) Test-QtPrefix $Path }
}

function Find-QtFromEnvironment {
    $paths = @($env:QT_DIR)
    if ($env:CMAKE_PREFIX_PATH) {
        $paths += $env:CMAKE_PREFIX_PATH -split ';'
    }
    return Find-QtFromKnownPath $paths
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

    $defaultInstallPath = "C:\Qt\$($DetectConfig.QT_VERSION)\$($DetectConfig.WINDOWS_QT_SUBDIR)"
    return Find-QtFromKnownPath @(
        $ExplicitQtDir,
        $defaultInstallPath,
        (Find-QtUnderDefaultRoot),
        (Find-QtFromEnvironment)
    )
}

function Test-SqlitePrefix {
    param([string]$Path)
    if (-not $Path) {
        return $false
    }
    return (Test-Path (Join-Path $Path "include\sqlite3.h")) -and
        (Test-Path (Join-Path $Path "lib\sqlite3.lib"))
}

function Find-SqliteFromKnownPath {
    param([string[]]$Paths)
    return Find-FirstValidPrefix -Paths $Paths -IsValid { param([string]$Path) Test-SqlitePrefix $Path }
}

function Get-DefaultVcpkgTriplet {
    if ($env:VCPKG_DEFAULT_TRIPLET) {
        return $env:VCPKG_DEFAULT_TRIPLET
    }
    if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") {
        return "arm64-windows"
    }
    return "x64-windows"
}

function Find-SqliteFromVcpkg {
    $triplet = Get-DefaultVcpkgTriplet
    $paths = @()
    if ($env:VCPKG_ROOT) {
        $paths += Join-Path $env:VCPKG_ROOT "installed\$triplet"
    }
    $paths += "C:\vcpkg\installed\$triplet"
    return Find-SqliteFromKnownPath $paths
}

function Find-SqliteRoot {
    param([string]$ExplicitSQLiteRoot)

    return Find-SqliteFromKnownPath @(
        $ExplicitSQLiteRoot,
        $env:SQLite3_ROOT,
        $env:SQLITE_ROOT,
        (Find-SqliteFromVcpkg)
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

$cmakeArgs = @(
    "--preset", $Preset,
    "-DCMAKE_PREFIX_PATH=$detectedQtDir"
)

$detectedSQLiteRoot = Find-SqliteRoot -ExplicitSQLiteRoot $SQLiteRoot
if ($detectedSQLiteRoot) {
    Write-Output "Using SQLite prefix: $detectedSQLiteRoot"
    $cmakeArgs += "-DSQLite3_INCLUDE_DIR=$(Join-Path $detectedSQLiteRoot "include")"
    $cmakeArgs += "-DSQLite3_LIBRARY=$(Join-Path $detectedSQLiteRoot "lib\sqlite3.lib")"
} else {
    Write-Output "SQLite development files were not detected automatically."
    Write-Output "If CMake fails with 'Could NOT find SQLite3', install with:"
    Write-Output "  vcpkg install sqlite3:$(Get-DefaultVcpkgTriplet)"
    Write-Output "Then rerun this script with VCPKG_ROOT set, or pass:"
    Write-Output "  .\tools\configure-dev.ps1 -SQLiteRoot `"C:\vcpkg\installed\$(Get-DefaultVcpkgTriplet)`""
}

cmake @cmakeArgs
