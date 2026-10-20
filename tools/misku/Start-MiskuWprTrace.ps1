[CmdletBinding()]
param(
    [int]$Seconds = 12,
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$tools = & (Join-Path $PSScriptRoot 'Get-MiskuToolchain.ps1') -Quiet
if (-not $tools.WPR) {
    throw 'wpr.exe was not found.'
}

if (-not $OutputPath) {
    $artifactDir = Join-Path $tools.RepoRoot 'artifacts\misku'
    New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $OutputPath = Join-Path $artifactDir "misku-startup-$stamp.etl"
}

Write-Host 'Starting WPR GeneralProfile trace...'
& $tools.WPR -start GeneralProfile -filemode
if ($LASTEXITCODE -ne 0) {
    throw "wpr -start failed with exit code $LASTEXITCODE. Try running from an elevated terminal."
}

try {
    & (Join-Path $PSScriptRoot 'Launch-Misku.ps1') -Restart
    Start-Sleep -Seconds $Seconds
} finally {
    Write-Host "Stopping trace: $OutputPath"
    & $tools.WPR -stop $OutputPath
}

Write-Host "Trace saved: $OutputPath"
