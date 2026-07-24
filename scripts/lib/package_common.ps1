. (Join-Path $PSScriptRoot "windows_build_layout.ps1")

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

function Publish-WindowsReleaseSet {
    param(
        [Parameter(Mandatory)]
        [string]$StageDir,
        [Parameter(Mandatory)]
        [string]$DistRoot,
        [Parameter(Mandatory)]
        [string]$Version,
        [Parameter(Mandatory)]
        [string]$CommitSha
    )

    if (-not (Test-Path -LiteralPath $StageDir -PathType Container)) {
        throw "Windows release set is incomplete: stage directory not found."
    }

    $portableFiles = @(Get-ChildItem -LiteralPath $StageDir -File -Filter "*.exe" |
        Where-Object { $_.Name -notlike "*-installer.exe" })
    $installerFiles = @(Get-ChildItem -LiteralPath $StageDir -File -Filter "*-installer.exe")
    $zipFiles = @(Get-ChildItem -LiteralPath $StageDir -File -Filter "*.zip")
    $standaloneDirs = @(Get-ChildItem -LiteralPath $StageDir -Directory -Filter "*-standalone")
    $standaloneExecutables = if ($standaloneDirs.Count -eq 1 -and $portableFiles.Count -eq 1) {
        @(Get-ChildItem -LiteralPath $standaloneDirs[0].FullName -File |
            Where-Object { $_.Name -eq $portableFiles[0].Name })
    } else {
        @()
    }
    if ($portableFiles.Count -ne 1 -or $installerFiles.Count -ne 1 -or
        $zipFiles.Count -ne 1 -or $standaloneDirs.Count -ne 1 -or
        $standaloneExecutables.Count -ne 1) {
        throw "Windows release set is incomplete: portable, installer, zip and standalone are required."
    }

    $releaseId = "$Version-$CommitSha"
    $releaseRecord = [ordered]@{
        version = $Version
        commit = $CommitSha
        release = $releaseId
    }
    $releaseRecord | ConvertTo-Json |
        Set-Content -LiteralPath (Join-Path $StageDir "release.json") -Encoding UTF8

    $hashLines = @(Get-ChildItem -LiteralPath $StageDir -File -Recurse |
        Where-Object { $_.Name -ne "SHA256SUMS" } |
        Sort-Object FullName |
        ForEach-Object {
            $relativePath = $_.FullName.Substring($StageDir.Length).TrimStart('\', '/').Replace('\', '/')
            $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
            "$hash  $relativePath"
        })
    $hashLines | Set-Content -LiteralPath (Join-Path $StageDir "SHA256SUMS") -Encoding ASCII

    $releasesRoot = Join-Path $DistRoot "releases"
    $releaseDir = Join-Path $releasesRoot $releaseId
    $finalDir = Join-Path $DistRoot "final"
    $nextFinalDir = Join-Path $DistRoot ".final-$PID-staging"
    $previousFinalDir = Join-Path $DistRoot ".final-$PID-previous"
    $currentPath = Join-Path $DistRoot "current.json"
    $nextCurrentPath = Join-Path $DistRoot ".current-$PID.json"
    $lockPath = Join-Path $DistRoot ".publish.lock"
    $lockStream = $null
    $previousFinalMoved = $false
    try {
        try {
            $lockStream = [System.IO.File]::Open(
                $lockPath,
                [System.IO.FileMode]::OpenOrCreate,
                [System.IO.FileAccess]::ReadWrite,
                [System.IO.FileShare]::None
            )
        }
        catch [System.IO.IOException] {
            throw "Another Windows release publication is already running for: $DistRoot"
        }

        New-Item -ItemType Directory -Path $releasesRoot -Force | Out-Null
        if (Test-Path -LiteralPath $releaseDir -PathType Container) {
            $existingHashes = Get-Content -LiteralPath (Join-Path $releaseDir "SHA256SUMS") -Raw
            $stagedHashes = Get-Content -LiteralPath (Join-Path $StageDir "SHA256SUMS") -Raw
            if ($existingHashes -ne $stagedHashes) {
                throw "Immutable Windows release already exists with different hashes: $releaseId"
            }
            Remove-Item -LiteralPath $StageDir -Recurse -Force
        } else {
            Move-Item -LiteralPath $StageDir -Destination $releaseDir
        }

        Get-Item -LiteralPath $nextFinalDir, $previousFinalDir -Force -ErrorAction SilentlyContinue |
            Remove-Item -Recurse -Force
        Get-Item -LiteralPath $nextCurrentPath -Force -ErrorAction SilentlyContinue |
            Remove-Item -Force
        Copy-Item -LiteralPath $releaseDir -Destination $nextFinalDir -Recurse
        $releaseRecord | ConvertTo-Json |
            Set-Content -LiteralPath $nextCurrentPath -Encoding UTF8

        try {
            if (Test-Path -LiteralPath $finalDir -PathType Container) {
                Move-Item -LiteralPath $finalDir -Destination $previousFinalDir
                $previousFinalMoved = $true
            }
            Move-Item -LiteralPath $nextFinalDir -Destination $finalDir
            Move-Item -LiteralPath $nextCurrentPath -Destination $currentPath -Force
        }
        catch {
            Get-Item -LiteralPath $finalDir -Force -ErrorAction SilentlyContinue |
                Remove-Item -Recurse -Force
            if ($previousFinalMoved -and (Test-Path -LiteralPath $previousFinalDir -PathType Container)) {
                Move-Item -LiteralPath $previousFinalDir -Destination $finalDir
                $previousFinalMoved = $false
            }
            throw
        }
    }
    finally {
        if ($lockStream) {
            $lockStream.Dispose()
        }
        try {
            Get-Item -LiteralPath $nextFinalDir, $previousFinalDir -Force -ErrorAction SilentlyContinue |
                Remove-Item -Recurse -Force
            Get-Item -LiteralPath $nextCurrentPath -Force -ErrorAction SilentlyContinue |
                Remove-Item -Force
        }
        catch {
            Write-Warning "Windows release publication left temporary promotion files: $($_.Exception.Message)"
        }
    }
}
