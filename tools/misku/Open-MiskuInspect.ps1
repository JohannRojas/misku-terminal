[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$tools = & (Join-Path $PSScriptRoot 'Get-MiskuToolchain.ps1') -Quiet
if (-not $tools.Inspect) {
    throw 'inspect.exe was not found in the Windows SDK.'
}

Write-Host "Opening Inspect: $($tools.Inspect)"
Start-Process $tools.Inspect
