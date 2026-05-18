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
        $versionLine = Select-String -Path $cmakeLists -Pattern '^[ \t]*project\(\s*SSAConsultaRapidaCpp\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)' -AllMatches | Select-Object -First 1
        if ($versionLine.Matches.Count -gt 0) {
            return $versionLine.Matches[0].Groups[1].Value
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
