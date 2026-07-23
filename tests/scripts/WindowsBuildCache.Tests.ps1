$ErrorActionPreference = "Stop"

Describe "Windows build cache ownership" {
    BeforeEach {
        $testRoot = (Get-PSDrive -Name TestDrive).Root
        $script:repoRoot = Join-Path $testRoot "repo"
        $script:toolsDir = Join-Path $script:repoRoot "tools"
        $script:buildDir = Join-Path $script:repoRoot "build/dev"
        $script:fakeBin = Join-Path $testRoot "bin"
        $script:configureMarker = Join-Path $testRoot "configure-called.txt"
        $script:cmakeLog = Join-Path $testRoot "cmake-called.txt"
        $script:pathLog = Join-Path $testRoot "path-used.txt"
        $script:originalPath = $env:Path
        $script:originalPathExt = $env:PATHEXT
        $script:buildScript = Join-Path (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path "scripts/build-windows.ps1"

        Remove-Item -LiteralPath $script:configureMarker, $script:cmakeLog, $script:pathLog -Force -ErrorAction SilentlyContinue

        New-Item -ItemType Directory -Path $script:toolsDir, $script:buildDir, $script:fakeBin -Force | Out-Null

        @'
param(
    [string]$Preset,
    [string]$QtDir,
    [string]$QtRoot,
    [string]$QtSubdir,
    [string]$SQLiteRoot,
    [string[]]$CmakeExtraArgs
)
Set-Content -LiteralPath $env:SSA_TEST_CONFIGURE_MARKER -Value "$Preset|$($CmakeExtraArgs -join ',')|$QtSubdir"
'@ | Set-Content -LiteralPath (Join-Path $script:toolsDir "configure-dev.ps1") -Encoding ASCII

        @'
@echo off
echo %*>>"%SSA_TEST_CMAKE_LOG%"
echo %PATH%>"%SSA_TEST_PATH_LOG%"
exit /b 0
'@ | Set-Content -LiteralPath (Join-Path $script:fakeBin "cmake.cmd") -Encoding ASCII
        New-Item -ItemType File -Path (Join-Path $script:fakeBin "cl.exe") -Force | Out-Null

        $env:SSA_TEST_CONFIGURE_MARKER = $script:configureMarker
        $env:SSA_TEST_CMAKE_LOG = $script:cmakeLog
        $env:SSA_TEST_PATH_LOG = $script:pathLog
        $env:Path = "$script:fakeBin;$script:originalPath"
        $env:PATHEXT = ".COM;.EXE;.BAT;.CMD"
    }

    AfterEach {
        $env:Path = $script:originalPath
        $env:PATHEXT = $script:originalPathExt
        Remove-Item Env:SSA_TEST_CONFIGURE_MARKER, Env:SSA_TEST_CMAKE_LOG, Env:SSA_TEST_PATH_LOG -ErrorAction SilentlyContinue
    }

    It "reconfigures when the existing cache belongs to WSL" {
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=/mnt/c/Users/mauri/project/build/dev"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=/usr/bin/ninja"
        ) | Set-Content -LiteralPath (Join-Path $script:buildDir "CMakeCache.txt") -Encoding ASCII

        & $script:buildScript -ProjectRoot $script:repoRoot

        (Test-Path -LiteralPath $script:configureMarker -PathType Leaf) | Should Be $true
        (Get-Content -LiteralPath $script:configureMarker) | Should Be "dev|--fresh|msvc2022_64"
        (Get-Content -LiteralPath $script:cmakeLog) | Should Match "--build --preset dev"
    }

    It "reconfigures when a Windows cache has no generated Ninja files" {
        $windowsBuildDir = $script:buildDir.Replace('\', '/')
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=$windowsBuildDir"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=C:/Qt/Tools/Ninja/ninja.exe"
        ) | Set-Content -LiteralPath (Join-Path $script:buildDir "CMakeCache.txt") -Encoding ASCII

        & $script:buildScript -ProjectRoot $script:repoRoot

        (Test-Path -LiteralPath $script:configureMarker -PathType Leaf) | Should Be $true
        (Get-Content -LiteralPath $script:configureMarker) | Should Be "dev|--fresh|msvc2022_64"
    }

    It "adds the linked vcpkg SQLite runtime before invoking CMake" {
        $sqliteRoot = Join-Path $testRoot "vcpkg/installed/x64-windows"
        $sqliteLibrary = Join-Path $sqliteRoot "lib/sqlite3.lib"
        $sqliteBin = Join-Path $sqliteRoot "bin"
        $windowsBuildDir = $script:buildDir.Replace('\', '/')
        $windowsSqliteLibrary = $sqliteLibrary.Replace('\', '/')
        $qtPrefix = Join-Path $testRoot "Qt/6.11.1/mingw_64"
        $qtCmakeDirectory = Join-Path $qtPrefix "lib/cmake/Qt6"
        $windowsQtDirectory = $qtCmakeDirectory.Replace('\', '/')

        New-Item -ItemType Directory -Path (Split-Path -Parent $sqliteLibrary), $sqliteBin, (Join-Path $qtPrefix "bin"), (Join-Path $script:buildDir "CMakeFiles") -Force | Out-Null
        New-Item -ItemType File -Path $sqliteLibrary, (Join-Path $sqliteBin "sqlite3.dll"), (Join-Path $qtPrefix "bin/Qt6Core.dll"), (Join-Path $script:buildDir "build.ninja"), (Join-Path $script:buildDir "CMakeFiles/rules.ninja") -Force | Out-Null
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=$windowsBuildDir"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=C:/Qt/Tools/Ninja/ninja.exe"
            "CMAKE_CXX_COMPILER:FILEPATH=C:/Qt/Tools/mingw1310_64/bin/g++.exe"
            "Qt6_DIR:PATH=$windowsQtDirectory"
            "SQLite3_LIBRARY:FILEPATH=$windowsSqliteLibrary"
        ) | Set-Content -LiteralPath (Join-Path $script:buildDir "CMakeCache.txt") -Encoding ASCII

        & $script:buildScript -ProjectRoot $script:repoRoot

        (Test-Path -LiteralPath $script:configureMarker -PathType Leaf) | Should Be $false
        $buildPath = Get-Content -LiteralPath $script:pathLog
        $buildPath | Should Match ('^' + [regex]::Escape("$sqliteBin;"))
        $buildPath | Should Match ([regex]::Escape((Join-Path $qtPrefix "bin")))
    }

    It "restores the machine PATHEXT before invoking native tools" {
        $env:PATHEXT = ".CPL"
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=/mnt/c/Users/mauri/project/build/dev"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=/usr/bin/ninja"
        ) | Set-Content -LiteralPath (Join-Path $script:buildDir "CMakeCache.txt") -Encoding ASCII

        & $script:buildScript -ProjectRoot $script:repoRoot

        (Get-Content -LiteralPath $script:configureMarker) | Should Be "dev|--fresh|msvc2022_64"
        (Get-Content -LiteralPath $script:cmakeLog) | Should Match "--build --preset dev"
        $env:PATHEXT | Should Match "\.EXE"
    }

    It "refreshes when an explicit Qt kit differs from the cached kit" {
        $windowsBuildDir = $script:buildDir.Replace('\', '/')
        New-Item -ItemType Directory -Path (Join-Path $script:buildDir "CMakeFiles") -Force | Out-Null
        New-Item -ItemType File -Path (Join-Path $script:buildDir "build.ninja"), (Join-Path $script:buildDir "CMakeFiles/rules.ninja") -Force | Out-Null
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=$windowsBuildDir"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=C:/Qt/Tools/Ninja/ninja.exe"
            "CMAKE_CXX_COMPILER:FILEPATH=C:/Qt/Tools/mingw1310_64/bin/g++.exe"
            "Qt6_DIR:PATH=C:/Qt/6.11.1/mingw_64/lib/cmake/Qt6"
        ) | Set-Content -LiteralPath (Join-Path $script:buildDir "CMakeCache.txt") -Encoding ASCII

        & $script:buildScript -ProjectRoot $script:repoRoot -QtSubdir msvc2022_64

        (Get-Content -LiteralPath $script:configureMarker) | Should Be "dev|--fresh|msvc2022_64"
    }
}
