$ErrorActionPreference = "Stop"

Describe "Windows package build failure" {
    It "aborts before cleaning a previous release when the build fails" {
        $testRoot = (Get-PSDrive -Name TestDrive).Root
        $repoRoot = Join-Path $testRoot "repo"
        $scriptsDir = Join-Path $repoRoot "scripts"
        $distDir = Join-Path $testRoot "dist"
        $artifactDir = Join-Path $distDir "amd64/repo-windows-amd64-1.2.3"
        $sentinel = Join-Path $artifactDir "previous-release.txt"
        $buildDir = Join-Path $repoRoot "build/release"
        $packageScript = Join-Path (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path "scripts/package-windows.ps1"

        New-Item -ItemType Directory -Path $scriptsDir, $artifactDir, $buildDir -Force | Out-Null
        Set-Content -LiteralPath $sentinel -Value "keep" -Encoding ASCII
        New-Item -ItemType File -Path (Join-Path $buildDir "ssa_consulta_rapida.exe"), (Join-Path $buildDir "sqlite3.dll") | Out-Null
        @'
& $env:ComSpec /c "exit 23"
'@ | Set-Content -LiteralPath (Join-Path $scriptsDir "build-windows.ps1") -Encoding ASCII
        @'
placeholder
'@ | Set-Content -LiteralPath (Join-Path $repoRoot "CMakeLists.txt") -Encoding ASCII

        $failure = $null
        try {
            & $packageScript -ProjectRoot $repoRoot -DistDir $distDir -Version "1.2.3" -SkipTests
        }
        catch {
            $failure = $_
        }

        (Test-Path -LiteralPath $sentinel -PathType Leaf) | Should -BeTrue
        $failure.Exception.Message | Should -Match "build.*exit code 23"
    }
}
