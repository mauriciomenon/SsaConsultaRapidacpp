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
}
