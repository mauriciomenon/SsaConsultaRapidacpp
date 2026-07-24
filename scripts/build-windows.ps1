[CmdletBinding()]
param(
    [string]$Preset = "dev",
    [string]$Arch = "",
    [string]$QtDir = "",
    [string]$QtRoot = "",
    [string]$QtSubdir = "",
    [string]$SQLiteRoot = "",
    [string]$ProjectRoot = "",
    [string]$Target = "",
    [string[]]$CmakeExtraArgs = @(),
    [switch]$ReuseBuild,
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = if ($ProjectRoot) { (Resolve-Path $ProjectRoot).Path } else { (Resolve-Path (Join-Path $scriptDir "..")).Path }
$lazyBuildScript = (Resolve-Path (Join-Path $scriptDir "lazy_scripts\build-windows.ps1")).Path
$callerScript = if ($MyInvocation.ScriptName) {
    [IO.Path]::GetFullPath($MyInvocation.ScriptName)
} else {
    ""
}
if ($ReuseBuild -and
    -not $callerScript.Equals($lazyBuildScript, [StringComparison]::OrdinalIgnoreCase)) {
    throw "ReuseBuild is internal. Use scripts\lazy_scripts\build-windows.ps1."
}
. (Join-Path $scriptDir "lib\windows_build_layout.ps1")
$configureScript = Join-Path $repoRoot "tools\configure-dev.ps1"
$preset = if ($Preset) { $Preset } else { "dev" }
$layout = Resolve-WindowsBuildLayout -RepoRoot $repoRoot -Preset $preset -Arch $Arch -QtDir $QtDir -QtSubdir $QtSubdir
$arch = $layout.Arch
$qtKit = $layout.QtKit
$buildDir = $layout.BuildDir
$cacheFile = Join-Path $buildDir "CMakeCache.txt"
$effectiveCmakeExtraArgs = @($CmakeExtraArgs | Where-Object { $_ })
$defaultTriplet = if ($arch -eq 'arm64') { 'arm64-windows' } else { 'x64-windows' }
$tripletPattern = if ($arch -eq 'arm64') { '^arm64-windows(?:-|$)' } else { '^x64-windows(?:-|$)' }
$tripletArguments = @($effectiveCmakeExtraArgs | Where-Object { $_ -match '^-DVCPKG_TARGET_TRIPLET=(.+)$' })
if ($tripletArguments.Count -gt 0) {
    $targetTriplet = $tripletArguments[-1].Substring($tripletArguments[-1].IndexOf('=') + 1)
    if ($targetTriplet -notmatch $tripletPattern) {
        throw "vcpkg triplet '$targetTriplet' is incompatible with Windows architecture '$arch'."
    }
} else {
    $targetTriplet = $defaultTriplet
    $effectiveCmakeExtraArgs += "-DVCPKG_TARGET_TRIPLET=$targetTriplet"
}

if ($Help) {
    Write-Output @"
Usage:
  .\scripts\build-windows.ps1

Build the Windows target (default preset: dev).

Defaults:
  Build mode: clean. Use scripts\lazy_scripts\build-windows.ps1 for incremental builds.
  Preset: dev
  Architecture: host architecture, or amd64 when Windows reports x64
  Qt kit: msvc2022_64 for amd64; msvc2022_arm64 for arm64
  Repository root: directory that contains this script.

Explicit options can be used through:
  .\scripts\build-windows.ps1 -Preset <preset> [-Arch <amd64|arm64>] [-QtDir <qt-dir>] [-QtRoot <root>] [-QtSubdir <kit>] [-SQLiteRoot <path>] [-Target <target>] [-CmakeExtraArgs <args>]

Qt kit examples:
  .\scripts\build-windows.ps1
  .\scripts\build-windows.ps1 -QtSubdir llvm-mingw_64
  .\scripts\build-windows.ps1 -QtSubdir mingw_64
"@
    return
}

if (-not $ReuseBuild -and (Test-Path -LiteralPath $buildDir -PathType Container)) {
    Write-Output "Removing previous Windows build: $buildDir"
    Remove-Item -LiteralPath $buildDir -Recurse -Force
}

$processPathExt = $env:PATHEXT -split ';'
if (-not ($processPathExt -contains '.EXE')) {
    $machinePathExt = [Environment]::GetEnvironmentVariable('PATHEXT', 'Machine')
    if (-not $machinePathExt -or -not (($machinePathExt -split ';') -contains '.EXE')) {
        throw 'PATHEXT does not contain .EXE in the process or Machine environment.'
    }
    $env:PATHEXT = $machinePathExt
}

$cmakeExecutable = $null
foreach ($pathEntry in ($env:Path -split ';')) {
    foreach ($cmakeName in @('cmake.exe', 'cmake.cmd')) {
        $candidate = Join-Path $pathEntry $cmakeName
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $cmakeExecutable = $candidate
            break
        }
    }
    if ($cmakeExecutable) {
        break
    }
}
if (-not $cmakeExecutable) {
    throw 'CMake was not found in PATH.'
}

$cachedCompiler = ''
if (Test-Path -LiteralPath $cacheFile -PathType Leaf) {
    $compilerLine = Select-String -LiteralPath $cacheFile -Pattern '^CMAKE_CXX_COMPILER:[^=]+=(.+)$' |
        Select-Object -First 1
    if ($compilerLine) {
        $cachedCompiler = $compilerLine.Matches[0].Groups[1].Value.Trim()
    }
}
$explicitMsvcKit = $qtKit -like 'msvc*'

$cacheIsReusable = $false
if (Test-Path -LiteralPath $cacheFile -PathType Leaf) {
    $cacheDirectoryLine = Select-String -LiteralPath $cacheFile -Pattern '^CMAKE_CACHEFILE_DIR:INTERNAL=(.+)$' |
        Select-Object -First 1
    $generatorLine = Select-String -LiteralPath $cacheFile -Pattern '^CMAKE_GENERATOR:INTERNAL=(.+)$' |
        Select-Object -First 1
    $makeProgramLine = Select-String -LiteralPath $cacheFile -Pattern '^CMAKE_MAKE_PROGRAM:FILEPATH=(.+)$' |
        Select-Object -First 1
    if ($cacheDirectoryLine -and $generatorLine -and $makeProgramLine) {
        $cachedBuildDir = $cacheDirectoryLine.Matches[0].Groups[1].Value.Trim().Replace('\', '/')
        $expectedBuildDir = $buildDir.Replace('\', '/')
        $generator = $generatorLine.Matches[0].Groups[1].Value.Trim()
        $makeProgram = $makeProgramLine.Matches[0].Groups[1].Value.Trim()
        $cacheIsReusable = $cachedBuildDir.Equals($expectedBuildDir, [StringComparison]::OrdinalIgnoreCase) -and
            $generator -eq 'Ninja' -and $makeProgram -match '(?i)[\\/]ninja\.exe$' -and
            (Test-Path -LiteralPath (Join-Path $buildDir 'build.ninja') -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $buildDir 'CMakeFiles/rules.ninja') -PathType Leaf)
    }
}

if ($cacheIsReusable) {
    $cachedQtDirectoryLine = Select-String -LiteralPath $cacheFile -Pattern '^Qt6_DIR:[^=]+=(.+)$' |
        Select-Object -First 1
    $cachedQtDirectory = if ($cachedQtDirectoryLine) {
        $cachedQtDirectoryLine.Matches[0].Groups[1].Value.Trim().Replace('\', '/')
    } else {
        ''
    }
    $requestedQtMismatch = if ($QtDir) {
        $requestedQtPrefix = $QtDir.Replace('\', '/').TrimEnd('/')
        -not $cachedQtDirectory.StartsWith("$requestedQtPrefix/", [StringComparison]::OrdinalIgnoreCase)
    } else {
        $cachedQtDirectory -notmatch "(?i)/$([regex]::Escape($qtKit))/"
    }
    if ($requestedQtMismatch) {
        $cacheIsReusable = $false
    }
}
if ($cacheIsReusable) {
    $cachedTripletLine = Select-String -LiteralPath $cacheFile -Pattern '^VCPKG_TARGET_TRIPLET:[^=]+=(.+)$' |
        Select-Object -First 1
    $cachedTriplet = if ($cachedTripletLine) {
        $cachedTripletLine.Matches[0].Groups[1].Value.Trim()
    } else {
        ''
    }
    if (-not $cachedTriplet.Equals($targetTriplet, [StringComparison]::OrdinalIgnoreCase)) {
        $cacheIsReusable = $false
    }
}

$defaultMsvcKit = -not $QtSubdir -and -not $QtDir -and
    (-not $cacheIsReusable -or $cachedCompiler -match '(?i)[\\/]cl\.exe$')
if (($explicitMsvcKit -or $defaultMsvcKit) -and -not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $devShellPattern = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\*\*\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
    $devShellModule = Get-ChildItem -Path $devShellPattern -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $devShellModule) {
        throw 'MSVC was selected, but the Visual Studio Developer PowerShell module was not found.'
    }
    $vsInstallPath = $devShellModule.Directory.Parent.Parent.FullName
    Import-Module $devShellModule.FullName -ErrorAction Stop
    $vsTargetArch = if ($arch -eq 'arm64') { 'arm64' } else { 'amd64' }
    $expectedTargetArch = if ($arch -eq 'arm64') { 'arm64' } else { 'x64' }
    Enter-VsDevShell -VsInstallPath $vsInstallPath -SkipAutomaticLocation -Arch $vsTargetArch -HostArch amd64
    if ($env:VSCMD_ARG_HOST_ARCH -ne 'x64' -or $env:VSCMD_ARG_TGT_ARCH -ne $expectedTargetArch -or
        -not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw "MSVC Developer environment initialization failed: host=$env:VSCMD_ARG_HOST_ARCH target=$env:VSCMD_ARG_TGT_ARCH"
    }
}

$configureParams = @{
    Preset = $preset
    BinaryDir = $buildDir
    QtSubdir = $qtKit
}
if ($QtDir) { $configureParams.QtDir = $QtDir }
if ($QtRoot) { $configureParams.QtRoot = $QtRoot }
if ($SQLiteRoot) { $configureParams.SQLiteRoot = $SQLiteRoot }
if ((Test-Path -LiteralPath $cacheFile -PathType Leaf) -and -not $cacheIsReusable) {
    Write-Output "Refreshing foreign or stale CMake cache: $cacheFile"
    if ($effectiveCmakeExtraArgs -notcontains '--fresh') {
        $effectiveCmakeExtraArgs = @('--fresh') + $effectiveCmakeExtraArgs
    }
}
if ($effectiveCmakeExtraArgs.Count -gt 0) {
    $configureParams.CmakeExtraArgs = $effectiveCmakeExtraArgs
}

$configurationRequested = $QtDir -or $QtRoot -or $QtSubdir -or $SQLiteRoot -or
    @($CmakeExtraArgs | Where-Object { $_ }).Count -gt 0
if ($configurationRequested -or -not $cacheIsReusable) {
    & $configureScript @configureParams
    if (-not (Test-Path -LiteralPath $cacheFile -PathType Leaf)) {
        throw "CMake configuration did not produce expected cache: $cacheFile"
    }
}

$qtDirectoryLine = Select-String -LiteralPath $cacheFile -Pattern '^Qt6_DIR:[^=]+=(.+)$' |
    Select-Object -First 1
if ($qtDirectoryLine) {
    $qtCmakeDirectory = $qtDirectoryLine.Matches[0].Groups[1].Value.Trim()
    $qtPrefix = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $qtCmakeDirectory))
    $qtBin = Join-Path $qtPrefix 'bin'
    if (Test-Path -LiteralPath (Join-Path $qtBin 'Qt6Core.dll') -PathType Leaf) {
        $remainingPath = @($env:Path -split ';' | Where-Object {
                $_ -and -not $_.Equals($qtBin, [StringComparison]::OrdinalIgnoreCase)
            })
        $env:Path = (@($qtBin) + $remainingPath) -join ';'
    }
}

$sqliteLibraryLine = Select-String -LiteralPath $cacheFile -Pattern '^SQLite3_LIBRARY:[^=]+=(.+)$' |
    Select-Object -First 1
if ($sqliteLibraryLine) {
    $sqliteLibrary = $sqliteLibraryLine.Matches[0].Groups[1].Value.Trim()
    $sqliteRoot = Split-Path -Parent (Split-Path -Parent $sqliteLibrary)
    $sqliteBin = Join-Path $sqliteRoot 'bin'
    if (Test-Path -LiteralPath (Join-Path $sqliteBin 'sqlite3.dll') -PathType Leaf) {
        $remainingPath = @($env:Path -split ';' | Where-Object {
                $_ -and -not $_.Equals($sqliteBin, [StringComparison]::OrdinalIgnoreCase)
            })
        $env:Path = (@($sqliteBin) + $remainingPath) -join ';'
    }
}

$buildArgs = @("--build", $buildDir)
if ($Target) {
    $buildArgs += @("--target", $Target)
}
& $cmakeExecutable @buildArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed with exit code $LASTEXITCODE."
}
