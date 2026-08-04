function Get-SsaWindowsRepoRoot {
    if ([string]::IsNullOrWhiteSpace($env:USERPROFILE)) {
        throw "[native-guard] BLOCKED: USERPROFILE is required for the Windows harness."
    }
    return Join-Path ([IO.Path]::GetFullPath($env:USERPROFILE)) 'gitlab\ssa_consulta_rapida_cpp'
}

function Assert-SsaWindowsHost {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$ExpectedRoot
    )

    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        throw "[native-guard] BLOCKED: PowerShell host is not native Windows."
    }
    if ($env:WSL_INTEROP -or $env:WSL_DISTRO_NAME) {
        throw "[native-guard] BLOCKED: Windows harness cannot be launched from WSL."
    }

    $resolvedRoot = (Resolve-Path -LiteralPath $RepoRoot -ErrorAction Stop).ProviderPath.TrimEnd('\')
    $expected = [IO.Path]::GetFullPath($ExpectedRoot).TrimEnd('\')
    if ($resolvedRoot.StartsWith('\\')) {
        throw "[native-guard] BLOCKED: UNC/WSL repository is forbidden: $resolvedRoot"
    }

    $allowed = $resolvedRoot.Equals($expected, [StringComparison]::OrdinalIgnoreCase)
    if (-not $allowed -and $env:SSA_NATIVE_GUARD_TEST_ROOT) {
        $testRoot = [IO.Path]::GetFullPath($env:SSA_NATIVE_GUARD_TEST_ROOT).TrimEnd('\')
        $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
        $isTempTest = $testRoot.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)
        $allowed = $isTempTest -and (
            $resolvedRoot.Equals($testRoot, [StringComparison]::OrdinalIgnoreCase) -or
            $resolvedRoot.StartsWith("$testRoot\", [StringComparison]::OrdinalIgnoreCase)
        )
    }
    if (-not $allowed) {
        throw "[native-guard] BLOCKED: Windows repo is '$resolvedRoot'; expected '$expected'."
    }

    if ($resolvedRoot.Equals($expected, [StringComparison]::OrdinalIgnoreCase)) {
        $repoItem = Get-Item -LiteralPath $resolvedRoot -Force
        if ($repoItem.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "[native-guard] BLOCKED: Windows repo cannot be a reparse point: $resolvedRoot"
        }
    }
}
