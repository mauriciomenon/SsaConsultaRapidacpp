$ErrorActionPreference = "Stop"

Describe "Windows package build failure" {
    It "keeps the NSIS source path short inside each transactional run" {
        $packageScript = Join-Path (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path "scripts/package-windows.ps1"
        $scriptText = Get-Content -LiteralPath $packageScript -Raw

        $scriptText | Should -Match ([regex]::Escape("ToString('N').Substring(0, 8)"))
        $scriptText | Should -Match ([regex]::Escape('$artifactDir = Join-Path $runStage "app"'))
    }

    It "aborts before cleaning a previous release when the build fails" {
        $testRoot = (Get-PSDrive -Name TestDrive).Root
        $repoRoot = Join-Path $testRoot "repo"
        $scriptsDir = Join-Path $repoRoot "scripts"
        $distDir = Join-Path $testRoot "dist"
        $artifactDir = Join-Path $distDir "amd64/repo-windows-amd64-1.2.3"
        $sentinel = Join-Path $artifactDir "previous-release.txt"
        $buildDir = Join-Path $repoRoot "build/windows/amd64/mingw_64/release"
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
            (Test-Path -LiteralPath $buildDir) | Should -BeFalse
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
        $buildDir = Join-Path $repoRoot "build/windows/amd64/mingw_64/release"
        $qtPrefix = Join-Path $testRoot "Qt/6.11.1/mingw_64"
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
            (Test-Path -LiteralPath (Join-Path $testRoot "dist/amd64/.staging")) | Should -BeFalse
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

Describe "Windows release set publication" {
    BeforeEach {
        $script:testRoot = (Get-PSDrive -Name TestDrive).Root
        $script:distRoot = Join-Path $script:testRoot "dist/windows/amd64"
        $script:stageDir = Join-Path $script:distRoot ".staging/run-123"
        $script:finalDir = Join-Path $script:distRoot "final"
        $script:currentPath = Join-Path $script:distRoot "current.json"
        Remove-Item -LiteralPath $script:distRoot -Recurse -Force -ErrorAction SilentlyContinue
        . (Join-Path (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path "scripts/lib/package_common.ps1")
    }

    It "preserves the current release when the staged set is incomplete" {
        New-Item -ItemType Directory -Path $script:finalDir, $script:stageDir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $script:finalDir "previous-release.txt") -Value "keep" -Encoding ASCII
        Set-Content -LiteralPath $script:currentPath -Value '{"version":"1.0.0","commit":"old"}' -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida.exe") -Value "incomplete" -Encoding ASCII

        {
            Publish-WindowsReleaseSet -StageDir $script:stageDir -DistRoot $script:distRoot -Version "1.2.3" -CommitSha "abc123"
        } | Should -Throw -ExpectedMessage "*incomplete*"

        (Get-Content -LiteralPath (Join-Path $script:finalDir "previous-release.txt")) | Should -Be "keep"
        (Get-Content -LiteralPath $script:currentPath) | Should -Be '{"version":"1.0.0","commit":"old"}'
    }

    It "promotes a complete staged set with immutable hashes" {
        New-Item -ItemType Directory -Path $script:finalDir, (Join-Path $script:stageDir "ssa_consulta_rapida-standalone") -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $script:finalDir "previous-release.txt") -Value "replace" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida.exe") -Value "portable" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida-installer.exe") -Value "installer" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida.zip") -Value "zip" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida-standalone/ssa_consulta_rapida.exe") -Value "standalone" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida-standalone/vc_redist.x64.exe") -Value "runtime" -Encoding ASCII

        Publish-WindowsReleaseSet -StageDir $script:stageDir -DistRoot $script:distRoot -Version "1.2.3" -CommitSha "abc123"

        $releaseDir = Join-Path $script:distRoot "releases/1.2.3-abc123"
        (Test-Path -LiteralPath (Join-Path $script:finalDir "previous-release.txt")) | Should -BeFalse
        (Test-Path -LiteralPath (Join-Path $script:finalDir "ssa_consulta_rapida-standalone/ssa_consulta_rapida.exe")) | Should -BeTrue
        (Test-Path -LiteralPath (Join-Path $releaseDir "ssa_consulta_rapida.zip")) | Should -BeTrue
        (Get-Content -LiteralPath (Join-Path $releaseDir "SHA256SUMS") -Raw) | Should -Match (Get-FileHash -LiteralPath (Join-Path $releaseDir "ssa_consulta_rapida.zip") -Algorithm SHA256).Hash
        (Get-Content -LiteralPath $script:currentPath -Raw) | Should -Match "1.2.3"
        (Get-Content -LiteralPath $script:currentPath -Raw) | Should -Match "abc123"
    }

    It "rejects a different immutable release manifest without replacing current" {
        $releaseDir = Join-Path $script:distRoot "releases/1.2.3-abc123"
        New-Item -ItemType Directory -Path $script:finalDir, (Join-Path $script:stageDir "ssa_consulta_rapida-standalone"), $releaseDir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $script:finalDir "previous-release.txt") -Value "keep" -Encoding ASCII
        Set-Content -LiteralPath $script:currentPath -Value '{"version":"1.0.0","commit":"old"}' -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $releaseDir "SHA256SUMS") -Value "old-hash" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida.exe") -Value "portable" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida-installer.exe") -Value "installer" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida.zip") -Value "zip" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida-standalone/ssa_consulta_rapida.exe") -Value "standalone" -Encoding ASCII

        {
            Publish-WindowsReleaseSet -StageDir $script:stageDir -DistRoot $script:distRoot -Version "1.2.3" -CommitSha "abc123"
        } | Should -Throw -ExpectedMessage "*different hashes*"

        (Get-Content -LiteralPath (Join-Path $script:finalDir "previous-release.txt")) | Should -Be "keep"
        (Get-Content -LiteralPath $script:currentPath) | Should -Be '{"version":"1.0.0","commit":"old"}'
    }

    It "preserves current release when another publication holds the lock" {
        $lockPath = Join-Path $script:distRoot ".publish.lock"
        New-Item -ItemType Directory -Path $script:finalDir, (Join-Path $script:stageDir "ssa_consulta_rapida-standalone") -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $script:finalDir "previous-release.txt") -Value "keep" -Encoding ASCII
        Set-Content -LiteralPath $script:currentPath -Value '{"version":"1.0.0","commit":"old"}' -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida.exe") -Value "portable" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida-installer.exe") -Value "installer" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida.zip") -Value "zip" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida-standalone/ssa_consulta_rapida.exe") -Value "standalone" -Encoding ASCII
        $lock = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)

        try {
            {
                Publish-WindowsReleaseSet -StageDir $script:stageDir -DistRoot $script:distRoot -Version "1.2.3" -CommitSha "abc123"
            } | Should -Throw -ExpectedMessage "*already running*"
        }
        finally {
            $lock.Dispose()
        }

        (Get-Content -LiteralPath (Join-Path $script:finalDir "previous-release.txt")) | Should -Be "keep"
        (Get-Content -LiteralPath $script:currentPath) | Should -Be '{"version":"1.0.0","commit":"old"}'
    }
}
