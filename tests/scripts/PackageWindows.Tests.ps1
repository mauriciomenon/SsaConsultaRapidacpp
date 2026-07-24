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

    It "packages matching MSVC and LLVM runtimes from their authoritative toolsets" {
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
        $originalVcToolsInstallDir = $env:VCToolsInstallDir
        $originalVcToolsVersion = $env:VCToolsVersion
        $fakeBin = Join-Path $testRoot "bin"
        $buildMarker = Join-Path $testRoot "build-arch.txt"
        $ctestLog = Join-Path $testRoot "ctest-called.txt"
        $toolsetVersion = "14.51.36231"
        $toolsetRoot = Join-Path $testRoot "Visual Studio/VC/Tools/MSVC/$toolsetVersion"
        $runtimeDir = Join-Path $testRoot "Visual Studio/VC/Redist/MSVC/$toolsetVersion/x64/Microsoft.VC145.CRT"

        New-Item -ItemType Directory -Path $scriptsDir, $buildDir, (Join-Path $buildDir "SsaConsultaRapida"), $qtBinDir, $nsisDir, $fakeBin, $runtimeDir,
            (Join-Path $repoRoot "resources"), (Join-Path $repoRoot "third_party/tinted-themes") -Force | Out-Null
        $fakeTool = Join-Path $fakeBin "package-tool.exe"
        Add-Type -TypeDefinition @'
using System;
using System.IO;

public static class PackageTool
{
    public static int Main(string[] args)
    {
        var tool = Path.GetFileNameWithoutExtension(Environment.GetCommandLineArgs()[0]).ToLowerInvariant();
        if (tool == "windeployqt") {
            if (args.Length == 0 || args[0] != "--no-compiler-runtime") return 13;
            var executable = args[args.Length - 1];
            File.WriteAllText(Path.Combine(Path.GetDirectoryName(executable), "Qt6Core.dll"), "qt");
            return 0;
        }
        if (args.Length > 0 && args[0] == "/imports") {
            Console.WriteLine("    MSVCP140.dll");
            Console.WriteLine("    MSVCP140_ATOMIC_WAIT.dll");
            Console.WriteLine("    VCRUNTIME140.dll");
            Console.WriteLine("    VCRUNTIME140_1.dll");
            return 0;
        }
        if (tool == "makensis") {
            foreach (var line in File.ReadAllLines(args[0])) {
                if (!line.StartsWith("OutFile \"", StringComparison.Ordinal)) continue;
                var output = line.Substring(9).TrimEnd('\"');
                Directory.CreateDirectory(Path.GetDirectoryName(output));
                File.WriteAllText(output, "nsis");
                return 0;
            }
            return 1;
        }
        if (tool == "tar") {
            for (var index = 0; index + 1 < args.Length; index++) {
                if (args[index] != "-f") continue;
                File.WriteAllText(args[index + 1], "zip");
                return 0;
            }
            return 1;
        }
        return 0;
    }
}
'@ -OutputAssembly $fakeTool -OutputType ConsoleApplication
        Copy-Item -LiteralPath $fakeTool -Destination (Join-Path $nsisDir "makensis.exe")
        Copy-Item -LiteralPath $fakeTool -Destination (Join-Path $qtBinDir "windeployqt.exe")
        Copy-Item -LiteralPath $fakeTool -Destination (Join-Path $fakeBin "tar.exe")
        Copy-Item -LiteralPath $fakeTool -Destination (Join-Path $fakeBin "robocopy.exe")
        $dumpbinDir = Join-Path $toolsetRoot "bin/Hostx64/x64"
        New-Item -ItemType Directory -Path $dumpbinDir -Force | Out-Null
        Copy-Item -LiteralPath $fakeTool -Destination (Join-Path $dumpbinDir "dumpbin.exe")
        $env:NSIS_HOME = $nsisDir
        $env:PATHEXT = [Environment]::GetEnvironmentVariable("PATHEXT", "Machine")
        $env:Path = "$fakeBin;$originalPath"
        New-Item -ItemType File -Path (Join-Path $buildDir "ssa_consulta_rapida.exe"), (Join-Path $buildDir "sqlite3.dll"),
            (Join-Path $repoRoot "resources/app_icon.ico"), (Join-Path $repoRoot "THIRD_PARTY_NOTICES.md"),
            (Join-Path $repoRoot "third_party/tinted-themes/LICENSE") -Force | Out-Null
        foreach ($runtimeFile in @("MSVCP140.dll", "MSVCP140_ATOMIC_WAIT.dll", "VCRUNTIME140.dll", "VCRUNTIME140_1.dll")) {
            New-Item -ItemType File -Path (Join-Path $runtimeDir $runtimeFile) | Out-Null
        }
        @(
            "Qt6_DIR:PATH=$($qtCmakeDir.Replace('\', '/'))"
            "CMAKE_CXX_COMPILER:FILEPATH=$($toolsetRoot.Replace('\', '/'))/bin/Hostx64/x64/cl.exe"
            "CMAKE_LINKER:FILEPATH=$($toolsetRoot.Replace('\', '/'))/bin/Hostx64/x64/link.exe"
        ) | Set-Content -LiteralPath (Join-Path $buildDir "CMakeCache.txt") -Encoding ASCII
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
@'
@echo off
echo %* | findstr /C:"rev-parse" >nul
if not errorlevel 1 echo abc123def456
exit /b 0
'@ | Set-Content -LiteralPath (Join-Path $fakeBin "git.cmd") -Encoding ASCII
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
            $failure | Should -BeNullOrEmpty
            $finalDir = Join-Path $testRoot "dist/amd64/msvc/final"
            foreach ($runtimeFile in @("MSVCP140.dll", "MSVCP140_ATOMIC_WAIT.dll", "VCRUNTIME140.dll", "VCRUNTIME140_1.dll")) {
                (Get-ChildItem -LiteralPath $finalDir -Filter $runtimeFile -File -Recurse).Count | Should -BeGreaterThan 0
            }
            (Test-Path -LiteralPath (Join-Path $testRoot "dist/amd64/.staging")) | Should -BeFalse

            $env:VCToolsInstallDir = $toolsetRoot
            Remove-Item Env:VCToolsVersion -ErrorAction SilentlyContinue
            New-Item -ItemType Directory -Path $buildDir, (Join-Path $buildDir "SsaConsultaRapida") -Force | Out-Null
            New-Item -ItemType File -Path (Join-Path $buildDir "ssa_consulta_rapida.exe"), (Join-Path $buildDir "sqlite3.dll") -Force | Out-Null
            @(
                "Qt6_DIR:PATH=$($qtCmakeDir.Replace('\', '/'))"
                "VCToolsInstallDir:PATH=C:/stale/VC/Tools/MSVC/14.52.00000"
                "CMAKE_CXX_COMPILER:FILEPATH=C:/LLVM/bin/clang-cl.exe"
                "CMAKE_LINKER:FILEPATH=C:/LLVM/bin/lld-link.exe"
            ) | Set-Content -LiteralPath (Join-Path $buildDir "CMakeCache.txt") -Encoding ASCII
            {
                & $packageScript -ProjectRoot $repoRoot -Arch amd64 -Toolchain llvm -DistDir (Join-Path $testRoot "llvm-missing-version") -Version "1.2.3" -SkipTests
            } | Should -Throw -ExpectedMessage "*requires VCToolsVersion*"
            (Test-Path -LiteralPath (Join-Path $testRoot "llvm-missing-version/amd64/llvm/final") -PathType Container) | Should -BeFalse

            $env:VCToolsVersion = "14.52.00000"
            New-Item -ItemType Directory -Path $buildDir, (Join-Path $buildDir "SsaConsultaRapida") -Force | Out-Null
            New-Item -ItemType File -Path (Join-Path $buildDir "ssa_consulta_rapida.exe"), (Join-Path $buildDir "sqlite3.dll") -Force | Out-Null
            @(
                "Qt6_DIR:PATH=$($qtCmakeDir.Replace('\', '/'))"
                "VCToolsInstallDir:PATH=C:/stale/VC/Tools/MSVC/14.52.00000"
                "CMAKE_CXX_COMPILER:FILEPATH=C:/LLVM/bin/clang-cl.exe"
                "CMAKE_LINKER:FILEPATH=C:/LLVM/bin/lld-link.exe"
            ) | Set-Content -LiteralPath (Join-Path $buildDir "CMakeCache.txt") -Encoding ASCII
            {
                & $packageScript -ProjectRoot $repoRoot -Arch amd64 -Toolchain llvm -DistDir (Join-Path $testRoot "llvm-mismatch") -Version "1.2.3" -SkipTests
            } | Should -Throw -ExpectedMessage "*VCToolsInstallDir and VCToolsVersion disagree*"

            $env:VCToolsVersion = $toolsetVersion
            New-Item -ItemType Directory -Path $buildDir, (Join-Path $buildDir "SsaConsultaRapida") -Force | Out-Null
            New-Item -ItemType File -Path (Join-Path $buildDir "ssa_consulta_rapida.exe"), (Join-Path $buildDir "sqlite3.dll") -Force | Out-Null
            @(
                "Qt6_DIR:PATH=$($qtCmakeDir.Replace('\', '/'))"
                "VCToolsInstallDir:PATH=C:/stale/VC/Tools/MSVC/14.52.00000"
                "CMAKE_CXX_COMPILER:FILEPATH=C:/LLVM/bin/clang-cl.exe"
                "CMAKE_LINKER:FILEPATH=C:/LLVM/bin/lld-link.exe"
            ) | Set-Content -LiteralPath (Join-Path $buildDir "CMakeCache.txt") -Encoding ASCII
            $llvmFailure = $null
            try {
                & $packageScript -ProjectRoot $repoRoot -Arch amd64 -Toolchain llvm -DistDir (Join-Path $testRoot "llvm") -Version "1.2.3" -SkipTests
            }
            catch {
                $llvmFailure = $_
            }
            $llvmFailure | Should -BeNullOrEmpty
            ((Get-Content -LiteralPath (Join-Path $testRoot "llvm/amd64/llvm/current.json") -Raw | ConvertFrom-Json).compiler) | Should -Match "clang-cl.exe$"
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
            if ($null -eq $originalVcToolsInstallDir) {
                Remove-Item Env:VCToolsInstallDir -ErrorAction SilentlyContinue
            }
            else {
                $env:VCToolsInstallDir = $originalVcToolsInstallDir
            }
            if ($null -eq $originalVcToolsVersion) {
                Remove-Item Env:VCToolsVersion -ErrorAction SilentlyContinue
            }
            else {
                $env:VCToolsVersion = $originalVcToolsVersion
            }
            Remove-Item Env:SSA_TEST_BUILD_ARCH, Env:SSA_TEST_CTEST_LOG -ErrorAction SilentlyContinue
        }
    }

    It "fails before package promotion when the matching Visual C++ runtime is absent" {
        $testRoot = (Get-PSDrive -Name TestDrive).Root
        $repoRoot = Join-Path $testRoot "repo"
        $scriptsDir = Join-Path $repoRoot "scripts"
        $buildDir = Join-Path $repoRoot "build/windows/amd64/msvc2022_64/release"
        $qtPrefix = Join-Path $testRoot "Qt/6.11.1/msvc2022_64"
        $qtCmakeDir = Join-Path $qtPrefix "lib/cmake/Qt6"
        $qtBinDir = Join-Path $qtPrefix "bin"
        $nsisDir = Join-Path $testRoot "nsis"
        $fakeBin = Join-Path $testRoot "bin"
        $packageScript = Join-Path (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path "scripts/package-windows.ps1"
        $originalNsisHome = $env:NSIS_HOME
        $originalPathExt = $env:PATHEXT
        $toolsetRoot = Join-Path $testRoot "Visual Studio/VC/Tools/MSVC/14.52.00000"
        $missingRuntimeDistDir = Join-Path $testRoot "missing-runtime-dist"

        New-Item -ItemType Directory -Path $scriptsDir, $buildDir, (Join-Path $buildDir "SsaConsultaRapida"), $qtBinDir, $nsisDir, $fakeBin,
            (Join-Path $repoRoot "resources"), (Join-Path $repoRoot "third_party/tinted-themes") -Force | Out-Null
        Copy-Item -LiteralPath $env:ComSpec -Destination (Join-Path $nsisDir "makensis.exe")
        $qtDeployMock = Join-Path $fakeBin "windeployqt.exe"
        Add-Type -TypeDefinition @'
using System;
using System.IO;

public static class QtDeployMock
{
    public static int Main(string[] args)
    {
        File.WriteAllText(Path.Combine(Path.GetDirectoryName(args[args.Length - 1]), "Qt6Core.dll"), "qt");
        return 0;
    }
}
'@ -OutputAssembly $qtDeployMock -OutputType ConsoleApplication
        Copy-Item -LiteralPath $qtDeployMock -Destination (Join-Path $qtBinDir "windeployqt.exe")
        $env:NSIS_HOME = $nsisDir
        $originalPath = $env:Path
        $env:PATHEXT = [Environment]::GetEnvironmentVariable("PATHEXT", "Machine")
        $env:Path = "$fakeBin;$originalPath"
        New-Item -ItemType File -Path (Join-Path $buildDir "ssa_consulta_rapida.exe"), (Join-Path $buildDir "sqlite3.dll"),
            (Join-Path $repoRoot "resources/app_icon.ico"), (Join-Path $repoRoot "THIRD_PARTY_NOTICES.md"),
            (Join-Path $repoRoot "third_party/tinted-themes/LICENSE") -Force | Out-Null
        @(
            "Qt6_DIR:PATH=$($qtCmakeDir.Replace('\', '/'))"
            "CMAKE_CXX_COMPILER:FILEPATH=$($toolsetRoot.Replace('\', '/'))/bin/Hostx64/x64/cl.exe"
            "CMAKE_LINKER:FILEPATH=$($toolsetRoot.Replace('\', '/'))/bin/Hostx64/x64/link.exe"
        ) | Set-Content -LiteralPath (Join-Path $buildDir "CMakeCache.txt") -Encoding ASCII
@'
$global:LASTEXITCODE = 0
'@ | Set-Content -LiteralPath (Join-Path $scriptsDir "build-windows.ps1") -Encoding ASCII
@'
@echo off
echo %* | findstr /C:"rev-parse" >nul
if not errorlevel 1 echo abc123def456
exit /b 0
'@ | Set-Content -LiteralPath (Join-Path $fakeBin "git.cmd") -Encoding ASCII

        try {
            {
                & $packageScript -ProjectRoot $repoRoot -Arch amd64 -Toolchain msvc -DistDir $missingRuntimeDistDir -Version "1.2.3" -SkipTests
            } | Should -Throw -ExpectedMessage "*Matching Visual C++ runtime*"

            (Test-Path -LiteralPath (Join-Path $missingRuntimeDistDir "amd64/msvc/final") -PathType Container) | Should -BeFalse
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
        }
    }
}

Describe "Windows release set publication" {
    BeforeEach {
        $script:testRoot = (Get-PSDrive -Name TestDrive).Root
        $script:distRoot = Join-Path $script:testRoot "dist/windows/amd64/msvc"
        $script:stageDir = Join-Path $script:distRoot ".staging/run-123"
        $script:finalDir = Join-Path $script:distRoot "final"
        $script:currentPath = Join-Path $script:distRoot "current.json"
        $script:releaseMetadata = @{
            Preset = "release"
            QtKit = "msvc2022_64"
            Compiler = "C:\\tool\\cl.exe"
            CompilerVersion = "test"
            Linker = "C:\\tool\\link.exe"
        }
        Remove-Item -LiteralPath $script:distRoot -Recurse -Force -ErrorAction SilentlyContinue
        . (Join-Path (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path "scripts/lib/package_common.ps1")
    }

    It "preserves the current release when the staged set is incomplete" {
        New-Item -ItemType Directory -Path $script:finalDir, $script:stageDir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $script:finalDir "previous-release.txt") -Value "keep" -Encoding ASCII
        Set-Content -LiteralPath $script:currentPath -Value '{"version":"1.0.0","commit":"old"}' -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida.exe") -Value "incomplete" -Encoding ASCII

        {
            Publish-WindowsReleaseSet -StageDir $script:stageDir -DistRoot $script:distRoot -Version "1.2.3" -CommitSha "abc123" -RepoRoot $script:testRoot -Platform windows -Architecture amd64 -Toolchain msvc -TaggedRelease $false @script:releaseMetadata
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

        Publish-WindowsReleaseSet -StageDir $script:stageDir -DistRoot $script:distRoot -Version "1.2.3" -CommitSha "abc123" -RepoRoot $script:testRoot -Platform windows -Architecture amd64 -Toolchain msvc -TaggedRelease $false @script:releaseMetadata

        $releaseDir = Join-Path $script:distRoot "releases/1.2.3-abc123-windows-amd64-msvc"
        (Test-Path -LiteralPath (Join-Path $script:finalDir "previous-release.txt")) | Should -BeFalse
        (Test-Path -LiteralPath (Join-Path $script:finalDir "ssa_consulta_rapida-standalone/ssa_consulta_rapida.exe")) | Should -BeTrue
        (Test-Path -LiteralPath $releaseDir) | Should -BeFalse
        (Get-Content -LiteralPath $script:currentPath -Raw) | Should -Match "1.2.3"
        (Get-Content -LiteralPath $script:currentPath -Raw) | Should -Match "abc123"
        (Get-Content -LiteralPath $script:currentPath -Raw) | Should -Match '"toolchain":  "msvc"'
        ((Get-Content -LiteralPath $script:currentPath -Raw | ConvertFrom-Json).compiler) | Should -Be $script:releaseMetadata.Compiler
    }

    It "rejects a different immutable release manifest without replacing current" {
        $releaseDir = Join-Path $script:distRoot "releases/1.2.3-abc123-windows-amd64-msvc"
        New-Item -ItemType Directory -Path $script:finalDir, (Join-Path $script:stageDir "ssa_consulta_rapida-standalone"), $releaseDir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $script:finalDir "previous-release.txt") -Value "keep" -Encoding ASCII
        Set-Content -LiteralPath $script:currentPath -Value '{"version":"1.0.0","commit":"old"}' -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $releaseDir "SHA256SUMS") -Value "old-hash" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida.exe") -Value "portable" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida-installer.exe") -Value "installer" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida.zip") -Value "zip" -Encoding ASCII
        Set-Content -LiteralPath (Join-Path $script:stageDir "ssa_consulta_rapida-standalone/ssa_consulta_rapida.exe") -Value "standalone" -Encoding ASCII

        {
            Publish-WindowsReleaseSet -StageDir $script:stageDir -DistRoot $script:distRoot -Version "1.2.3" -CommitSha "abc123" -RepoRoot $script:testRoot -Platform windows -Architecture amd64 -Toolchain msvc -TaggedRelease $true @script:releaseMetadata
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
                Publish-WindowsReleaseSet -StageDir $script:stageDir -DistRoot $script:distRoot -Version "1.2.3" -CommitSha "abc123" -RepoRoot $script:testRoot -Platform windows -Architecture amd64 -Toolchain msvc -TaggedRelease $false @script:releaseMetadata
            } | Should -Throw -ExpectedMessage "*already running*"
        }
        finally {
            $lock.Dispose()
        }

        (Get-Content -LiteralPath (Join-Path $script:finalDir "previous-release.txt")) | Should -Be "keep"
        (Get-Content -LiteralPath $script:currentPath) | Should -Be '{"version":"1.0.0","commit":"old"}'
    }

    It "keeps identical version and commit releases isolated by toolchain" {
        $llvmRoot = Join-Path $script:testRoot "dist/windows/amd64/llvm"
        $llvmStage = Join-Path $llvmRoot ".staging/run-llvm"
        New-Item -ItemType Directory -Path (Join-Path $script:stageDir "ssa_consulta_rapida-standalone"), (Join-Path $llvmStage "ssa_consulta_rapida-standalone") -Force | Out-Null
        foreach ($root in @($script:stageDir, $llvmStage)) {
            Set-Content -LiteralPath (Join-Path $root "ssa_consulta_rapida.exe") -Value "portable" -Encoding ASCII
            Set-Content -LiteralPath (Join-Path $root "ssa_consulta_rapida-installer.exe") -Value "installer" -Encoding ASCII
            Set-Content -LiteralPath (Join-Path $root "ssa_consulta_rapida.zip") -Value "zip" -Encoding ASCII
            Set-Content -LiteralPath (Join-Path $root "ssa_consulta_rapida-standalone/ssa_consulta_rapida.exe") -Value "standalone" -Encoding ASCII
        }

        Publish-WindowsReleaseSet -StageDir $script:stageDir -DistRoot $script:distRoot -Version "1.2.3" -CommitSha "abc123" -RepoRoot $script:testRoot -Platform windows -Architecture amd64 -Toolchain msvc -TaggedRelease $false @script:releaseMetadata
        Publish-WindowsReleaseSet -StageDir $llvmStage -DistRoot $llvmRoot -Version "1.2.3" -CommitSha "abc123" -RepoRoot $script:testRoot -Platform windows -Architecture amd64 -Toolchain llvm -TaggedRelease $false @script:releaseMetadata

        (Test-Path -LiteralPath (Join-Path $script:distRoot "final/ssa_consulta_rapida.exe")) | Should -BeTrue
        (Test-Path -LiteralPath (Join-Path $llvmRoot "final/ssa_consulta_rapida.exe")) | Should -BeTrue
    }

    It "retains the prior release and preserves invalid release identities during pruning" {
        $secondStage = Join-Path $script:distRoot ".staging/run-456"
        $invalidRelease = Join-Path $script:distRoot "releases/manual-copy"
        foreach ($root in @($script:stageDir, $secondStage)) {
            New-Item -ItemType Directory -Path (Join-Path $root "ssa_consulta_rapida-standalone") -Force | Out-Null
            Set-Content -LiteralPath (Join-Path $root "ssa_consulta_rapida.exe") -Value $root -Encoding ASCII
            Set-Content -LiteralPath (Join-Path $root "ssa_consulta_rapida-installer.exe") -Value "installer" -Encoding ASCII
            Set-Content -LiteralPath (Join-Path $root "ssa_consulta_rapida.zip") -Value "zip" -Encoding ASCII
            Set-Content -LiteralPath (Join-Path $root "ssa_consulta_rapida-standalone/ssa_consulta_rapida.exe") -Value "standalone" -Encoding ASCII
        }
        New-Item -ItemType Directory -Path $invalidRelease -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $invalidRelease "release.json") -Value '{"release":"manual-copy","platform":"windows"}' -Encoding ASCII

        Publish-WindowsReleaseSet -StageDir $script:stageDir -DistRoot $script:distRoot -Version "1.2.3" -CommitSha "abc123" -RepoRoot $script:testRoot -Platform windows -Architecture amd64 -Toolchain msvc -TaggedRelease $false @script:releaseMetadata
        Publish-WindowsReleaseSet -StageDir $secondStage -DistRoot $script:distRoot -Version "1.2.4" -CommitSha "def456" -RepoRoot $script:testRoot -Platform windows -Architecture amd64 -Toolchain msvc -TaggedRelease $false @script:releaseMetadata

        (Get-Content -LiteralPath (Join-Path $script:distRoot "previous.json") -Raw) | Should -Match '"release":  "1.2.3-abc123-windows-amd64-msvc"'
        (Get-Content -LiteralPath (Join-Path $script:distRoot "current.json") -Raw) | Should -Match '"release":  "1.2.4-def456-windows-amd64-msvc"'
        (Test-Path -LiteralPath (Join-Path $script:distRoot "previous/ssa_consulta_rapida.exe")) | Should -BeTrue
        (Test-Path -LiteralPath (Join-Path $invalidRelease "release.json")) | Should -BeTrue
    }
}
