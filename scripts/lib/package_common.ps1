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
        [string]$CommitSha,
        [Parameter(Mandatory)]
        [string]$RepoRoot,
        [Parameter(Mandatory)]
        [string]$Platform,
        [Parameter(Mandatory)]
        [string]$Architecture,
        [Parameter(Mandatory)]
        [ValidateSet("msvc", "llvm", "mingw")]
        [string]$Toolchain,
        [Parameter(Mandatory)]
        [bool]$TaggedRelease,
        [Parameter(Mandatory)]
        [string]$Preset,
        [Parameter(Mandatory)]
        [string]$QtKit,
        [Parameter(Mandatory)]
        [string]$Compiler,
        [string]$CompilerVersion = "",
        [Parameter(Mandatory)]
        [string]$Linker
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

    foreach ($identityPart in @($Version, $CommitSha, $Platform, $Architecture, $Toolchain)) {
        if ($identityPart -notmatch '^[A-Za-z0-9._-]+$') {
            throw "Windows release identity contains an unsupported value: $identityPart"
        }
    }
    if ($Platform -ne "windows" -or $Architecture -notin @("amd64", "arm64")) {
        throw "Windows release identity is invalid: platform=$Platform architecture=$Architecture"
    }

    $releaseId = "$Version-$CommitSha-$Platform-$Architecture-$Toolchain"
    $releaseRecord = [ordered]@{
        version = $Version
        commit = $CommitSha
        release = $releaseId
        platform = $Platform
        architecture = $Architecture
        toolchain = $Toolchain
        preset = $Preset
        qtKit = $QtKit
        compiler = $Compiler
        compilerVersion = $CompilerVersion
        linker = $Linker
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
    $previousDir = Join-Path $DistRoot "previous"
    $nextFinalDir = Join-Path $DistRoot ".final-$PID-staging"
    $nextPreviousDir = Join-Path $DistRoot ".previous-$PID-staging"
    $priorFinalDir = Join-Path $DistRoot ".final-$PID-prior"
    $priorPreviousDir = Join-Path $DistRoot ".previous-$PID-prior"
    $currentPath = Join-Path $DistRoot "current.json"
    $previousPath = Join-Path $DistRoot "previous.json"
    $nextCurrentPath = Join-Path $DistRoot ".current-$PID.json"
    $nextPreviousPath = Join-Path $DistRoot ".previous-$PID.json"
    $priorCurrentPath = Join-Path $DistRoot ".current-$PID-prior.json"
    $priorPreviousPath = Join-Path $DistRoot ".previous-$PID-prior.json"
    $lockPath = Join-Path $DistRoot ".publish.lock"
    $lockStream = $null
    $finalMoved = $false
    $currentMoved = $false
    $previousMoved = $false
    $previousRecordMoved = $false
    $previousPromoted = $false
    $previousRecordPromoted = $false
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
        $publicationSource = $StageDir
        if ($TaggedRelease -and (Test-Path -LiteralPath $releaseDir -PathType Container)) {
            $existingHashes = Get-Content -LiteralPath (Join-Path $releaseDir "SHA256SUMS") -Raw
            $stagedHashes = Get-Content -LiteralPath (Join-Path $StageDir "SHA256SUMS") -Raw
            if ($existingHashes -ne $stagedHashes) {
                throw "Immutable Windows release already exists with different hashes: $releaseId"
            }
            $publicationSource = $releaseDir
        } elseif ($TaggedRelease) {
            Move-Item -LiteralPath $StageDir -Destination $releaseDir
            $publicationSource = $releaseDir
        }

        Get-Item -LiteralPath $nextFinalDir, $nextPreviousDir, $priorFinalDir, $priorPreviousDir -Force -ErrorAction SilentlyContinue |
            Remove-Item -Recurse -Force
        Get-Item -LiteralPath $nextCurrentPath, $nextPreviousPath, $priorCurrentPath, $priorPreviousPath -Force -ErrorAction SilentlyContinue |
            Remove-Item -Force
        Copy-Item -LiteralPath $publicationSource -Destination $nextFinalDir -Recurse
        $releaseRecord | ConvertTo-Json |
            Set-Content -LiteralPath $nextCurrentPath -Encoding UTF8
        if (Test-Path -LiteralPath $finalDir -PathType Container) {
            Copy-Item -LiteralPath $finalDir -Destination $nextPreviousDir -Recurse
            if (Test-Path -LiteralPath $currentPath -PathType Leaf) {
                Copy-Item -LiteralPath $currentPath -Destination $nextPreviousPath
            } elseif (Test-Path -LiteralPath (Join-Path $finalDir "release.json") -PathType Leaf) {
                Copy-Item -LiteralPath (Join-Path $finalDir "release.json") -Destination $nextPreviousPath
            } else {
                Remove-Item -LiteralPath $nextPreviousDir -Recurse -Force
            }
        }

        try {
            if (Test-Path -LiteralPath $finalDir -PathType Container) {
                Move-Item -LiteralPath $finalDir -Destination $priorFinalDir
                $finalMoved = $true
                if (Test-Path -LiteralPath $currentPath -PathType Leaf) {
                    Move-Item -LiteralPath $currentPath -Destination $priorCurrentPath
                    $currentMoved = $true
                }
            }
            Move-Item -LiteralPath $nextFinalDir -Destination $finalDir
            Move-Item -LiteralPath $nextCurrentPath -Destination $currentPath -Force
            if ($finalMoved -and (Test-Path -LiteralPath $nextPreviousDir -PathType Container) -and
                (Test-Path -LiteralPath $nextPreviousPath -PathType Leaf)) {
                if (Test-Path -LiteralPath $previousDir -PathType Container) {
                    Move-Item -LiteralPath $previousDir -Destination $priorPreviousDir
                    $previousMoved = $true
                }
                if (Test-Path -LiteralPath $previousPath -PathType Leaf) {
                    Move-Item -LiteralPath $previousPath -Destination $priorPreviousPath
                    $previousRecordMoved = $true
                }
                Move-Item -LiteralPath $nextPreviousDir -Destination $previousDir
                $previousPromoted = $true
                Move-Item -LiteralPath $nextPreviousPath -Destination $previousPath
                $previousRecordPromoted = $true
            }
        }
        catch {
            Get-Item -LiteralPath $finalDir -Force -ErrorAction SilentlyContinue |
                Remove-Item -Recurse -Force
            Get-Item -LiteralPath $currentPath -Force -ErrorAction SilentlyContinue |
                Remove-Item -Force
            if ($finalMoved -and (Test-Path -LiteralPath $priorFinalDir -PathType Container)) {
                Move-Item -LiteralPath $priorFinalDir -Destination $finalDir
                $finalMoved = $false
            }
            if ($currentMoved -and (Test-Path -LiteralPath $priorCurrentPath -PathType Leaf)) {
                Move-Item -LiteralPath $priorCurrentPath -Destination $currentPath
                $currentMoved = $false
            }
            if ($previousPromoted) {
                Get-Item -LiteralPath $previousDir -Force -ErrorAction SilentlyContinue |
                    Remove-Item -Recurse -Force
            }
            if ($previousRecordPromoted) {
                Get-Item -LiteralPath $previousPath -Force -ErrorAction SilentlyContinue |
                    Remove-Item -Force
            }
            if ($previousMoved -and (Test-Path -LiteralPath $priorPreviousDir -PathType Container)) {
                Move-Item -LiteralPath $priorPreviousDir -Destination $previousDir
                $previousMoved = $false
            }
            if ($previousRecordMoved -and (Test-Path -LiteralPath $priorPreviousPath -PathType Leaf)) {
                Move-Item -LiteralPath $priorPreviousPath -Destination $previousPath
                $previousRecordMoved = $false
            }
            throw
        }

        if (-not $TaggedRelease) {
            Remove-Item -LiteralPath $StageDir -Recurse -Force
        }
        foreach ($candidate in @(Get-ChildItem -LiteralPath $releasesRoot -Directory -ErrorAction SilentlyContinue)) {
            $manifestPath = Join-Path $candidate.FullName "release.json"
            if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
                continue
            }
            try {
                $record = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
                $validIdentity = $record.release -eq $candidate.Name -and
                    $record.platform -eq $Platform -and $record.architecture -eq $Architecture -and
                    $record.toolchain -eq $Toolchain -and $record.release -eq "$($record.version)-$($record.commit)-$Platform-$Architecture-$Toolchain"
                if (-not $validIdentity) {
                    Write-Warning "Preserving release with invalid identity: $($candidate.FullName)"
                    continue
                }
                $releaseTags = @(& git -C $RepoRoot tag --points-at $record.commit --list "v$($record.version)")
                if ($LASTEXITCODE -ne 0) {
                    Write-Warning "Preserving release because tag verification failed: $($candidate.FullName)"
                    continue
                }
                if ($releaseTags -notcontains "v$($record.version)") {
                    Remove-Item -LiteralPath $candidate.FullName -Recurse -Force
                }
            }
            catch {
                Write-Warning "Preserving release with unreadable manifest: $($candidate.FullName)"
                continue
            }
        }
    }
    finally {
        if ($lockStream) {
            $lockStream.Dispose()
        }
        try {
            Get-Item -LiteralPath $nextFinalDir, $nextPreviousDir, $priorFinalDir, $priorPreviousDir -Force -ErrorAction SilentlyContinue |
                Remove-Item -Recurse -Force
            Get-Item -LiteralPath $nextCurrentPath, $nextPreviousPath, $priorCurrentPath, $priorPreviousPath -Force -ErrorAction SilentlyContinue |
                Remove-Item -Force
        }
        catch {
            Write-Warning "Windows release publication left temporary promotion files: $($_.Exception.Message)"
        }
    }
}
