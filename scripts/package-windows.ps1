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
  Artifact dir: dist\windows\<arch>\
  Required build output: build\<preset>\SsaConsultaRapida QML module must exist.
  Optional parameters: -Preset, -ProjectRoot, -Arch, -DistDir, -Version,
    -QtDir, -QtRoot, -QtSubdir
  Optional switch: -SkipTests
  Latest pointers: symbolic links when permitted, copied fallback otherwise.

  Generated files:
  - ssa_consulta_rapida-<version>-<arch>-windows.zip
  - latest-binary points to the portable application executable
  - run-ssa_consulta_rapida.bat for quick start from extracted zip
  - Optional: installer .exe when MakeNSIS is installed
    - ssa_consulta_rapida-<version>-<arch>-windows-installer.exe
    - latest.exe points to the installer only when it exists
"@
    Write-Output $helpText
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
$buildDir = Join-Path $repoRoot "build\$preset"

$configureScript = Join-Path $repoRoot "tools\configure-dev.ps1"
$version = Resolve-PackageVersion -RepoRoot $repoRoot -ExplicitVersion $Version
$arch = Resolve-WindowsArch -RequestedArch $Arch
$distRoot = if ($DistDir) { $DistDir } else { Join-Path $repoRoot "dist\windows" }
$artifactRoot = Join-Path $distRoot $arch
$artifactName = "ssa_consulta_rapida-$version-$arch-windows"
$artifactDir = Join-Path $artifactRoot $artifactName
$zipPath = Join-Path $artifactRoot ("{0}.zip" -f $artifactName)
$installerPath = Join-Path $artifactRoot ("{0}-installer.exe" -f $artifactName)
$nsiPath = Join-Path $artifactRoot "installer.nsi"

$configureParams = @{ Preset = $preset }
if ($QtDir) {
    $configureParams.QtDir = $QtDir
}
if ($QtRoot) {
    $configureParams.QtRoot = $QtRoot
}
if ($QtSubdir) {
    $configureParams.QtSubdir = $QtSubdir
}
if ($CmakeExtraArgs -and $CmakeExtraArgs.Count -gt 0) {
    $configureParams.CmakeExtraArgs = $CmakeExtraArgs
}
& $configureScript @configureParams | Out-Null

cmake --build --preset $preset
if (-not $SkipTests) {
    ctest --preset $preset --output-on-failure
}

$binary = Join-Path $buildDir "ssa_consulta_rapida.exe"
if (-not (Test-Path $binary)) {
    throw "Binary not found after build: $binary"
}

if (Test-Path $artifactDir) {
    Remove-Item $artifactDir -Recurse -Force
}
if (Test-Path $installerPath) {
    Remove-Item $installerPath -Force
}
New-Item -ItemType Directory -Path $artifactDir -Force | Out-Null

Copy-Item $binary (Join-Path $artifactDir "ssa_consulta_rapida.exe")
$sqliteRuntime = Join-Path $buildDir "sqlite3.dll"
if (-not (Test-Path -LiteralPath $sqliteRuntime -PathType Leaf)) {
    throw "SQLite runtime not found after build: $sqliteRuntime"
}
Copy-Item $sqliteRuntime (Join-Path $artifactDir "sqlite3.dll")
Copy-Item (Join-Path $repoRoot "resources\app_icon.ico") (Join-Path $artifactDir "app_icon.ico")
Copy-Item (Join-Path $repoRoot "THIRD_PARTY_NOTICES.md") $artifactDir
Copy-Item (Join-Path $repoRoot "third_party\tinted-themes\LICENSE") (Join-Path $artifactDir "TINTED_SCHEMES_LICENSE.txt")

# Resolver windeployqt da MESMA Qt usada no build (ler CMakeCache.txt Qt6_DIR),
# para evitar mismatch de versao entre as DLLs copiadas pelo windeployqt e o
# binario. Ordem: CMakeCache (verdade do build) -> Qt6_DIR -> QT_DIR -> PATH.
$qtBinDir = $null
$cmakeCache = Join-Path $buildDir "CMakeCache.txt"
if (Test-Path $cmakeCache) {
    $cacheQtLine = Select-String -Path $cmakeCache -Pattern '^Qt6_DIR:PATH=(.+)$' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cacheQtLine) {
        $cacheQtDir = (Split-Path (Split-Path $cacheQtLine.Matches[0].Groups[1].Value.Trim()))
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
        $deployArgs = @("--qmldir", $qmlDir) + $deployArgs
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

if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Set-Content -Path (Join-Path $artifactDir "run-ssa_consulta_rapida.bat") -Value '@echo off
setlocal
"%~dp0ssa_consulta_rapida.exe" %*' -Encoding UTF8

$tarCommand = Get-Command "tar.exe" -ErrorAction SilentlyContinue
if (-not $tarCommand) {
    $tarCommand = Get-Command "tar" -ErrorAction SilentlyContinue
}
if (-not $tarCommand) {
    throw "tar.exe not found. Install Windows tar support or use a Developer PowerShell where tar.exe is available."
}
& $tarCommand.Source -a -c -f "$zipPath" -C "$artifactRoot" $artifactName

Set-LatestArtifactLink -DistRoot $artifactRoot -ArtifactDir $artifactDir
Set-LatestArtifactAlias -DistRoot $artifactRoot -AliasName "latest.zip" -TargetPath $zipPath
Set-LatestArtifactAlias -DistRoot $artifactRoot -AliasName "latest-binary" -TargetPath (Join-Path $artifactDir "ssa_consulta_rapida.exe")
Set-LatestArtifactAlias -DistRoot $artifactRoot -AliasName "latest-run.bat" -TargetPath (Join-Path $artifactDir "run-ssa_consulta_rapida.bat")

$nsisScript = @"
!define APP_NAME "SSA Consulta Rapida"
!define APP_VERSION "$version"
!define APP_PUBLISHER "SSA Consulta Rapida"
!define APP_EXE "ssa_consulta_rapida.exe"
!define APP_DIR "$artifactDir"

OutFile "$installerPath"
InstallDir "`$PROGRAMFILES64\ssa-consulta-rapida-cpp"
RequestExecutionLevel admin
Page directory
Page instfiles
Section
SetOutPath "`$INSTDIR"
File /r "$artifactDir\*.*"
CreateShortCut "`$DESKTOP\\${APP_NAME}.lnk" "`$INSTDIR\\ssa_consulta_rapida.exe" "" "`$INSTDIR\\app_icon.ico"
SectionEnd
"@

$makeNsis = Get-Command "makensis.exe" -ErrorAction SilentlyContinue
if ($makeNsis) {
    if (-not $hasQtCore) {
        Write-Warning "Skipping installer because required Qt runtime DLLs were not staged."
    } else {
        Set-Content -Path $nsiPath -Value $nsisScript -Encoding UTF8
        & $makeNsis.Source $nsiPath | Out-Null
        Remove-Item $nsiPath -Force
    }
}

if (Test-Path $installerPath) {
    Set-LatestArtifactAlias -DistRoot $artifactRoot -AliasName "latest.exe" -TargetPath $installerPath
    Set-LatestArtifactAlias -DistRoot $artifactRoot -AliasName "latest-installer.exe" -TargetPath $installerPath
}

Write-Output "Windows release artifacts generated:"
Write-Output "  project_root: $repoRoot"
Write-Output "  version: $version"
Write-Output "  preset: $preset"
Write-Output "  architecture: $arch"
Write-Output "  package: $zipPath"
Write-Output "  latest_zip: $artifactRoot/latest.zip"
Write-Output "  latest_binary: $artifactRoot/latest-binary"
Write-Output "  latest_run: $artifactRoot/latest-run.bat"
Write-Output "  latest: $artifactRoot/latest"
if (Test-Path $installerPath) {
    Write-Output "  latest_exe: $artifactRoot/latest.exe"
    Write-Output "  latest_installer: $artifactRoot/latest-installer.exe"
    Write-Output "  installer: $installerPath"
}
