function Resolve-WindowsArch {
    param([string]$RequestedArch = "")

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

function Resolve-WindowsBuildLayout {
    param(
        [Parameter(Mandatory)]
        [string]$RepoRoot,
        [Parameter(Mandatory)]
        [string]$Preset,
        [string]$Arch = "",
        [string]$QtDir = "",
        [string]$QtSubdir = ""
    )

    $resolvedArch = Resolve-WindowsArch -RequestedArch $Arch
    $qtKit = if ($QtSubdir) {
        $QtSubdir
    } elseif ($QtDir) {
        $normalizedQtDir = $QtDir.Trim().Trim('"').Trim("'").TrimEnd('\', '/')
        if ($normalizedQtDir -match '^(.*)[\\/]lib[\\/]cmake[\\/]Qt6$') {
            $normalizedQtDir = $matches[1]
        }
        Split-Path $normalizedQtDir -Leaf
    } elseif ($resolvedArch -eq "arm64") {
        "msvc2022_arm64"
    } else {
        "mingw_64"
    }

    if ($Preset -notmatch '^[A-Za-z0-9_.-]+$') {
        throw "Invalid CMake preset name: $Preset"
    }
    if ($qtKit -notmatch '^[A-Za-z0-9_.-]+$') {
        throw "Invalid Qt kit name: $qtKit"
    }
    $qtKitIsArm64 = $qtKit -match '(?i)(^|_)arm64($|_)'
    if (($resolvedArch -eq "arm64" -and -not $qtKitIsArm64) -or
        ($resolvedArch -eq "amd64" -and $qtKitIsArm64)) {
        throw "Qt kit '$qtKit' is incompatible with Windows architecture '$resolvedArch'."
    }

    return [PSCustomObject]@{
        Arch = $resolvedArch
        QtKit = $qtKit
        BuildDir = Join-Path $RepoRoot "build\windows\$resolvedArch\$qtKit\$Preset"
    }
}
