$ErrorActionPreference = "Stop"

Describe "Windows Qt detection" {
    BeforeAll {
        function Initialize-FakeQtPrefix {
            param(
                [Parameter(Mandatory)]
                [string]$Root,
                [Parameter(Mandatory)]
                [string]$Version,
                [Parameter(Mandatory)]
                [string]$Subdir
            )

            $configDir = Join-Path $Root "$Version/$Subdir/lib/cmake/Qt6"
            New-Item -ItemType Directory -Path $configDir -Force | Out-Null
            Set-Content -Path (Join-Path $configDir "Qt6Config.cmake") -Value "# fake Qt config"
            Set-Content -Path (Join-Path $configDir "Qt6ConfigVersionImpl.cmake") -Value "set(PACKAGE_VERSION `"$Version`")"
        }
    }

    BeforeEach {
        $script:qtRoot = Join-Path $TestDrive "Qt"
        Remove-Item $script:qtRoot -Recurse -Force -ErrorAction SilentlyContinue
        $script:configureScript = Join-Path (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path "tools/configure-dev.ps1"
        $env:Qt6_DIR = ""
        $env:QT_DIR = ""
        $env:CMAKE_PREFIX_PATH = ""
    }

    It "selects the latest compatible patch for the default MSVC kit" {
        Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.11.0" -Subdir "msvc2022_64"
        Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.11.1" -Subdir "msvc2022_64"
        Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.12.0" -Subdir "msvc2022_64"

        $output = & $script:configureScript -QtRoot $script:qtRoot -PrintQtSelection

        $output | Should -Contain "QtVersion=6.11.1"
        $output | Should -Contain "QtSubdir=msvc2022_64"
    }

    It "falls back to a lower patch when the latest compatible prefix is invalid" {
        Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.11.0" -Subdir "msvc2022_64"
        New-Item -ItemType Directory -Path (Join-Path $script:qtRoot "6.11.1/msvc2022_64") -Force | Out-Null

        $output = & $script:configureScript -QtRoot $script:qtRoot -PrintQtSelection

        $output | Should -Contain "QtVersion=6.11.0"
    }

    It "accepts an explicit LLVM MinGW desktop kit" {
        Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.11.1" -Subdir "llvm-mingw_64"

        $output = & $script:configureScript -QtRoot $script:qtRoot -QtSubdir "llvm-mingw_64" -PrintQtSelection

        $output | Should -Contain "QtSubdir=llvm-mingw_64"
    }

    It "accepts the explicit MSVC ARM64 desktop kit" {
        Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.11.1" -Subdir "msvc2022_arm64"

        $output = & $script:configureScript -QtRoot $script:qtRoot -QtSubdir "msvc2022_arm64" -PrintQtSelection

        $output | Should -Contain "QtSubdir=msvc2022_arm64"
    }

    It "uses LLVM MinGW as a desktop fallback when MSVC is absent" {
        Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.11.1" -Subdir "llvm-mingw_64"
        Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.11.1" -Subdir "wasm_multithread"

        $output = & $script:configureScript -QtRoot $script:qtRoot -PrintQtSelection

        $output | Should -Contain "QtSubdir=llvm-mingw_64"
    }

    It "never treats a WASM kit as a desktop fallback" {
        Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.11.1" -Subdir "wasm_singlethread"
        Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.11.1" -Subdir "wasm_multithread"

        { & $script:configureScript -QtRoot $script:qtRoot -PrintQtSelection } |
            Should -Throw "*desktop Qt kit*"
    }

    It "rejects an explicit Qt outside the configured family" {
        Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.12.0" -Subdir "msvc2022_64"
        $qtDir = Join-Path $script:qtRoot "6.12.0/msvc2022_64"

        { & $script:configureScript -QtDir $qtDir -PrintQtSelection } |
            Should -Throw "*Qt 6.11*"
    }

    It "reports an unknown version file without a null dereference" {
        Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.11.1" -Subdir "msvc2022_64"
        $qtDir = Join-Path $script:qtRoot "6.11.1/msvc2022_64"
        Remove-Item (Join-Path $qtDir "lib/cmake/Qt6/Qt6ConfigVersionImpl.cmake")

        { & $script:configureScript -QtDir $qtDir -PrintQtSelection } |
            Should -Throw "*detected unknown*"
    }

    Context "read-only dependency checks" {
        BeforeEach {
            $script:fixtureRepo = Join-Path $TestDrive "repo"
            $script:fixtureTools = Join-Path $script:fixtureRepo "tools"
            $script:fixtureBuild = Join-Path $script:fixtureRepo "build/dev"
            $script:fixtureBin = Join-Path $TestDrive "bin"
            New-Item -ItemType Directory -Path $script:fixtureTools, $script:fixtureBuild, $script:fixtureBin -Force |
                Out-Null
            Copy-Item $script:configureScript (Join-Path $script:fixtureTools "configure-dev.ps1")
            Copy-Item (Join-Path (Split-Path $script:configureScript -Parent) "qt-detect.conf") $script:fixtureTools
            $script:checkScript = Join-Path $script:fixtureTools "configure-dev.ps1"
            $script:cachePath = Join-Path $script:fixtureBuild "CMakeCache.txt"
            Set-Content -Path $script:cachePath -Value "unchanged-cache" -NoNewline
            $script:cacheBefore = [System.IO.File]::ReadAllBytes($script:cachePath)

            Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.11.1" -Subdir "msvc2022_64"
            $qtBin = Join-Path $script:qtRoot "6.11.1/msvc2022_64/bin"
            New-Item -ItemType Directory -Path $qtBin -Force | Out-Null
            $script:sqliteRoot = Join-Path $TestDrive "vcpkg/installed/x64-windows"
            New-Item -ItemType Directory -Path (Join-Path $script:sqliteRoot "include"), (Join-Path $script:sqliteRoot "lib") -Force |
                Out-Null
            New-Item -ItemType File -Path (Join-Path $script:sqliteRoot "include/sqlite3.h"), (Join-Path $script:sqliteRoot "lib/sqlite3.lib") -Force |
                Out-Null

            $nativeExecutable = if ($IsWindows -or $PSVersionTable.PSVersion.Major -le 5) {
                $env:ComSpec
            } else {
                (Get-Command true).Source
            }
            Copy-Item $nativeExecutable (Join-Path $script:fixtureBin "cl.exe")
            Copy-Item $nativeExecutable (Join-Path $qtBin "windeployqt.exe")
            Copy-Item $nativeExecutable (Join-Path $script:fixtureBin "robocopy.exe")
            Copy-Item $nativeExecutable (Join-Path $script:fixtureBin "tar.exe")
            foreach ($commandName in @("cmake", "ninja")) {
                if ($IsWindows -or $PSVersionTable.PSVersion.Major -le 5) {
                    Set-Content -Path (Join-Path $script:fixtureBin "$commandName.cmd") -Value "@exit /b 0" -Encoding ASCII
                } else {
                    Copy-Item $nativeExecutable (Join-Path $script:fixtureBin $commandName)
                }
            }
            $script:originalPath = $env:Path
            $script:originalNsisHome = $env:NSIS_HOME
            $script:nsisRoot = Join-Path $TestDrive "NSIS"
            New-Item -ItemType Directory -Path $script:nsisRoot -Force | Out-Null
            Copy-Item $nativeExecutable (Join-Path $script:nsisRoot "makensis.exe")
            $env:NSIS_HOME = $script:nsisRoot
            $env:Path = "$script:fixtureBin$([IO.Path]::PathSeparator)$env:Path"
        }

        AfterEach {
            $env:Path = $script:originalPath
            $env:NSIS_HOME = $script:originalNsisHome
        }

        It "reports detected development dependencies and preserves the selected cache" {
            $output = & $script:checkScript -QtRoot $script:qtRoot -SQLiteRoot $script:sqliteRoot -Check

            $output | Should -Contain "OK qt path=$(Join-Path $script:qtRoot '6.11.1/msvc2022_64') version=6.11.1"
            ($output | Where-Object { $_ -like "OK compiler path=* version=*" }) | Should -Not -BeNullOrEmpty
            ($output | Where-Object { $_ -like "OK sqlite path=* version=*" }) | Should -Not -BeNullOrEmpty
            [System.IO.File]::ReadAllBytes($script:cachePath) | Should -Be $script:cacheBefore
        }

        It "reports official downloads and the vcpkg command for missing dependencies" {
            Remove-Item (Join-Path $script:sqliteRoot "lib/sqlite3.lib")

            $hostExecutable = (Get-Process -Id $PID).Path
            $output = & $hostExecutable -NoProfile -File $script:checkScript -QtRoot $script:qtRoot -SQLiteRoot $script:sqliteRoot -Check 2>&1
            $LASTEXITCODE | Should -Not -Be 0

            $output | Should -Contain "MISSING sqlite path=- version=-"
            $output | Should -Contain "HINT sqlite vcpkg install sqlite3:x64-windows"

            Remove-Item $script:qtRoot -Recurse -Force
            $missingOutput = & $hostExecutable -NoProfile -File $script:checkScript -QtRoot $script:qtRoot -SQLiteRoot $script:sqliteRoot -Check 2>&1
            $LASTEXITCODE | Should -Not -Be 0
            ($missingOutput | Where-Object { $_ -match '^HINT (qt|compiler) https://' }).Count |
                Should -BeGreaterThan 0
        }

        It "checks package tools without configuring CMake" {
            $output = & $script:checkScript -QtRoot $script:qtRoot -SQLiteRoot $script:sqliteRoot -CheckPackage 2>&1

            ($output | Where-Object { $_ -match '^OK nsis path=' }).Count | Should -Be 1
            ($output | Where-Object { $_ -match '^OK windeployqt path=' }).Count | Should -Be 1
            ($output | Where-Object { $_ -match '^OK robocopy path=' }).Count | Should -Be 1
            ($output | Where-Object { $_ -match '^OK tar path=' }).Count | Should -Be 1
            [System.IO.File]::ReadAllBytes($script:cachePath) | Should -Be $script:cacheBefore
        }

        It "distinguishes an unsupported Qt family" {
            Initialize-FakeQtPrefix -Root $script:qtRoot -Version "6.12.0" -Subdir "msvc2022_64"
            $qtDir = Join-Path $script:qtRoot "6.12.0/msvc2022_64"

            $hostExecutable = (Get-Process -Id $PID).Path
            $output = & $hostExecutable -NoProfile -File $script:checkScript -QtDir $qtDir -SQLiteRoot $script:sqliteRoot -Check 2>&1

            $LASTEXITCODE | Should -Not -Be 0
            $output | Should -Contain "UNSUPPORTED qt path=$qtDir version=6.12.0"
            [System.IO.File]::ReadAllBytes($script:cachePath) | Should -Be $script:cacheBefore
        }
    }
}
