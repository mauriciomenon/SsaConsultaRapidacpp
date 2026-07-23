function Resolve-PackageVersion {
    param(
        [Parameter(Mandatory)]
        [string]$RepoRoot,
        [string]$ExplicitVersion = ""
    )

    if ($ExplicitVersion) {
        return $ExplicitVersion
    }

    $cmakeLists = Join-Path $RepoRoot "CMakeLists.txt"
    if (Test-Path $cmakeLists) {
        $cmakeContent = Get-Content -LiteralPath $cmakeLists -Raw
        $versionMatch = [regex]::Match(
            $cmakeContent,
            '(?m)^[ \t]*project\(\s*SSAConsultaRapidaCpp\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)'
        )
        if ($versionMatch.Success) {
            return $versionMatch.Groups[1].Value
        }
    }

    throw "Could not read project version from CMakeLists.txt. Use -Version."
}

function Set-LatestArtifactLink {
    param(
        [Parameter(Mandatory)]
        [string]$DistRoot,
        [Parameter(Mandatory)]
        [string]$ArtifactDir
    )

    $latestLink = Join-Path $DistRoot "latest"
    if (Test-Path $latestLink) {
        Remove-Item $latestLink -Recurse -Force
    }

    try {
        New-Item -ItemType SymbolicLink -Path $latestLink -Target $ArtifactDir -Force | Out-Null
        return
    }
    catch {
        Write-Verbose "Could not create symbolic link for latest artifact: $($_.Exception.Message)"
    }

    try {
        New-Item -ItemType Junction -Path $latestLink -Target $ArtifactDir -Force | Out-Null
        return
    }
    catch {
        Write-Verbose "Could not create junction for latest artifact: $($_.Exception.Message)"
    }

    New-Item -ItemType Directory -Path $latestLink -Force | Out-Null
    Set-Content -Path (Join-Path $latestLink "README.txt") -Value "Latest artifact link could not be created. Run the package script again with administrative permissions to refresh this shortcut."
}

function Set-LatestArtifactAlias {
    param(
        [Parameter(Mandatory)]
        [string]$DistRoot,
        [Parameter(Mandatory)]
        [string]$AliasName,
        [Parameter(Mandatory)]
        [string]$TargetPath
    )

    $aliasPath = Join-Path $DistRoot $AliasName
    if (Test-Path $aliasPath) {
        Remove-Item $aliasPath -Recurse -Force
    }

    try {
        New-Item -ItemType SymbolicLink -Path $aliasPath -Target $TargetPath | Out-Null
        return
    }
    catch {
        Write-Verbose "Could not create symbolic link for ${AliasName}: $($_.Exception.Message)"
    }

    if (Test-Path $TargetPath -PathType Container) {
        Copy-Item -Path $TargetPath -Destination $aliasPath -Recurse -Force
    }
    else {
        Copy-Item -Path $TargetPath -Destination $aliasPath -Force
    }
}

function Resolve-WindowsArch {
    param(
        [string]$RequestedArch = ""
    )

    $arch = if ($RequestedArch) {
        $RequestedArch.ToLowerInvariant()
    } else {
        $primary = [Environment]::GetEnvironmentVariable("PROCESSOR_ARCHITECTURE")
        $wow = [Environment]::GetEnvironmentVariable("PROCESSOR_ARCHITEW6432")
        if ($primary -eq "ARM64" -or $wow -eq "ARM64") {
            "arm64"
        } elseif ($primary -eq "AMD64" -or $primary -eq "X64" -or $wow -eq "AMD64" -or $wow -eq "X64") {
            "amd64"
        } else {
            $primary
        }
    }

    if ($arch -eq "x64" -or $arch -eq "x86_64") {
        $arch = "amd64"
    }

    if ($arch -ne "amd64" -and $arch -ne "arm64") {
        throw "Unsupported architecture: $arch. Use amd64 or arm64."
    }

    return $arch
}

function Test-ExactReleaseTag {
    param(
        [Parameter(Mandatory)]
        [string]$RepoRoot,
        [Parameter(Mandatory)]
        [string]$Version
    )

    & git -C $RepoRoot diff --quiet --
    if ($LASTEXITCODE -ne 0) {
        return $false
    }
    & git -C $RepoRoot diff --cached --quiet --
    if ($LASTEXITCODE -ne 0) {
        return $false
    }
    $untracked = @(& git -C $RepoRoot ls-files --others --exclude-standard)
    if ($LASTEXITCODE -ne 0 -or $untracked.Count -ne 0) {
        return $false
    }
    $tags = @(& git -C $RepoRoot tag --points-at HEAD --list "v$Version")
    return $LASTEXITCODE -eq 0 -and $tags -contains "v$Version"
}

function Publish-FinalArtifact {
    param(
        [Parameter(Mandatory)]
        [string]$SourcePath,
        [Parameter(Mandatory)]
        [string]$FinalRoot,
        [Parameter(Mandatory)]
        [string]$LatestName,
        [Parameter(Mandatory)]
        [string]$VersionedName,
        [Parameter(Mandatory)]
        [bool]$TaggedRelease
    )

    New-Item -ItemType Directory -Path $FinalRoot -Force | Out-Null
    $latestPath = Join-Path $FinalRoot $LatestName
    $versionedPath = Join-Path $FinalRoot $VersionedName
    $stagedPath = Join-Path $FinalRoot ".$LatestName.$PID.staging"
    Get-Item -LiteralPath $stagedPath -Force -ErrorAction SilentlyContinue |
        Remove-Item -Recurse -Force

    if (Test-Path -LiteralPath $SourcePath -PathType Container) {
        Copy-Item -LiteralPath $SourcePath -Destination $stagedPath -Recurse
        Get-Item -LiteralPath $latestPath -Force -ErrorAction SilentlyContinue |
            Remove-Item -Recurse -Force
    }
    else {
        Copy-Item -LiteralPath $SourcePath -Destination $stagedPath
    }
    Move-Item -LiteralPath $stagedPath -Destination $latestPath -Force

    if ($TaggedRelease) {
        if (Test-Path -LiteralPath $versionedPath) {
            Write-Output "Preserving existing final artifact: $versionedPath"
        }
        elseif (Test-Path -LiteralPath $SourcePath -PathType Container) {
            Copy-Item -LiteralPath $SourcePath -Destination $versionedPath -Recurse
        }
        else {
            Copy-Item -LiteralPath $SourcePath -Destination $versionedPath
        }
    }
}
