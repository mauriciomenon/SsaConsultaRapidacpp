[CmdletBinding()]
param(
    [string]$Preset = "release",
    [switch]$Help,
    [string]$ProjectRoot = "",
    [string]$Arch = "",
    [string]$DistDir = "",
    [string]$Version = "",
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

function Show-Help {
    [string]$helpText = @"
Usage:
  .\scripts\package-windows.ps1

Build and package Windows release artifacts.

Defaults:
  Preset: release
  Architecture: amd64 (x64-windows) or arm64
  Artifact dir: dist\windows\<arch>\
  Required build output: build\<preset>\SsaConsultaRapida QML module must exist.
  Optional parameters: -ProjectRoot, -Arch, -DistDir, -Version
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
$artifactRoot = Join-Path (if ($DistDir) { $DistDir } else { Join-Path $repoRoot "dist\windows" }) $arch
$artifactName = "ssa_consulta_rapida-$version-$arch-windows"
$artifactDir = Join-Path $artifactRoot $artifactName
$zipPath = Join-Path $artifactRoot ("{0}.zip" -f $artifactName)
$installerPath = Join-Path $artifactRoot ("{0}-installer.exe" -f $artifactName)
$nsiPath = Join-Path $artifactRoot "installer.nsi"

& $configureScript -Preset $preset | Out-Null

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

$qtBinDir = $null
if (Test-Path Env:\QT_DIR) {
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
CreateShortCut "`$DESKTOP\\${APP_NAME}.lnk" "`$INSTDIR\\ssa_consulta_rapida.exe"
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
