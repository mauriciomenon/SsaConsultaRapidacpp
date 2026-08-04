$ErrorActionPreference = "Stop"

Describe "Windows build cache ownership" {
    BeforeEach {
        $testRoot = (Get-PSDrive -Name TestDrive).Root
        $script:repoRoot = Join-Path $testRoot "repo"
        $script:toolsDir = Join-Path $script:repoRoot "tools"
        $script:buildDir = Join-Path $script:repoRoot "build/windows/amd64/msvc/msvc2022_64/dev"
        $script:llvmBuildDir = Join-Path $script:repoRoot "build/windows/amd64/llvm/msvc2022_64/dev"
        $script:fakeBin = Join-Path $testRoot "bin"
        $script:configureMarker = Join-Path $testRoot "configure-called.txt"
        $script:configureBinaryMarker = Join-Path $testRoot "configure-binary-dir.txt"
        $script:cmakeLog = Join-Path $testRoot "cmake-called.txt"
        $script:pathLog = Join-Path $testRoot "path-used.txt"
        $script:originalPath = $env:Path
        $script:originalPathExt = $env:PATHEXT
        $script:originalGuardTestRoot = $env:SSA_NATIVE_GUARD_TEST_ROOT
        $script:buildScript = Join-Path (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path "scripts/build-windows.ps1"
        $script:lazyBuildScript = Join-Path (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path "scripts/lazy_scripts/build-windows.ps1"

        Remove-Item -LiteralPath $script:repoRoot -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $script:configureMarker, $script:configureBinaryMarker, $script:cmakeLog, $script:pathLog -Force -ErrorAction SilentlyContinue

        New-Item -ItemType Directory -Path $script:toolsDir, $script:buildDir, $script:llvmBuildDir, $script:fakeBin -Force | Out-Null

        @'
param(
    [string]$Preset,
    [string]$QtDir,
    [string]$QtRoot,
    [string]$QtSubdir,
    [string]$SQLiteRoot,
    [string]$BinaryDir,
    [string[]]$CmakeExtraArgs
)
Set-Content -LiteralPath $env:SSA_TEST_CONFIGURE_MARKER -Value "$Preset|$($CmakeExtraArgs -join ',')|$QtSubdir"
Set-Content -LiteralPath $env:SSA_TEST_CONFIGURE_BINARY_MARKER -Value $BinaryDir
if (-not $env:SSA_TEST_SKIP_CONFIGURE_CACHE) {
    New-Item -ItemType Directory -Path $BinaryDir -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $BinaryDir "CMakeCache.txt") -Value "" -Encoding ASCII
}
'@ | Set-Content -LiteralPath (Join-Path $script:toolsDir "configure-dev.ps1") -Encoding ASCII

        @'
@echo off
echo %*>>"%SSA_TEST_CMAKE_LOG%"
echo %PATH%>"%SSA_TEST_PATH_LOG%"
exit /b 0
'@ | Set-Content -LiteralPath (Join-Path $script:fakeBin "cmake.cmd") -Encoding ASCII
        New-Item -ItemType File -Path (Join-Path $script:fakeBin "cl.exe"), (Join-Path $script:fakeBin "clang-cl.exe"), (Join-Path $script:fakeBin "lld-link.exe") -Force | Out-Null

        $env:SSA_TEST_CONFIGURE_MARKER = $script:configureMarker
        $env:SSA_TEST_CONFIGURE_BINARY_MARKER = $script:configureBinaryMarker
        $env:SSA_TEST_CMAKE_LOG = $script:cmakeLog
        $env:SSA_TEST_PATH_LOG = $script:pathLog
        $env:Path = "$script:fakeBin;$script:originalPath"
        $env:PATHEXT = ".COM;.EXE;.BAT;.CMD"
        $env:SSA_NATIVE_GUARD_TEST_ROOT = $testRoot
    }

    AfterEach {
        $env:Path = $script:originalPath
        $env:PATHEXT = $script:originalPathExt
        $env:SSA_NATIVE_GUARD_TEST_ROOT = $script:originalGuardTestRoot
        Remove-Item Env:SSA_TEST_CONFIGURE_MARKER, Env:SSA_TEST_CONFIGURE_BINARY_MARKER, Env:SSA_TEST_CMAKE_LOG, Env:SSA_TEST_PATH_LOG -ErrorAction SilentlyContinue
    }

    It "reconfigures when the existing cache belongs to WSL" {
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=/mnt/c/Users/example/project/build/dev"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=/usr/bin/ninja"
        ) | Set-Content -LiteralPath (Join-Path $script:buildDir "CMakeCache.txt") -Encoding ASCII

        & $script:lazyBuildScript -ProjectRoot $script:repoRoot

        (Test-Path -LiteralPath $script:configureMarker -PathType Leaf) | Should -BeTrue
        (Get-Content -LiteralPath $script:configureMarker) | Should -Be "dev|--fresh,-DVCPKG_TARGET_TRIPLET=x64-windows|msvc2022_64"
        (Get-Content -LiteralPath $script:cmakeLog) | Should -Match ([regex]::Escape("--build $script:buildDir"))
    }

    It "reconfigures when a Windows cache has no generated Ninja files" {
        $windowsBuildDir = $script:buildDir.Replace('\', '/')
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=$windowsBuildDir"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=C:/Qt/Tools/Ninja/ninja.exe"
        ) | Set-Content -LiteralPath (Join-Path $script:buildDir "CMakeCache.txt") -Encoding ASCII

        & $script:lazyBuildScript -ProjectRoot $script:repoRoot

        (Test-Path -LiteralPath $script:configureMarker -PathType Leaf) | Should -BeTrue
        (Get-Content -LiteralPath $script:configureMarker) | Should -Be "dev|--fresh,-DVCPKG_TARGET_TRIPLET=x64-windows|msvc2022_64"
    }

    It "adds the linked vcpkg SQLite runtime before invoking CMake" {
        $sqliteRoot = Join-Path $testRoot "vcpkg/installed/x64-windows"
        $sqliteLibrary = Join-Path $sqliteRoot "lib/sqlite3.lib"
        $sqliteBin = Join-Path $sqliteRoot "bin"
        $windowsBuildDir = $script:buildDir.Replace('\', '/')
        $windowsSqliteLibrary = $sqliteLibrary.Replace('\', '/')
        $qtPrefix = Join-Path $testRoot "Qt/6.11.1/msvc2022_64"
        $qtCmakeDirectory = Join-Path $qtPrefix "lib/cmake/Qt6"
        $windowsQtDirectory = $qtCmakeDirectory.Replace('\', '/')

        New-Item -ItemType Directory -Path (Split-Path -Parent $sqliteLibrary), $sqliteBin, (Join-Path $qtPrefix "bin"), (Join-Path $script:buildDir "CMakeFiles") -Force | Out-Null
        New-Item -ItemType File -Path $sqliteLibrary, (Join-Path $sqliteBin "sqlite3.dll"), (Join-Path $qtPrefix "bin/Qt6Core.dll"), (Join-Path $script:buildDir "build.ninja"), (Join-Path $script:buildDir "CMakeFiles/rules.ninja") -Force | Out-Null
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=$windowsBuildDir"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=C:/Qt/Tools/Ninja/ninja.exe"
            "CMAKE_CXX_COMPILER:FILEPATH=$($script:fakeBin.Replace('\\', '/'))/cl.exe"
            "Qt6_DIR:PATH=$windowsQtDirectory"
            "VCPKG_TARGET_TRIPLET:STRING=x64-windows"
            "SQLite3_LIBRARY:FILEPATH=$windowsSqliteLibrary"
        ) | Set-Content -LiteralPath (Join-Path $script:buildDir "CMakeCache.txt") -Encoding ASCII

        & $script:lazyBuildScript -ProjectRoot $script:repoRoot

        (Test-Path -LiteralPath $script:configureMarker -PathType Leaf) | Should -BeFalse
        $buildPath = Get-Content -LiteralPath $script:pathLog
        $buildPath | Should -Match ('^' + [regex]::Escape("$sqliteBin;"))
        $buildPath | Should -Match ([regex]::Escape((Join-Path $qtPrefix "bin")))
    }

    It "restores the machine PATHEXT before invoking native tools" {
        $env:PATHEXT = ".CPL"
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=/mnt/c/Users/example/project/build/dev"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=/usr/bin/ninja"
        ) | Set-Content -LiteralPath (Join-Path $script:buildDir "CMakeCache.txt") -Encoding ASCII

        & $script:lazyBuildScript -ProjectRoot $script:repoRoot

        (Get-Content -LiteralPath $script:configureMarker) | Should -Be "dev|--fresh,-DVCPKG_TARGET_TRIPLET=x64-windows|msvc2022_64"
        (Get-Content -LiteralPath $script:cmakeLog) | Should -Match ([regex]::Escape("--build $script:buildDir"))
        $env:PATHEXT | Should -Match "\.EXE"
    }

    It "reuses the default amd64 MSVC cache" {
        $windowsBuildDir = $script:buildDir.Replace('\', '/')
        New-Item -ItemType Directory -Path (Join-Path $script:buildDir "CMakeFiles") -Force | Out-Null
        New-Item -ItemType File -Path (Join-Path $script:buildDir "build.ninja"), (Join-Path $script:buildDir "CMakeFiles/rules.ninja") -Force | Out-Null
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=$windowsBuildDir"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=C:/Qt/Tools/Ninja/ninja.exe"
            "CMAKE_CXX_COMPILER:FILEPATH=$($script:fakeBin.Replace('\', '/'))/cl.exe"
            "Qt6_DIR:PATH=C:/Qt/6.11.1/msvc2022_64/lib/cmake/Qt6"
            "VCPKG_TARGET_TRIPLET:STRING=x64-windows"
        ) | Set-Content -LiteralPath (Join-Path $script:buildDir "CMakeCache.txt") -Encoding ASCII

        & $script:lazyBuildScript -ProjectRoot $script:repoRoot

        (Test-Path -LiteralPath $script:configureMarker -PathType Leaf) | Should -BeFalse
    }

    It "configures LLVM with clang-cl and lld-link outside the MSVC cache" {
        $windowsBuildDir = $script:buildDir.Replace('\', '/')
        New-Item -ItemType Directory -Path (Join-Path $script:buildDir "CMakeFiles") -Force | Out-Null
        New-Item -ItemType File -Path (Join-Path $script:buildDir "build.ninja"), (Join-Path $script:buildDir "CMakeFiles/rules.ninja") -Force | Out-Null
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=$windowsBuildDir"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=C:/Qt/Tools/Ninja/ninja.exe"
            "CMAKE_CXX_COMPILER:FILEPATH=C:/Program Files/Microsoft Visual Studio/VC/Tools/MSVC/bin/cl.exe"
            "Qt6_DIR:PATH=C:/Qt/6.11.1/msvc2022_64/lib/cmake/Qt6"
            "VCPKG_TARGET_TRIPLET:STRING=x64-windows"
        ) | Set-Content -LiteralPath (Join-Path $script:buildDir "CMakeCache.txt") -Encoding ASCII

        & $script:lazyBuildScript -ProjectRoot $script:repoRoot -Toolchain llvm

        $configureArgs = Get-Content -LiteralPath $script:configureMarker
        $clangCl = (Get-Command "clang-cl.exe").Source
        $lldLink = (Get-Command "lld-link.exe").Source
        $configureArgs | Should -Match ([regex]::Escape("dev|-DVCPKG_TARGET_TRIPLET=x64-windows,-DCMAKE_C_COMPILER=$clangCl,-DCMAKE_CXX_COMPILER=$clangCl,-DCMAKE_LINKER=$lldLink"))
        $configureArgs | Should -Match ([regex]::Escape("-DCMAKE_C_FLAGS_INIT=-fuse-ld=lld,-DCMAKE_CXX_FLAGS_INIT=-fuse-ld=lld|msvc2022_64"))
        (Get-Content -LiteralPath $script:configureBinaryMarker) | Should -Be $script:llvmBuildDir
    }

    It "reuses a normalized LLVM compiler cache without fresh configuration" {
        $windowsBuildDir = $script:llvmBuildDir.Replace('\', '/')
        $clangCl = (Get-Command "clang-cl.exe").Source
        New-Item -ItemType Directory -Path (Join-Path $script:llvmBuildDir "CMakeFiles") -Force | Out-Null
        New-Item -ItemType File -Path (Join-Path $script:llvmBuildDir "build.ninja"), (Join-Path $script:llvmBuildDir "CMakeFiles/rules.ninja") -Force | Out-Null
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=$windowsBuildDir"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=C:/Qt/Tools/Ninja/ninja.exe"
            "CMAKE_CXX_COMPILER:FILEPATH=$($clangCl.Replace('\', '/'))"
            "Qt6_DIR:PATH=C:/Qt/6.11.1/msvc2022_64/lib/cmake/Qt6"
            "VCPKG_TARGET_TRIPLET:STRING=x64-windows"
        ) | Set-Content -LiteralPath (Join-Path $script:llvmBuildDir "CMakeCache.txt") -Encoding ASCII

        & $script:lazyBuildScript -ProjectRoot $script:repoRoot -Toolchain llvm

        (Get-Content -LiteralPath $script:configureMarker) | Should -Not -Match "--fresh"
    }

    It "ignores an LLVM cache during the default MSVC build" {
        $windowsBuildDir = $script:llvmBuildDir.Replace('\', '/')
        $clangCl = (Get-Command "clang-cl.exe").Source
        New-Item -ItemType Directory -Path (Join-Path $script:llvmBuildDir "CMakeFiles") -Force | Out-Null
        New-Item -ItemType File -Path (Join-Path $script:llvmBuildDir "build.ninja"), (Join-Path $script:llvmBuildDir "CMakeFiles/rules.ninja") -Force | Out-Null
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=$windowsBuildDir"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=C:/Qt/Tools/Ninja/ninja.exe"
            "CMAKE_CXX_COMPILER:FILEPATH=$($clangCl.Replace('\', '/'))"
            "Qt6_DIR:PATH=C:/Qt/6.11.1/msvc2022_64/lib/cmake/Qt6"
            "VCPKG_TARGET_TRIPLET:STRING=x64-windows"
        ) | Set-Content -LiteralPath (Join-Path $script:llvmBuildDir "CMakeCache.txt") -Encoding ASCII

        & $script:lazyBuildScript -ProjectRoot $script:repoRoot

        $configureArgs = Get-Content -LiteralPath $script:configureMarker
        $configureArgs | Should -Be "dev|-DVCPKG_TARGET_TRIPLET=x64-windows|msvc2022_64"
        $configureArgs | Should -Not -Match "clang-cl|lld-link|fuse-ld"
        (Get-Content -LiteralPath $script:configureBinaryMarker) | Should -Be $script:buildDir
    }

    It "throws when CMake returns a nonzero exit code" {
        @'
@echo off
echo %*>>"%SSA_TEST_CMAKE_LOG%"
exit /b 23
'@ | Set-Content -LiteralPath (Join-Path $script:fakeBin "cmake.cmd") -Encoding ASCII

        {
            & $script:buildScript -ProjectRoot $script:repoRoot
        } | Should -Throw -ExpectedMessage "*CMake*exit code 23*"
    }

    It "uses the canonical amd64 build directory" {
        $expectedBuildDir = Join-Path $script:repoRoot "build/windows/amd64/msvc/msvc2022_64/dev"

        & $script:buildScript -ProjectRoot $script:repoRoot

        (Get-Content -LiteralPath $script:configureMarker) | Should -Be "dev|-DVCPKG_TARGET_TRIPLET=x64-windows|msvc2022_64"
        (Get-Content -LiteralPath $script:configureBinaryMarker) | Should -Be $expectedBuildDir
        (Get-Content -LiteralPath $script:cmakeLog) | Should -Match ([regex]::Escape("--build $expectedBuildDir"))
    }

    It "uses the arm64 Qt kit, triplet, and canonical build directory" {
        $expectedBuildDir = Join-Path $script:repoRoot "build/windows/arm64/msvc/msvc2022_arm64/dev"

        & $script:lazyBuildScript -ProjectRoot $script:repoRoot -Arch arm64

        (Get-Content -LiteralPath $script:configureMarker) | Should -Be "dev|-DVCPKG_TARGET_TRIPLET=arm64-windows|msvc2022_arm64"
        (Get-Content -LiteralPath $script:configureBinaryMarker) | Should -Be $expectedBuildDir
        (Get-Content -LiteralPath $script:cmakeLog) | Should -Match ([regex]::Escape("--build $expectedBuildDir"))
    }

    It "refreshes an arm64 cache with an x64 vcpkg triplet" {
        $armBuildDir = Join-Path $script:repoRoot "build/windows/arm64/msvc/msvc2022_arm64/dev"
        $windowsBuildDir = $armBuildDir.Replace('\', '/')
        New-Item -ItemType Directory -Path (Join-Path $armBuildDir "CMakeFiles") -Force | Out-Null
        New-Item -ItemType File -Path (Join-Path $armBuildDir "build.ninja"), (Join-Path $armBuildDir "CMakeFiles/rules.ninja") | Out-Null
        @(
            "CMAKE_CACHEFILE_DIR:INTERNAL=$windowsBuildDir"
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_MAKE_PROGRAM:FILEPATH=C:/Qt/Tools/Ninja/ninja.exe"
            "Qt6_DIR:PATH=C:/Qt/6.11.1/msvc2022_arm64/lib/cmake/Qt6"
            "VCPKG_TARGET_TRIPLET:STRING=x64-windows"
        ) | Set-Content -LiteralPath (Join-Path $armBuildDir "CMakeCache.txt") -Encoding ASCII

        & $script:buildScript -ProjectRoot $script:repoRoot -Arch arm64

        (Get-Content -LiteralPath $script:configureMarker) | Should -Match "arm64-windows"
    }

    It "preserves an explicit compatible arm64 vcpkg triplet" {
        & $script:lazyBuildScript -ProjectRoot $script:repoRoot -Arch arm64 `
            -CmakeExtraArgs "-DVCPKG_TARGET_TRIPLET=arm64-windows-static" # pragma: allowlist secret

        (Get-Content -LiteralPath $script:configureMarker) | Should -Be "dev|-DVCPKG_TARGET_TRIPLET=arm64-windows-static|msvc2022_arm64"
    }

    It "rejects incompatible architecture and Qt kit combinations" -TestCases @(
        @{ Arch = "arm64"; QtSubdir = "msvc2022_64" }
        @{ Arch = "amd64"; QtSubdir = "msvc2022_arm64" }
    ) {
        param($Arch, $QtSubdir)
        $parameters = @{ Arch = $Arch; QtSubdir = $QtSubdir }

        {
            & $script:buildScript -ProjectRoot $script:repoRoot @parameters
        } | Should -Throw -ExpectedMessage "*incompatible*"
    }

    It "reports a missing CMake cache immediately after configuration" {
        $env:SSA_TEST_SKIP_CONFIGURE_CACHE = "1"

        try {
            {
                & $script:buildScript -ProjectRoot $script:repoRoot
            } | Should -Throw -ExpectedMessage "*CMake configuration did not produce expected cache*"
        }
        finally {
            Remove-Item Env:SSA_TEST_SKIP_CONFIGURE_CACHE -ErrorAction SilentlyContinue
        }
    }

    It "removes the canonical build directory by default" {
        $sentinel = Join-Path $script:buildDir "stale-output.txt"
        Set-Content -LiteralPath $sentinel -Value "stale" -Encoding ASCII

        & $script:buildScript -ProjectRoot $script:repoRoot

        (Test-Path -LiteralPath $sentinel) | Should -BeFalse
        (Get-Content -LiteralPath $script:configureMarker) |
            Should -Be "dev|-DVCPKG_TARGET_TRIPLET=x64-windows|msvc2022_64"
    }
}
