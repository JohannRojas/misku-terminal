[CmdletBinding()]
param(
    [ValidateSet('x64', 'arm64', 'Win32')]
    [string]$Platform = 'x64',

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [switch]$Launch
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$tools = & (Join-Path $PSScriptRoot 'Get-MiskuToolchain.ps1') -Quiet
if (-not $tools.DeployAppRecipe) {
    throw 'DeployAppRecipe.exe was not found. Install Visual Studio 2022 with Windows app packaging tools.'
}

$recipe = Join-Path $tools.RepoRoot "src\cascadia\CascadiaPackage\bin\$Platform\$Configuration\CascadiaPackage.build.appxrecipe"
if (-not (Test-Path $recipe)) {
    throw "App package recipe not found: $recipe. Run tools\misku\Build-MiskuDebug.ps1 first."
}

Write-Host "Deploying Misku Terminal from $recipe..."
& $tools.DeployAppRecipe $recipe
if ($LASTEXITCODE -ne 0) {
    throw "DeployAppRecipe failed with exit code $LASTEXITCODE."
}

Write-Host 'Deploy completed.'

if ($Launch) {
    & (Join-Path $PSScriptRoot 'Launch-Misku.ps1')
}
