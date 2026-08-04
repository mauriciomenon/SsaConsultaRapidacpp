$ErrorActionPreference = 'Stop'

Describe 'Windows native host guard' {
    BeforeAll {
        $script:repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
        . (Join-Path $script:repoRoot 'scripts\lib\native_host_guard.ps1')
    }

    It 'accepts the canonical Windows repository' -Skip:([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        Assert-SsaWindowsHost -RepoRoot $script:repoRoot `
            -ExpectedRoot (Get-SsaWindowsRepoRoot)
    }

    It 'rejects a Windows harness launched from WSL' -Skip:([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        $previous = $env:WSL_INTEROP
        try {
            $env:WSL_INTEROP = 'blocked-test'
            {
                Assert-SsaWindowsHost -RepoRoot $script:repoRoot `
                    -ExpectedRoot (Get-SsaWindowsRepoRoot)
            } | Should -Throw '*cannot be launched from WSL*'
        }
        finally {
            $env:WSL_INTEROP = $previous
        }
    }
}
