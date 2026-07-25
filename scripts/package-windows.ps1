[CmdletBinding()]
param(
    [string]$Preset = "release",
    [switch]$Help,
    [string]$ProjectRoot = "",
    [string]$Arch = "",
    [string]$DistDir = "",
    [string]$Version = "",
    [string]$QtDir = "",
    [string]$QtRoot = "",
    [string]$QtSubdir = "",
    [ValidateSet("auto", "msvc", "llvm", "mingw", "llvm-mingw")]
    [string]$Toolchain = "auto",
    [switch]$SkipTests,
    [string[]]$CmakeExtraArgs = @()
)

$ErrorActionPreference = "Stop"

function Show-Help {
    [string]$helpText = @"
Usage:
  .\package-windows.ps1
  .\scripts\package-windows.ps1

Build and package Windows release artifacts.

Defaults:
  Preset: release
  Architecture: amd64 (x64-windows) or arm64
  Artifact dir: dist\windows\<arch>\<toolchain>\
  Required build output: build\windows\<arch>\<toolchain>\<qt-kit>\<preset>\SsaConsultaRapida must exist.
  Optional parameters: -Preset, -ProjectRoot, -Arch, -DistDir, -Version,
    -Toolchain, -QtDir, -QtRoot, -QtSubdir
  Optional switch: -SkipTests

  Generated files:
  - final\<repo>-<version>-<commit>-windows-<arch>-<toolchain>.exe
  - final\<repo>-<version>-<commit>-windows-<arch>-<toolchain>-installer.exe
  - final\<repo>-<version>-<commit>-windows-<arch>-<toolchain>.zip
  - final\<repo>-<version>-<commit>-windows-<arch>-<toolchain>-standalone\<repo>-<version>-<commit>-windows-<arch>-<toolchain>.exe
  - releases\ contains immutable tagged delivery sets
  - current.json and previous.json identify complete releases
"@
    Write-Output $helpText
}

function Resolve-MakeNsisPath {
    $command = Get-Command "makensis.exe" -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @()
    if ($env:NSIS_HOME) { $candidates += Join-Path $env:NSIS_HOME "makensis.exe" }
    if ($env:NSISDIR) { $candidates += Join-Path $env:NSISDIR "makensis.exe" }
    if ($env:ProgramFiles) { $candidates += Join-Path $env:ProgramFiles "NSIS\makensis.exe" }
    if (${env:ProgramFiles(x86)}) {
        $candidates += Join-Path ${env:ProgramFiles(x86)} "NSIS\makensis.exe"
    }
    $candidates += "C:\Tools\NSIS\makensis.exe"
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }
    return $null
}

function Get-CMakeCacheValue {
    param(
        [Parameter(Mandatory)]
        [string]$CachePath,
        [Parameter(Mandatory)]
        [string]$Name
    )

    $line = Select-String -LiteralPath $CachePath -Pattern "^$([regex]::Escape($Name)):[^=]+=(.+)$" |
        Select-Object -First 1
    if ($line) {
        return $line.Matches[0].Groups[1].Value.Trim()
    }
    return ""
}

function Resolve-VisualCppRuntimeDirectory {
    param(
        [Parameter(Mandatory)]
        [string]$CachePath,
        [Parameter(Mandatory)]
        [ValidateSet("amd64", "arm64")]
        [string]$Architecture,
        [Parameter(Mandatory)]
        [ValidateSet("msvc", "llvm")]
        [string]$EffectiveToolchain
    )

    $compiler = Get-CMakeCacheValue -CachePath $CachePath -Name "CMAKE_CXX_COMPILER"
    $compilerLeaf = Split-Path $compiler.Replace('/', '\') -Leaf
    if ($EffectiveToolchain -eq "msvc") {
        if (-not $compilerLeaf.Equals("cl.exe", [StringComparison]::OrdinalIgnoreCase)) {
            throw "MSVC package requires CMAKE_CXX_COMPILER=cl.exe, found: $compiler"
        }
        $compilerMatch = [regex]::Match($compiler.Replace('/', '\'),
            '^(?<toolset>.+\\VC\\Tools\\MSVC\\[^\\]+)\\bin\\',
            [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        if (-not $compilerMatch.Success) {
            throw "MSVC CMAKE_CXX_COMPILER is outside a Visual C++ toolset: $compiler"
        }
        $toolsetRoot = $compilerMatch.Groups['toolset'].Value
    } else {
        if (-not $compilerLeaf.Equals("clang-cl.exe", [StringComparison]::OrdinalIgnoreCase)) {
            throw "LLVM package requires CMAKE_CXX_COMPILER=clang-cl.exe, found: $compiler"
        }
        if (-not $env:VCToolsInstallDir) {
            throw "LLVM package requires VCToolsInstallDir from the active Developer Shell."
        }
        $toolsetRoot = $env:VCToolsInstallDir
    }

    $runtimeArchitecture = if ($Architecture -eq "amd64") { "x64" } else { "arm64" }
    $normalizedToolset = $toolsetRoot.Replace('/', '\').TrimEnd('\')
    $toolsetMatch = [regex]::Match($normalizedToolset,
        '^(?<vcRoot>.+\\VC)\\Tools\\MSVC\\(?<version>[^\\]+)$',
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if (-not $toolsetMatch.Success) {
        throw "Visual C++ toolset has an unsupported path: $toolsetRoot"
    }
    if ($EffectiveToolchain -eq "llvm") {
        if (-not $env:VCToolsVersion) {
            throw "LLVM package requires VCToolsVersion from the active Developer Shell."
        }
        if (-not $env:VCToolsVersion.Equals($toolsetMatch.Groups['version'].Value, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Developer Shell VCToolsInstallDir and VCToolsVersion disagree."
        }
    }

    $runtimeDirectory = Join-Path $toolsetMatch.Groups['vcRoot'].Value `
        "Redist\MSVC\$($toolsetMatch.Groups['version'].Value)\$runtimeArchitecture"
    $crtDirectory = Get-ChildItem -LiteralPath $runtimeDirectory -Directory -Filter "Microsoft.VC*.CRT" `
        -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $crtDirectory) {
        throw "Matching Visual C++ runtime for the configured toolset was not found: $runtimeDirectory"
    }
    $dumpbinPath = Join-Path $normalizedToolset "bin\Hostx64\$runtimeArchitecture\dumpbin.exe"
    if (-not (Test-Path -LiteralPath $dumpbinPath -PathType Leaf)) {
        throw "dumpbin.exe for the configured Visual C++ toolset was not found: $dumpbinPath"
    }
    return [PSCustomObject]@{
        CrtDirectory = $crtDirectory.FullName
        DumpbinPath = $dumpbinPath
    }
}

function Copy-VisualCppRuntime {
    param(
        [Parameter(Mandatory)]
        [PSCustomObject]$Runtime,
        [Parameter(Mandatory)]
        [string]$Destination
    )

    $runtimeFiles = @(Get-ChildItem -LiteralPath $Runtime.CrtDirectory -File -Filter "*.dll")
    if ($runtimeFiles.Count -eq 0) {
        throw "Matching Visual C++ runtime contains no DLLs: $($Runtime.CrtDirectory)"
    }
    $debugRuntimeFiles = @($runtimeFiles | Where-Object { $_.Name -match '(?i)d\.dll$' })
    if ($debugRuntimeFiles.Count -gt 0) {
        throw "Matching Visual C++ runtime contains Debug DLLs: $($debugRuntimeFiles.Name -join ', ')"
    }
    Copy-Item -LiteralPath $runtimeFiles.FullName -Destination $Destination

    $requiredRuntimeFiles = @(
        "MSVCP140.dll",
        "MSVCP140_ATOMIC_WAIT.dll",
        "VCRUNTIME140.dll",
        "VCRUNTIME140_1.dll"
    )
    $missingRuntimeFiles = @($requiredRuntimeFiles | Where-Object {
            -not (Test-Path -LiteralPath (Join-Path $Destination $_) -PathType Leaf)
        })
    if ($missingRuntimeFiles.Count -gt 0) {
        throw "Matching Visual C++ runtime did not stage required DLLs: $($missingRuntimeFiles -join ', ')"
    }
    foreach ($runtimeFile in $runtimeFiles) {
        $stagedRuntimeFile = Join-Path $Destination $runtimeFile.Name
        $sourceHash = (Get-FileHash -LiteralPath $runtimeFile.FullName -Algorithm SHA256).Hash
        $stagedHash = (Get-FileHash -LiteralPath $stagedRuntimeFile -Algorithm SHA256).Hash
        if ($sourceHash -ne $stagedHash) {
            throw "Staged Visual C++ runtime hash mismatch: $($runtimeFile.Name)"
        }
    }
}

function Test-VisualCppRuntimeImport {
    param(
        [Parameter(Mandatory)]
        [PSCustomObject]$Runtime,
        [Parameter(Mandatory)]
        [string]$Executable
    )

    $imports = @(& $Runtime.DumpbinPath /imports $Executable 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin failed to inspect Windows runtime imports with exit code $LASTEXITCODE."
    }
    $importText = $imports -join "`n"
    $requiredRuntimeFiles = @(
        "MSVCP140.dll",
        "MSVCP140_ATOMIC_WAIT.dll",
        "VCRUNTIME140.dll",
        "VCRUNTIME140_1.dll"
    )
    $missingImports = @($requiredRuntimeFiles | Where-Object {
            $importText -notmatch "(?im)^\s*$([regex]::Escape($_))\s*$"
        })
    if ($missingImports.Count -gt 0) {
        throw "Executable is missing required Visual C++ runtime imports: $($missingImports -join ', ')"
    }
    $debugImports = @([regex]::Matches($importText, '(?im)^\s*(?:MSVCP|VCRUNTIME)[^\s]*D\.dll\s*$') |
            ForEach-Object { $_.Value.Trim() })
    if ($debugImports.Count -gt 0) {
        throw "Executable imports Debug Visual C++ runtime DLLs: $($debugImports -join ', ')"
    }
}

if ($Help) {
    Show-Help
    return
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = if ($ProjectRoot) { (Resolve-Path $ProjectRoot).Path } else { (Resolve-Path (Join-Path $scriptDir "..")).Path }
$commonHelpers = Join-Path $scriptDir "lib\package_common.ps1"
. $commonHelpers

$preset = if ($Preset) { $Preset } else { "release" }
$buildScript = Join-Path $repoRoot "scripts\build-windows.ps1"
$version = Resolve-PackageVersion -RepoRoot $repoRoot -ExplicitVersion $Version
if ($version -notmatch '^[A-Za-z0-9._-]+$') {
    throw "Windows release version contains unsupported characters: $version"
}
$arch = Resolve-WindowsArch -RequestedArch $Arch
$layout = Resolve-WindowsBuildLayout -RepoRoot $repoRoot -Preset $preset -Arch $arch `
    -QtDir $QtDir -QtSubdir $QtSubdir -Toolchain $Toolchain
$buildDir = $layout.BuildDir
$effectiveToolchain = $layout.EffectiveToolchain
$repoName = Split-Path $repoRoot -Leaf
$distRoot = if ($DistDir) { $DistDir } else { Join-Path $repoRoot "dist\windows" }
$artifactRoot = Join-Path (Join-Path $distRoot $arch) $effectiveToolchain
$stagingRoot = Join-Path $artifactRoot ".staging"
$runStage = Join-Path $stagingRoot "$PID-$([guid]::NewGuid().ToString('N').Substring(0, 8))"
$releaseStage = Join-Path $runStage "release"
$artifactDir = Join-Path $runStage "app"
$finalRoot = Join-Path $artifactRoot "final"
$portableNsiPath = Join-Path $runStage "$repoName-portable.nsi"
$installerNsiPath = Join-Path $runStage "$repoName-installer.nsi"
$makeNsisPath = Resolve-MakeNsisPath
if (-not $makeNsisPath) {
    throw "MakeNSIS not found. NSIS is required for the portable EXE and installer."
}

$buildParams = @{
    Preset = $preset
    ProjectRoot = $repoRoot
    Arch = $arch
    Toolchain = $effectiveToolchain
}
if ($QtDir) {
    $buildParams.QtDir = $QtDir
}
if ($QtRoot) {
    $buildParams.QtRoot = $QtRoot
}
if ($QtSubdir) {
    $buildParams.QtSubdir = $QtSubdir
}
if ($CmakeExtraArgs -and $CmakeExtraArgs.Count -gt 0) {
    $buildParams.CmakeExtraArgs = $CmakeExtraArgs
}
if ($SkipTests) {
    $buildParams.Target = "ssa_consulta_rapida"
}
try {
& $buildScript @buildParams
if ($LASTEXITCODE -ne 0) {
    throw "Windows build failed with exit code $LASTEXITCODE."
}
if (-not $SkipTests) {
    ctest --test-dir $buildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "Windows tests failed with exit code $LASTEXITCODE."
    }
}

$commitSha = @(& git -C $repoRoot rev-parse --short=12 HEAD) | Select-Object -First 1
if ($LASTEXITCODE -ne 0 -or -not $commitSha) {
    throw "Could not resolve Git commit for Windows release publication."
}
$commitSha = $commitSha.Trim()
$artifactName = "$repoName-$version-$commitSha-windows-$arch-$effectiveToolchain"
$zipPath = Join-Path $releaseStage "$artifactName.zip"
$installerPath = Join-Path $releaseStage "$artifactName-installer.exe"
$portableStagePath = Join-Path $releaseStage "$artifactName.exe"
$standaloneStagePath = Join-Path $releaseStage "$artifactName-standalone"
$standaloneExecutableName = "$artifactName.exe"

$binary = Join-Path $buildDir "ssa_consulta_rapida.exe"
if (-not (Test-Path $binary)) {
    throw "Binary not found after build: $binary"
}

New-Item -ItemType Directory -Path $artifactDir, $releaseStage -Force | Out-Null

Copy-Item $binary (Join-Path $artifactDir "ssa_consulta_rapida.exe")
$sqliteRuntime = Join-Path $buildDir "sqlite3.dll"
if (-not (Test-Path -LiteralPath $sqliteRuntime -PathType Leaf)) {
    throw "SQLite runtime not found after build: $sqliteRuntime"
}
Copy-Item $sqliteRuntime (Join-Path $artifactDir "sqlite3.dll")
Copy-Item (Join-Path $repoRoot "resources\app_icon.ico") (Join-Path $artifactDir "app_icon.ico")
Copy-Item (Join-Path $repoRoot "THIRD_PARTY_NOTICES.md") $artifactDir
Copy-Item (Join-Path $repoRoot "third_party\tinted-themes\LICENSE") (Join-Path $artifactDir "TINTED_SCHEMES_LICENSE.txt")

$cmakeCache = Join-Path $buildDir "CMakeCache.txt"

# Resolver windeployqt da MESMA Qt usada no build (ler CMakeCache.txt Qt6_DIR),
# para evitar mismatch de versao entre as DLLs copiadas pelo windeployqt e o
# binario. Ordem: CMakeCache (verdade do build) -> Qt6_DIR -> QT_DIR -> PATH.
$qtBinDir = $null
if (Test-Path $cmakeCache) {
    $cacheQtLine = Select-String -Path $cmakeCache -Pattern '^Qt6_DIR:PATH=(.+)$' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cacheQtLine) {
        $cacheQtDir = (Split-Path (Split-Path (Split-Path $cacheQtLine.Matches[0].Groups[1].Value.Trim())))
        if ($cacheQtDir -and (Test-Path (Join-Path $cacheQtDir "bin\windeployqt.exe"))) {
            $qtBinDir = Join-Path $cacheQtDir "bin"
        }
    }
}
if (-not $qtBinDir -and (Test-Path Env:\Qt6_DIR)) {
    $qt6Dir = $env:Qt6_DIR
    # Qt6_DIR aponta para <prefix>/lib/cmake/Qt6 -> sobe 3 niveis.
    $qt6Prefix = (Split-Path (Split-Path (Split-Path $qt6Dir)))
    if (Test-Path (Join-Path $qt6Prefix "bin\windeployqt.exe")) {
        $qtBinDir = Join-Path $qt6Prefix "bin"
    }
}
if (-not $qtBinDir -and (Test-Path Env:\QT_DIR)) {
    $qtDir = $env:QT_DIR
    if (Test-Path (Join-Path $qtDir "bin\windeployqt.exe")) {
        $qtBinDir = Join-Path $qtDir "bin"
    }
}
if (-not $qtBinDir) {
    $commandCandidate = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
    if ($commandCandidate) {
        $qtBinDir = (Split-Path $commandCandidate.Source)
    }
}

if ($qtBinDir -and (Test-Path (Join-Path $qtBinDir "windeployqt.exe"))) {
    $hasQtCore = $false
    $previousPath = $env:Path
    try {
        $env:Path = "$qtBinDir;$previousPath"
        $deployArgs = @((Join-Path $artifactDir "ssa_consulta_rapida.exe"))
        $qmlDir = Join-Path $buildDir "SsaConsultaRapida"
        if (-not (Test-Path $qmlDir)) {
            throw "Generated QML module not found: $qmlDir"
        }
        $deployArgs = @("--no-compiler-runtime", "--qmldir", $qmlDir) + $deployArgs
        & windeployqt.exe @deployArgs | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "windeployqt failed with exit code $LASTEXITCODE."
        }
        $robocopy = Get-Command "robocopy.exe" -ErrorAction SilentlyContinue
        if (-not $robocopy) {
            throw "robocopy.exe not found. Cannot mirror QML resources efficiently."
        }
        $qmlArtifactDir = Join-Path $artifactDir "SsaConsultaRapida"
        New-Item -ItemType Directory -Path $qmlArtifactDir -Force | Out-Null
        & $robocopy.Source $qmlDir $qmlArtifactDir /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
        if ($LASTEXITCODE -gt 7) {
            throw "robocopy failed while mirroring QML resources with exit code $LASTEXITCODE."
        }
        $hasQtCore = [bool](Get-ChildItem -Path $artifactDir -Filter "Qt6Core*.dll" -File -ErrorAction SilentlyContinue)
        if (-not $hasQtCore) {
            throw "windeployqt did not copy Qt6Core runtime DLLs. Aborting package."
        }
    }
    finally {
        $env:Path = $previousPath
    }
} else {
    throw "windeployqt.exe not found. Cannot generate a self contained Windows package."
}

if ($effectiveToolchain -eq "msvc" -or $effectiveToolchain -eq "llvm") {
    $visualCppRuntime = Resolve-VisualCppRuntimeDirectory -CachePath $cmakeCache -Architecture $arch `
        -EffectiveToolchain $effectiveToolchain
    Copy-VisualCppRuntime -Runtime $visualCppRuntime -Destination $artifactDir
    Test-VisualCppRuntimeImport -Runtime $visualCppRuntime -Executable (Join-Path $artifactDir "ssa_consulta_rapida.exe")
}

Set-Content -Path (Join-Path $artifactDir "run-ssa_consulta_rapida.bat") -Value '@echo off
setlocal
"%~dp0ssa_consulta_rapida.exe" %*' -Encoding UTF8

$portableNsisScript = @"
!include "FileFunc.nsh"
Name "SSA Consulta Rapida Cpp Portable"
OutFile "$portableStagePath"
RequestExecutionLevel user
SilentInstall silent
AutoCloseWindow true
Icon "$artifactDir\app_icon.ico"
SetCompressor /SOLID lzma
Section
InitPluginsDir
SetOutPath "`$PLUGINSDIR\app"
File /r "$artifactDir\*.*"
`${GetParameters} `$0
ExecWait '"`$PLUGINSDIR\app\ssa_consulta_rapida.exe" `$0' `$1
SetErrorLevel `$1
SectionEnd
"@
Set-Content -Path $portableNsiPath -Value $portableNsisScript -Encoding UTF8
& $makeNsisPath $portableNsiPath | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $portableStagePath -PathType Leaf)) {
    throw "MakeNSIS failed to generate the portable executable: $portableStagePath"
}

$uninstallFileCommands = @(Get-ChildItem -LiteralPath $artifactDir -File -Recurse | ForEach-Object {
        $relativePath = $_.FullName.Substring($artifactDir.Length).TrimStart('\')
        'Delete "$INSTDIR\{0}"' -f $relativePath
    }) -join "`r`n"
$uninstallDirectoryCommands = @(Get-ChildItem -LiteralPath $artifactDir -Directory -Recurse |
    Sort-Object { $_.FullName.Length } -Descending |
    ForEach-Object {
        $relativePath = $_.FullName.Substring($artifactDir.Length).TrimStart('\')
        'RMDir "$INSTDIR\{0}"' -f $relativePath
    }) -join "`r`n"

$nsisScript = @"
!define APP_NAME "SSA Consulta Rapida"
!define APP_VERSION "$version"
!define APP_PUBLISHER "SSA Consulta Rapida"
!define APP_EXE "ssa_consulta_rapida.exe"
!define APP_DIR "$artifactDir"

Name "`${APP_NAME}"
OutFile "$installerPath"
InstallDir "`$PROGRAMFILES64\ssa-consulta-rapida-cpp"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles
Function un.onInit
SetRegView 64
ReadRegStr `$0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SSAConsultaRapida" "InstallLocation"
StrCmp `$0 "`$INSTDIR" valid_install
MessageBox MB_ICONSTOP|MB_OK "Installer identity mismatch. Uninstall aborted."
Abort
valid_install:
FunctionEnd
Section
SetOutPath "`$INSTDIR"
File /r "$artifactDir\*.*"
CreateShortCut "`$DESKTOP\\`${APP_NAME}.lnk" "`$INSTDIR\\ssa_consulta_rapida.exe" "" "`$INSTDIR\\app_icon.ico"
WriteUninstaller "`$INSTDIR\\uninstall.exe"
SetRegView 64
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SSAConsultaRapida" "DisplayName" "`${APP_NAME}"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SSAConsultaRapida" "DisplayVersion" "`${APP_VERSION}"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SSAConsultaRapida" "Publisher" "`${APP_PUBLISHER}"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SSAConsultaRapida" "DisplayIcon" "`$INSTDIR\\app_icon.ico"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SSAConsultaRapida" "InstallLocation" "`$INSTDIR"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SSAConsultaRapida" "UninstallString" '"`$INSTDIR\\uninstall.exe"'
SectionEnd
Section "Uninstall"
SetRegView 64
Delete "`$DESKTOP\\`${APP_NAME}.lnk"
$uninstallFileCommands
Delete "`$INSTDIR\\uninstall.exe"
DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SSAConsultaRapida"
$uninstallDirectoryCommands
RMDir "`$INSTDIR"
SectionEnd
"@

if (-not $hasQtCore) {
    throw "Required Qt runtime DLLs were not staged for the installer."
}
Set-Content -Path $installerNsiPath -Value $nsisScript -Encoding UTF8
& $makeNsisPath $installerNsiPath | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $installerPath -PathType Leaf)) {
    throw "MakeNSIS failed to generate the installer: $installerPath"
}

New-Item -ItemType Directory -Path $standaloneStagePath -Force | Out-Null
$standaloneRuntimePath = Join-Path $standaloneStagePath "ssa_consulta_rapida.exe"
Copy-Item -Path (Join-Path $artifactDir "*") -Destination $standaloneStagePath -Recurse
Rename-Item -LiteralPath $standaloneRuntimePath `
    -NewName $standaloneExecutableName
Set-Content -LiteralPath (Join-Path $standaloneStagePath "README.txt") -Encoding UTF8 -Value @"
Native Windows executable. Run:
  .\$standaloneExecutableName --db <path-to-ssas.db>

The Qt, QML and SQLite runtime files must remain beside the executable.
"@

$zipSourceDir = Join-Path $runStage $artifactName
Rename-Item -LiteralPath $artifactDir -NewName $artifactName
$tarCommand = Get-Command "tar.exe" -ErrorAction SilentlyContinue
if (-not $tarCommand) {
    $tarCommand = Get-Command "tar" -ErrorAction SilentlyContinue
}
if (-not $tarCommand) {
    throw "tar.exe not found. Install Windows tar support or use a Developer PowerShell where tar.exe is available."
}
& $tarCommand.Source -a -c -f "$zipPath" -C "$runStage" $artifactName
if ($LASTEXITCODE -ne 0) {
    throw "tar failed to generate the Windows package with exit code $LASTEXITCODE."
}
if (-not (Test-Path -LiteralPath $zipSourceDir -PathType Container)) {
    throw "Windows ZIP source directory was not staged: $zipSourceDir"
}

$compilerLine = Select-String -LiteralPath $cmakeCache -Pattern '^CMAKE_CXX_COMPILER:[^=]+=(.+)$' |
    Select-Object -First 1
$linkerLine = Select-String -LiteralPath $cmakeCache -Pattern '^CMAKE_LINKER:[^=]+=(.+)$' |
    Select-Object -First 1
if (-not $compilerLine -or -not $linkerLine) {
    throw "CMake cache is missing the effective compiler or linker for the Windows package."
}
$compiler = $compilerLine.Matches[0].Groups[1].Value.Trim()
$linker = $linkerLine.Matches[0].Groups[1].Value.Trim()
$compilerVersionLine = Select-String -LiteralPath $cmakeCache -Pattern '^CMAKE_CXX_COMPILER_VERSION:[^=]+=(.+)$' |
    Select-Object -First 1
$compilerVersion = if ($compilerVersionLine) {
    $compilerVersionLine.Matches[0].Groups[1].Value.Trim()
} else {
    ""
}
$taggedRelease = Test-ExactReleaseTag -RepoRoot $repoRoot -Version $version
Publish-WindowsReleaseSet -StageDir $releaseStage -DistRoot $artifactRoot `
    -Version $version -CommitSha $commitSha -RepoRoot $repoRoot -Platform "windows" `
    -Architecture $arch -Toolchain $effectiveToolchain -TaggedRelease $taggedRelease `
    -Preset $preset -QtKit $layout.QtKit -Compiler $compiler -CompilerVersion $compilerVersion -Linker $linker
}
finally {
    Get-Item -LiteralPath $runStage -Force -ErrorAction SilentlyContinue |
        Remove-Item -Recurse -Force
    if ((Test-Path -LiteralPath $stagingRoot -PathType Container) -and
        -not (Get-ChildItem -LiteralPath $stagingRoot -Force | Select-Object -First 1)) {
        Remove-Item -LiteralPath $stagingRoot -Force
    }
    Get-Item -LiteralPath $buildDir -Force -ErrorAction SilentlyContinue |
        Remove-Item -Recurse -Force
}

Write-Output "Windows release artifacts generated:"
Write-Output "  project_root: $repoRoot"
Write-Output "  version: $version"
Write-Output "  preset: $preset"
Write-Output "  architecture: $arch"
Write-Output "  toolchain: $effectiveToolchain"
Write-Output "  package: $finalRoot\$artifactName.zip"
Write-Output "  portable: $finalRoot\$artifactName.exe"
Write-Output "  installer: $finalRoot\$artifactName-installer.exe"
Write-Output "  standalone: $finalRoot\$artifactName-standalone\$standaloneExecutableName"
Write-Output "  final_root: $finalRoot"
if ($taggedRelease) {
    Write-Output "  release: $artifactRoot\releases\$version-$commitSha-windows-$arch-$effectiveToolchain"
}
Write-Output "  current: $artifactRoot\current.json"
Write-Output "  tagged_release: $taggedRelease"
