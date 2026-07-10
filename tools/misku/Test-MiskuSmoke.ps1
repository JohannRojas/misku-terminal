[CmdletBinding()]
param(
    [switch]$Launch,
    [int]$TimeoutSeconds = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($Launch) {
    & (Join-Path $PSScriptRoot 'Launch-Misku.ps1')
}

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

function Get-MiskuWindowElement {
    param([int]$TimeoutSeconds)

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $process = Get-Process misku -ErrorAction SilentlyContinue |
            Where-Object { $_.MainWindowHandle -ne 0 } |
            Select-Object -First 1

        if ($process) {
            $element = [System.Windows.Automation.AutomationElement]::FromHandle($process.MainWindowHandle)
            if ($element) {
                return $element
            }
        }

        Start-Sleep -Milliseconds 500
    }

    throw "Misku window was not found after $TimeoutSeconds seconds."
}

function Find-FirstByAutomationId {
    param(
        [Parameter(Mandatory)]$Root,
        [Parameter(Mandatory)][string[]]$AutomationIds
    )

    foreach ($automationId in $AutomationIds) {
        $condition = [System.Windows.Automation.PropertyCondition]::new(
            [System.Windows.Automation.AutomationElement]::AutomationIdProperty,
            $automationId)

        $element = $Root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $condition)
        if ($element) {
            return $element
        }
    }

    return $null
}

$window = Get-MiskuWindowElement -TimeoutSeconds $TimeoutSeconds
Write-Host "Found Misku window: $($window.Current.Name)"

$newTabButton = Find-FirstByAutomationId -Root $window -AutomationIds @('VerticalTabsNewTabButton', 'NewTabButton')
if (-not $newTabButton) {
    throw 'New tab button was not found through UI Automation.'
}

$invoke = $newTabButton.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
$invoke.Invoke()
Start-Sleep -Milliseconds 800

$menuCondition = [System.Windows.Automation.PropertyCondition]::new(
    [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
    [System.Windows.Automation.ControlType]::MenuItem)

$items = [System.Windows.Automation.AutomationElement]::RootElement.FindAll(
    [System.Windows.Automation.TreeScope]::Descendants,
    $menuCondition)

$names = for ($i = 0; $i -lt $items.Count; $i++) {
    $name = $items.Item($i).Current.Name
    if ($name) {
        $name
    }
}

$powershellProfiles = $names | Where-Object { $_ -match 'PowerShell' }
if (-not $powershellProfiles) {
    throw 'PowerShell was not found in the new-tab menu.'
}

Write-Host 'Smoke test passed. PowerShell profiles found:'
$powershellProfiles | Sort-Object -Unique | ForEach-Object { Write-Host " - $_" }
