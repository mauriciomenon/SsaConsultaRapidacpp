$ErrorActionPreference = "Stop"

Describe "Windows package build failure" {
    It "aborts before cleaning a previous release when the build fails" {
        $testRoot = (Get-PSDrive -Name TestDrive).Root
        $repoRoot = Join-Path $testRoot "repo"
        $scriptsDir = Join-Path $repoRoot "scripts"
        $distDir = Join-Path $testRoot "dist"
        $artifactDir = Join-Path $distDir "amd64/repo-windows-amd64-1.2.3"
        $sentinel = Join-Path $artifactDir "previous-release.txt"
        $buildDir = Join-Path $repoRoot "build/windows/amd64/msvc2022_64/release"
        $nsisDir = Join-Path $testRoot "nsis"
        $packageScript = Join-Path (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path "scripts/package-windows.ps1"
        $originalNsisHome = $env:NSIS_HOME

        New-Item -ItemType Directory -Path $scriptsDir, $artifactDir, $buildDir, $nsisDir -Force | Out-Null
        Copy-Item -LiteralPath $env:ComSpec -Destination (Join-Path $nsisDir "makensis.exe")
        $env:NSIS_HOME = $nsisDir
        Set-Content -LiteralPath $sentinel -Value "keep" -Encoding ASCII
        New-Item -ItemType File -Path (Join-Path $buildDir "ssa_consulta_rapida.exe"), (Join-Path $buildDir "sqlite3.dll") | Out-Null
@'
$global:LASTEXITCODE = 23
'@ | Set-Content -LiteralPath (Join-Path $scriptsDir "build-windows.ps1") -Encoding ASCII
        @'
placeholder
'@ | Set-Content -LiteralPath (Join-Path $repoRoot "CMakeLists.txt") -Encoding ASCII

        try {
            $failure = $null
            try {
                & $packageScript -ProjectRoot $repoRoot -DistDir $distDir -Version "1.2.3" -SkipTests
            }
            catch {
                $failure = $_
            }

            (Test-Path -LiteralPath $sentinel -PathType Leaf) | Should -BeTrue
            $failure.Exception.Message | Should -Match "build.*exit code 23"
        }
        finally {
            if ($null -eq $originalNsisHome) {
                Remove-Item Env:NSIS_HOME -ErrorAction SilentlyContinue
            }
            else {
                $env:NSIS_HOME = $originalNsisHome
            }
        }
    }

    It "uses CMakeCache Qt6_DIR to find windeployqt" {
        $testRoot = (Get-PSDrive -Name TestDrive).Root
        $repoRoot = Join-Path $testRoot "repo"
        $scriptsDir = Join-Path $repoRoot "scripts"
        $buildDir = Join-Path $repoRoot "build/windows/amd64/msvc2022_64/release"
        $qtPrefix = Join-Path $testRoot "Qt/6.11.1/msvc2022_64"
        $qtCmakeDir = Join-Path $qtPrefix "lib/cmake/Qt6"
        $qtBinDir = Join-Path $qtPrefix "bin"
        $nsisDir = Join-Path $testRoot "nsis"
        $packageScript = Join-Path (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path "scripts/package-windows.ps1"
        $originalNsisHome = $env:NSIS_HOME
        $originalPathExt = $env:PATHEXT
        $originalPath = $env:Path
        $fakeBin = Join-Path $testRoot "bin"
        $buildMarker = Join-Path $testRoot "build-arch.txt"
        $ctestLog = Join-Path $testRoot "ctest-called.txt"

        New-Item -ItemType Directory -Path $scriptsDir, $buildDir, (Join-Path $buildDir "SsaConsultaRapida"), $qtBinDir, $nsisDir, $fakeBin,
            (Join-Path $repoRoot "resources"), (Join-Path $repoRoot "third_party/tinted-themes") -Force | Out-Null
        Copy-Item -LiteralPath $env:ComSpec -Destination (Join-Path $nsisDir "makensis.exe")
        Copy-Item -LiteralPath $env:ComSpec -Destination (Join-Path $qtBinDir "windeployqt.exe")
        $env:NSIS_HOME = $nsisDir
        $env:PATHEXT = [Environment]::GetEnvironmentVariable("PATHEXT", "Machine")
        $env:Path = "$fakeBin;$originalPath"
        New-Item -ItemType File -Path (Join-Path $buildDir "ssa_consulta_rapida.exe"), (Join-Path $buildDir "sqlite3.dll"),
            (Join-Path $repoRoot "resources/app_icon.ico"), (Join-Path $repoRoot "THIRD_PARTY_NOTICES.md"),
            (Join-Path $repoRoot "third_party/tinted-themes/LICENSE") -Force | Out-Null
        "Qt6_DIR:PATH=$($qtCmakeDir.Replace('\\', '/'))" | Set-Content -LiteralPath (Join-Path $buildDir "CMakeCache.txt") -Encoding ASCII
@'
param([string]$Arch)
Set-Content -LiteralPath $env:SSA_TEST_BUILD_ARCH -Value $Arch
$global:LASTEXITCODE = 0
'@ | Set-Content -LiteralPath (Join-Path $scriptsDir "build-windows.ps1") -Encoding ASCII
        @'
@echo off
echo %*>>"%SSA_TEST_CTEST_LOG%"
exit /b 0
'@ | Set-Content -LiteralPath (Join-Path $fakeBin "ctest.cmd") -Encoding ASCII
        $env:SSA_TEST_BUILD_ARCH = $buildMarker
        $env:SSA_TEST_CTEST_LOG = $ctestLog

        try {
            $failure = $null
            try {
                & $packageScript -ProjectRoot $repoRoot -Arch amd64 -DistDir (Join-Path $testRoot "dist") -Version "1.2.3"
            }
            catch {
                $failure = $_
            }

            (Get-Content -LiteralPath $buildMarker) | Should -Be "amd64"
            (Get-Content -LiteralPath $ctestLog) | Should -Match ([regex]::Escape("--test-dir $buildDir"))
            $failure.Exception.Message | Should -Match "did not copy Qt6Core"
        }
        finally {
            if ($null -eq $originalNsisHome) {
                Remove-Item Env:NSIS_HOME -ErrorAction SilentlyContinue
            }
            else {
                $env:NSIS_HOME = $originalNsisHome
            }
            $env:PATHEXT = $originalPathExt
            $env:Path = $originalPath
            Remove-Item Env:SSA_TEST_BUILD_ARCH, Env:SSA_TEST_CTEST_LOG -ErrorAction SilentlyContinue
        }
    }
}
