[CmdletBinding()]
param(
    [string]$QtDir,
    [string]$QtRoot,
    [string]$QtSubdir,
    [string]$SQLiteRoot,
    [string]$Preset = "dev",
    [string[]]$CmakeExtraArgs = @(),
    [switch]$PrintQtSelection
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
foreach ($requiredKey in @("QT_VERSION", "QT_VERSION_FAMILY", "WINDOWS_QT_SUBDIR", "WINDOWS_QT_FALLBACK_SUBDIRS")) {
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
    return Test-Path (Join-Path $Path "lib/cmake/Qt6/Qt6Config.cmake")
}

function ConvertTo-NormalizedPath {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }

    $normalized = $Path.Trim().Trim('"').Trim("'")
    if (-not $normalized) {
        return $null
    }
    $normalized = $normalized -replace '^[\\/]+([A-Za-z]:)', '$1'
    $normalized = $normalized.TrimEnd('\', '/')
    return $normalized
}

function Get-NormalizedPathList {
    param([string]$Value)
    if ([string]::IsNullOrWhiteSpace($Value)) {
        return @()
    }
    return $Value.Split(@(';'), [System.StringSplitOptions]::RemoveEmptyEntries) |
        ForEach-Object { ConvertTo-NormalizedPath $_ } |
        Where-Object { $_ }
}

function Find-FirstValidPrefix {
    param(
        [string[]]$Paths,
        [scriptblock]$IsValid
    )
    foreach ($path in $Paths) {
        $candidate = ConvertTo-NormalizedPath $path
        if (-not $candidate) {
            continue
        }
        if ($candidate -and (& $IsValid $candidate)) {
            return (Resolve-Path -LiteralPath $candidate -ErrorAction SilentlyContinue).Path
        }
    }
    return $null
}

function Get-QtVersionForPrefix {
    param([string]$Path)

    $versionFile = Join-Path $Path "lib/cmake/Qt6/Qt6ConfigVersionImpl.cmake"
    if (-not (Test-Path $versionFile)) {
        return $null
    }
    $versionLine = Select-String -Path $versionFile -Pattern 'set\(PACKAGE_VERSION\s+"([0-9.]+)"\)' |
        Select-Object -First 1
    if (-not $versionLine) {
        return $null
    }
    return [version]$versionLine.Matches[0].Groups[1].Value
}

function Test-QtVersionFamily {
    param([version]$Version)

    if (-not $Version) {
        return $false
    }
    $familyParts = $DetectConfig.QT_VERSION_FAMILY.Split('.')
    return $Version.Major -eq [int]$familyParts[0] -and
        $Version.Minor -eq [int]$familyParts[1]
}

function Get-DesktopQtSubdir {
    $subdirs = @($DetectConfig.WINDOWS_QT_SUBDIR)
    $subdirs += $DetectConfig.WINDOWS_QT_FALLBACK_SUBDIRS.Split(
        @(':'),
        [System.StringSplitOptions]::RemoveEmptyEntries
    )
    return @($subdirs | Select-Object -Unique)
}

function Test-DesktopQtSubdir {
    param([string]$Subdir)
    return $Subdir -in (Get-DesktopQtSubdir)
}

function ConvertFrom-QtCmakeDir {
    param([string]$Path)

    $normalized = ConvertTo-NormalizedPath $Path
    if ($normalized -match '^(.*)[\\/]lib[\\/]cmake[\\/]Qt6$') {
        return $matches[1]
    }
    return $normalized
}

function Get-QtSelectionForPrefix {
    param(
        [string]$Path,
        [string]$Source,
        [string]$ExpectedSubdir = ""
    )

    $normalized = ConvertFrom-QtCmakeDir $Path
    if (-not (Test-QtPrefix $normalized)) {
        throw "Qt path is not valid: $normalized"
    }
    $version = Get-QtVersionForPrefix $normalized
    if (-not (Test-QtVersionFamily $version)) {
        $detectedVersion = if ($version) { $version.ToString() } else { "unknown" }
        throw "Qt $($DetectConfig.QT_VERSION_FAMILY).x is required; detected $detectedVersion at $normalized"
    }
    $subdir = Split-Path $normalized -Leaf
    if ($ExpectedSubdir -and $subdir -ne $ExpectedSubdir) {
        throw "Qt path uses kit '$subdir', but -QtSubdir requested '$ExpectedSubdir'."
    }
    if (-not (Test-DesktopQtSubdir $subdir)) {
        throw "No desktop Qt kit was selected. '$subdir' is not a supported desktop kit. WASM kits target browsers."
    }

    return [PSCustomObject]@{
        Path = (Resolve-Path -LiteralPath $normalized).Path
        Version = $version
        Subdir = $subdir
        Source = $Source
    }
}

function Find-QtFromEnvironment {
    param([string]$ExpectedSubdir = "")

    $environmentPaths = @()
    foreach ($path in (Get-NormalizedPathList $env:Qt6_DIR)) {
        $environmentPaths += ConvertFrom-QtCmakeDir $path
    }
    $environmentPaths += Get-NormalizedPathList $env:QT_DIR
    $environmentPaths += Get-NormalizedPathList $env:CMAKE_PREFIX_PATH

    foreach ($path in $environmentPaths) {
        if (Test-QtPrefix $path) {
            return Get-QtSelectionForPrefix -Path $path -Source "environment" -ExpectedSubdir $ExpectedSubdir
        }
    }
    return $null
}

function Find-QtUnderRoot {
    param(
        [string]$Root,
        [string]$Subdir
    )

    if (-not (Test-Path $Root)) {
        return $null
    }
    $candidates = foreach ($versionDir in Get-ChildItem $Root -Directory -ErrorAction SilentlyContinue) {
        $candidate = Join-Path $versionDir.FullName $Subdir
        if (-not (Test-QtPrefix $candidate)) {
            continue
        }
        $version = Get-QtVersionForPrefix $candidate
        if (Test-QtVersionFamily $version) {
            [PSCustomObject]@{
                Path = $candidate
                Version = $version
                Subdir = $Subdir
                Source = "automatic"
            }
        }
    }
    return $candidates | Sort-Object -Property Version -Descending | Select-Object -First 1
}

function Get-InstalledQtSubdir {
    param([string]$Root)

    if (-not (Test-Path $Root)) {
        return @()
    }
    $subdirs = foreach ($versionDir in Get-ChildItem $Root -Directory -ErrorAction SilentlyContinue) {
        foreach ($kitDir in Get-ChildItem $versionDir.FullName -Directory -ErrorAction SilentlyContinue) {
            if (Test-QtPrefix $kitDir.FullName) {
                $version = Get-QtVersionForPrefix $kitDir.FullName
                if (Test-QtVersionFamily $version) {
                    $kitDir.Name
                }
            }
        }
    }
    return @($subdirs | Sort-Object -Unique)
}

function Find-QtSelection {
    param(
        [string]$ExplicitQtDir,
        [string]$Root,
        [string]$RequestedSubdir
    )

    if ($ExplicitQtDir) {
        return Get-QtSelectionForPrefix -Path $ExplicitQtDir -Source "explicit" -ExpectedSubdir $RequestedSubdir
    }

    $environmentSelection = Find-QtFromEnvironment -ExpectedSubdir $RequestedSubdir
    if ($environmentSelection) {
        return $environmentSelection
    }

    $subdirs = if ($RequestedSubdir) { @($RequestedSubdir) } else { Get-DesktopQtSubdir }
    foreach ($subdir in $subdirs) {
        if (-not (Test-DesktopQtSubdir $subdir)) {
            throw "No desktop Qt kit was selected. '$subdir' is not supported. WASM kits target browsers."
        }
        $selection = Find-QtUnderRoot -Root $Root -Subdir $subdir
        if ($selection) {
            return $selection
        }
    }

    $installed = Get-InstalledQtSubdir -Root $Root
    if ($installed | Where-Object { $_ -like "wasm*" }) {
        throw "No desktop Qt kit was found under $Root. WASM kits target browsers and cannot build this desktop application."
    }
    return $null
}

function Find-CompilerInQtTool {
    param(
        [string]$Root,
        [string]$DirectoryPattern,
        [string]$Executable
    )

    $toolsRoot = Join-Path $Root "Tools"
    if (-not (Test-Path $toolsRoot)) {
        return $null
    }
    foreach ($toolDir in Get-ChildItem $toolsRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like $DirectoryPattern } |
        Sort-Object -Property Name -Descending) {
        $candidate = Join-Path $toolDir.FullName "bin/$Executable"
        if (Test-Path $candidate) {
            return $candidate
        }
    }
    return $null
}

function Test-MinGwCompilerTarget {
    param([string]$Cxx)

    try {
        $target = & $Cxx -dumpmachine 2>$null | Select-Object -First 1
    } catch {
        Write-Verbose "Compiler target probe failed for ${Cxx}: $($_.Exception.Message)"
        return $false
    }
    if ($LASTEXITCODE -ne 0) {
        Write-Verbose "Compiler target probe returned exit code $LASTEXITCODE for $Cxx."
        return $false
    }
    return $target -match '(mingw|windows-gnu)'
}

function Get-CompilerForQtSelection {
    param(
        [object]$Selection,
        [string]$Root
    )

    if ($Selection.Subdir -like "msvc*") {
        $cl = Get-Command "cl.exe" -ErrorAction SilentlyContinue
        if ($cl) {
            return [PSCustomObject]@{ C = ""; Cxx = ""; BinDir = ""; Name = "MSVC" }
        }
        return $null
    }

    $isLlvm = $Selection.Subdir -like "llvm-mingw*"
    $cName = if ($isLlvm) { "clang.exe" } else { "gcc.exe" }
    $cxxName = if ($isLlvm) { "clang++.exe" } else { "g++.exe" }
    $directoryPattern = if ($isLlvm) { "llvm*" } else { "mingw*" }
    $c = Find-CompilerInQtTool -Root $Root -DirectoryPattern $directoryPattern -Executable $cName
    $cxx = Find-CompilerInQtTool -Root $Root -DirectoryPattern $directoryPattern -Executable $cxxName
    if (-not $c -or -not $cxx) {
        $cCommand = Get-Command $cName -ErrorAction SilentlyContinue
        $cxxCommand = Get-Command $cxxName -ErrorAction SilentlyContinue
        $c = if ($cCommand) { $cCommand.Source } else { $null }
        $cxx = if ($cxxCommand) { $cxxCommand.Source } else { $null }
    }
    if (-not $c -or -not $cxx) {
        return $null
    }
    if (-not (Test-MinGwCompilerTarget -Cxx $cxx)) {
        return $null
    }

    return [PSCustomObject]@{
        C = $c
        Cxx = $cxx
        BinDir = Split-Path $cxx -Parent
        Name = if ($isLlvm) { "LLVM MinGW" } else { "MinGW" }
    }
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
    # Honrar triplet explicito (env ou -DVCPKG_TARGET_TRIPLET via CmakeExtraArgs)
    # antes de derivar do PROCESSOR_ARCHITECTURE do host, que pode divergir em
    # cross-compile (ex.: arm64 pedido em host x64).
    if ($env:VCPKG_TARGET_TRIPLET) {
        return $env:VCPKG_TARGET_TRIPLET
    }
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
    $vcpkgTripletPath = Join-Path "installed" $triplet
    $paths = @()
    if ($env:VCPKG_ROOT) {
        $paths += Join-Path $env:VCPKG_ROOT $vcpkgTripletPath
    }
    $paths += Join-Path "C:\vcpkg" $vcpkgTripletPath
    return Find-SqliteFromKnownPath $paths
}

function Find-SqliteRoot {
    param([string]$ExplicitSQLiteRoot)

    $normalizedExplicit = ConvertTo-NormalizedPath $ExplicitSQLiteRoot
    if ($normalizedExplicit -and (Test-SqlitePrefix $normalizedExplicit)) {
        return $normalizedExplicit
    }

    return Find-SqliteFromKnownPath @(
        $env:SQLite3_ROOT,
        $env:SQLITE_ROOT,
        (Find-SqliteFromVcpkg)
    )
}

function Confirm-ClangFormat {
    if (Get-Command clang-format -ErrorAction SilentlyContinue) {
        return
    }

    $candidatePaths = @(
        "C:\Program Files\LLVM\bin",
        "$env:ProgramFiles\LLVM\bin",
        "$env:ProgramFiles(x86)\LLVM\bin"
    )
    foreach ($candidate in $candidatePaths) {
        if ($candidate -and (Test-Path $candidate)) {
            $env:Path = "$candidate;$env:Path"
            break
        }
    }

    if (Get-Command clang-format -ErrorAction SilentlyContinue) {
        Write-Output "clang-format found in session PATH."
        return
    }

    Write-Warning "clang-format not found in PATH."
    Write-Output "Install with:"
    Write-Output "  winget install --id LLVM.LLVM"
    Write-Output "Add C:\Program Files\LLVM\bin to PATH for this session and profile if needed."
}

$effectiveQtRoot = if ($QtRoot) {
    ConvertTo-NormalizedPath $QtRoot
} elseif ($env:QT_INSTALL_ROOT) {
    ConvertTo-NormalizedPath $env:QT_INSTALL_ROOT
} else {
    "C:\Qt"
}
$selection = Find-QtSelection -ExplicitQtDir $QtDir -Root $effectiveQtRoot -RequestedSubdir $QtSubdir
if (-not $selection) {
    throw @"
Qt $($DetectConfig.QT_VERSION_FAMILY).x was not detected under $effectiveQtRoot.
Desktop kit priority:
  $((Get-DesktopQtSubdir) -join ', ')
You can run:
  .\tools\configure-dev.ps1 -QtSubdir llvm-mingw_64
"@
}

$installedSubdirs = Get-InstalledQtSubdir -Root $effectiveQtRoot
if ($PrintQtSelection) {
    Write-Output "QtVersion=$($selection.Version.ToString())"
    Write-Output "QtSubdir=$($selection.Subdir)"
    Write-Output "QtDir=$($selection.Path)"
    Write-Output "AvailableQtSubdirs=$($installedSubdirs -join ',')"
    return
}

Test-RequiredCommand "cmake" "Install CMake 3.24 or newer and add it to PATH."
Test-RequiredCommand "ninja" "Install Ninja and add it to PATH."

$compiler = Get-CompilerForQtSelection -Selection $selection -Root $effectiveQtRoot
if (-not $compiler -and $selection.Source -eq "automatic" -and -not $QtSubdir) {
    foreach ($fallbackSubdir in Get-DesktopQtSubdir) {
        $fallbackSelection = Find-QtUnderRoot -Root $effectiveQtRoot -Subdir $fallbackSubdir
        if (-not $fallbackSelection) {
            continue
        }
        $fallbackCompiler = Get-CompilerForQtSelection -Selection $fallbackSelection -Root $effectiveQtRoot
        if ($fallbackCompiler) {
            $selection = $fallbackSelection
            $compiler = $fallbackCompiler
            break
        }
    }
}
if (-not $compiler) {
    throw "Compiler for Qt kit '$($selection.Subdir)' was not found. Use Developer PowerShell for MSVC or pass -QtSubdir llvm-mingw_64 or mingw_64."
}

Write-Output "Target Qt family: $($DetectConfig.QT_VERSION_FAMILY).x (reference patch: $($DetectConfig.QT_VERSION))"
Write-Output "Using Qt version: $($selection.Version.ToString())"
Write-Output "Using Qt kit: $($selection.Subdir)"
Write-Output "Using Qt prefix: $($selection.Path)"
if ($installedSubdirs.Count -gt 1) {
    Write-Output "Detected Qt kits: $($installedSubdirs -join ', ')"
    Write-Output "Select another desktop kit with -QtSubdir <name>. WASM kits target browsers."
}
Write-Output "Using compiler: $($compiler.Name)"
if ($compiler.BinDir) {
    $env:Path = "$($compiler.BinDir);$env:Path"
}
$cmakeArgs = @(
    "--preset", $Preset,
    "-UQt6*_DIR",
    "-U*DEPLOYQT_EXECUTABLE",
    "-DCMAKE_PREFIX_PATH=$($selection.Path)",
    "-DQt6_DIR=$(Join-Path $selection.Path 'lib/cmake/Qt6')"
)
if ($compiler.C) {
    $cmakeArgs += "-DCMAKE_C_COMPILER=$($compiler.C)"
    $cmakeArgs += "-DCMAKE_CXX_COMPILER=$($compiler.Cxx)"
}

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

if ($CmakeExtraArgs -and $CmakeExtraArgs.Count -gt 0) {
    Write-Output "Appending extra CMake args: $($CmakeExtraArgs -join ' ')"
    $cmakeArgs += $CmakeExtraArgs
}

cmake @cmakeArgs
Confirm-ClangFormat
