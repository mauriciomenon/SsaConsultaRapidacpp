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
  - final\<repo-name>.exe (single portable executable)
  - final\<repo-name>-installer.exe
  - final\<repo-name>.zip
  - versioned copies are created only from a clean matching Git tag
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

$buildScript = Join-Path $repoRoot "scripts\build-windows.ps1"
$version = Resolve-PackageVersion -RepoRoot $repoRoot -ExplicitVersion $Version
$arch = Resolve-WindowsArch -RequestedArch $Arch
$repoName = Split-Path $repoRoot -Leaf
$distRoot = if ($DistDir) { $DistDir } else { Join-Path $repoRoot "dist\windows" }
$artifactRoot = Join-Path $distRoot $arch
$artifactName = "$repoName-windows-$arch-$version"
$artifactDir = Join-Path $artifactRoot $artifactName
$zipPath = Join-Path $artifactRoot ("{0}.zip" -f $artifactName)
$installerPath = Join-Path $artifactRoot ("{0}-installer.exe" -f $artifactName)
$finalRoot = Join-Path $artifactRoot "final"
$portableStagePath = Join-Path $artifactRoot ".$repoName-portable-$PID.exe"
$portableNsiPath = Join-Path $artifactRoot ".$repoName-portable-$PID.nsi"
$installerNsiPath = Join-Path $artifactRoot ".$repoName-installer-$PID.nsi"
$makeNsisPath = Resolve-MakeNsisPath
if (-not $makeNsisPath) {
    throw "MakeNSIS not found. NSIS is required for the portable EXE and installer."
}

$buildParams = @{ Preset = $preset; ProjectRoot = $repoRoot }
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
& $buildScript @buildParams
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
Get-Item -LiteralPath $portableStagePath, $portableNsiPath, $installerNsiPath -Force -ErrorAction SilentlyContinue |
    Remove-Item -Force
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

$taggedRelease = Test-ExactReleaseTag -RepoRoot $repoRoot -Version $version
Publish-FinalArtifact -SourcePath $portableStagePath -FinalRoot $finalRoot `
    -LatestName "$repoName.exe" -VersionedName "$repoName-$version.exe" `
    -TaggedRelease $taggedRelease
Publish-FinalArtifact -SourcePath $installerPath -FinalRoot $finalRoot `
    -LatestName "$repoName-installer.exe" `
    -VersionedName "$repoName-installer-$version.exe" `
    -TaggedRelease $taggedRelease
Publish-FinalArtifact -SourcePath $zipPath -FinalRoot $finalRoot `
    -LatestName "$repoName.zip" -VersionedName "$repoName-$version.zip" `
    -TaggedRelease $taggedRelease

$finalPortablePath = Join-Path $finalRoot "$repoName.exe"
Set-LatestArtifactAlias -DistRoot $artifactRoot -AliasName "latest-binary" `
    -TargetPath $finalPortablePath
Set-LatestArtifactAlias -DistRoot $artifactRoot -AliasName "latest.exe" `
    -TargetPath $installerPath
Set-LatestArtifactAlias -DistRoot $artifactRoot -AliasName "latest-installer.exe" `
    -TargetPath $installerPath
Set-Content -Path (Join-Path $artifactRoot "latest-run.bat") -Value @"
@echo off
setlocal
"%~dp0final\$repoName.exe" %*
"@ -Encoding UTF8
Get-Item -LiteralPath $portableStagePath, $portableNsiPath, $installerNsiPath -Force -ErrorAction SilentlyContinue |
    Remove-Item -Force

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
Write-Output "  latest_exe: $artifactRoot/latest.exe"
Write-Output "  latest_installer: $artifactRoot/latest-installer.exe"
Write-Output "  installer: $installerPath"
Write-Output "  final_root: $finalRoot"
Write-Output "  tagged_release: $taggedRelease"
