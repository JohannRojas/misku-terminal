[CmdletBinding()]
param(
    [ValidateSet('x64')][string]$Platform = 'x64',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [ValidateSet('Dev', 'Release')][string]$Branding = 'Release',
    [switch]$UI,
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$toolchain = & (Join-Path $PSScriptRoot 'Get-MiskuToolchain.ps1') -Quiet
$repoRoot = $toolchain.RepoRoot
$outputRoot = Join-Path $repoRoot "bin\$Platform\$Configuration"
$resultsRoot = Join-Path $repoRoot 'artifacts\tests'
New-Item -ItemType Directory -Path $resultsRoot -Force | Out-Null

if (-not $SkipBuild) {
    $projects = @(
        'src\cascadia\UnitTests_SettingsModel\SettingsModel.UnitTests.vcxproj',
        'src\cascadia\LocalTests_TerminalApp\TerminalApp.LocalTests.vcxproj'
    )
    if ($UI) {
        $projects += 'src\cascadia\LocalTests_TerminalApp\TestHostApp\TestHostApp.vcxproj'
    }
    foreach ($project in $projects) {
        & $toolchain.MSBuild (Join-Path $repoRoot $project) /t:Build "/p:SolutionDir=$repoRoot\" `
            "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:WindowsTerminalBranding=$Branding" `
            /p:BuildProjectReferences=false /p:MiskuLowMemoryBuild=true /m:1 /v:minimal
        if ($LASTEXITCODE -ne 0) { throw "Test build failed: $project ($LASTEXITCODE). Build the app first." }
    }
}

[xml]$versions = Get-Content -LiteralPath (Join-Path $repoRoot 'src\common.nugetversions.props') -Raw
$taefRelative = @($versions.Project.PropertyGroup.TAEFPackagePathRoot | Where-Object { $_ })[0]
$taefRoot = $taefRelative.Replace('$(MSBuildThisFileDirectory)', (Join-Path $repoRoot 'src\'))
$runner = Join-Path $taefRoot "build\Binaries\$Platform\TE.exe"
if (-not (Test-Path -LiteralPath $runner)) { throw "TAEF is missing: $runner" }

function Invoke-MiskuTest {
    param([string]$Name, [string]$Directory, [string]$Library, [string[]]$Options = @())
    Push-Location $Directory
    try {
        # In-process tests resolve PRI resources relative to the runner executable.
        $localRunner = Join-Path $Directory 'TE.exe'
        $testRunner = if (Test-Path -LiteralPath $localRunner) { $localRunner } else { $runner }
        & $testRunner $Library @Options /logOutput:Low 2>&1 |
            Tee-Object -FilePath (Join-Path $resultsRoot "$Name.log")
        if ($LASTEXITCODE -ne 0) { throw "$Name failed with exit code $LASTEXITCODE." }
        $summary = Get-Content -LiteralPath (Join-Path $resultsRoot "$Name.log") -Raw
        if ($summary -notmatch 'Summary: Total=(\d+), Passed=(\d+), Failed=0, Blocked=0, Not Run=0, Skipped=0' -or
            [int]$Matches[1] -eq 0 -or $Matches[1] -ne $Matches[2]) {
            throw "$Name did not execute a complete, passing test selection."
        }
    } finally {
        Pop-Location
    }
}

Invoke-MiskuTest -Name settings -Directory (Join-Path $outputRoot 'UnitTests_SettingsModel') -Library 'SettingsModel.Unit.Tests.dll' -Options '/inproc'
Invoke-MiskuTest -Name commandline -Directory (Join-Path $outputRoot 'LocalTests_TerminalApp') `
    -Library 'TerminalApp.LocalTests.dll' -Options @('/name:*CommandlineTest*', '/inproc')
if ($UI) {
    Invoke-MiskuTest -Name sessions -Directory (Join-Path $outputRoot 'TestHostApp') `
        -Library 'TerminalApp.LocalTests.dll' -Options '/name:*TabTests::*Misku*'
}
Write-Host "Misku tests passed. Results: $resultsRoot"
