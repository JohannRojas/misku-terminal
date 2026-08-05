[CmdletBinding()]
param(
    [switch]$Restart
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$tools = & (Join-Path $PSScriptRoot 'Get-MiskuToolchain.ps1') -Quiet

if ($Restart) {
    Get-Process misku -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 500
}

$app = $null
if (Get-Command Get-StartApps -ErrorAction SilentlyContinue) {
    $app = Get-StartApps | Where-Object { $_.Name -like '*Misku*' } | Select-Object -First 1
}
$appUserModelId = if ($app) { $app.AppID } else { $tools.LaunchAppUserModelId }

Write-Host "Launching $appUserModelId..."
Start-Process explorer.exe "shell:AppsFolder\$appUserModelId"
