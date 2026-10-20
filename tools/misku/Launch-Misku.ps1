[CmdletBinding()]
param(
    [switch]$Restart
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$package = Get-AppxPackage -Name MiskuTerminal | Select-Object -First 1
if (-not $package) {
    throw 'Misku Terminal is not registered. Deploy the package first or launch misku.exe from the portable folder.'
}

if ($Restart) {
    Get-Process misku -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -eq (Join-Path $package.InstallLocation 'misku.exe') } |
        Stop-Process -Force
    Start-Sleep -Milliseconds 500
}

$appUserModelId = "$($package.PackageFamilyName)!App"

Write-Host "Launching $appUserModelId..."
Start-Process explorer.exe "shell:AppsFolder\$appUserModelId"
