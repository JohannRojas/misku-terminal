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

function Get-DescendantProcesses {
    param([Parameter(Mandatory)][int]$RootProcessId)

    $allProcesses = @(Get-CimInstance Win32_Process -Property ProcessId, ParentProcessId, Name)
    $descendants = [System.Collections.Generic.List[object]]::new()
    $pending = [System.Collections.Generic.Queue[int]]::new()
    $pending.Enqueue($RootProcessId)

    while ($pending.Count -gt 0) {
        $parentProcessId = $pending.Dequeue()
        foreach ($child in $allProcesses | Where-Object { [int]$_.ParentProcessId -eq $parentProcessId }) {
            $descendants.Add($child)
            $pending.Enqueue([int]$child.ProcessId)
        }
    }

    return $descendants.ToArray()
}

function Wait-MiskuInitialTerminal {
    param(
        [Parameter(Mandatory)][int]$MiskuProcessId,
        [Parameter(Mandatory)][int]$TimeoutSeconds
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $lastDescendants = @()

    while ((Get-Date) -lt $deadline) {
        $lastDescendants = @(Get-DescendantProcesses -RootProcessId $MiskuProcessId)
        $openConsole = @($lastDescendants | Where-Object { $_.Name -ieq 'OpenConsole.exe' })
        $shells = @($lastDescendants | Where-Object { $_.Name -iin @('pwsh.exe', 'powershell.exe') })

        if ($openConsole.Count -eq 1 -and $shells.Count -ge 1) {
            return [pscustomobject]@{
                OpenConsole = $openConsole[0]
                Shell = $shells[0]
            }
        }

        Start-Sleep -Milliseconds 250
    }

    $processSummary = ($lastDescendants | ForEach-Object { "$($_.Name)[$($_.ProcessId)]" }) -join ', '
    if (-not $processSummary) {
        $processSummary = '<none>'
    }

    throw "The initial terminal session was not ready after $TimeoutSeconds seconds. Descendants: $processSummary"
}

$window = Get-MiskuWindowElement -TimeoutSeconds $TimeoutSeconds
$miskuProcessId = [int]$window.Current.ProcessId
Write-Host "Found Misku window: $($window.Current.Name) (PID $miskuProcessId)"

# This check must run before any UI interaction. Otherwise opening a tab from
# the split button can mask a broken cold-start path that left the window empty.
$initialTerminal = Wait-MiskuInitialTerminal -MiskuProcessId $miskuProcessId -TimeoutSeconds $TimeoutSeconds
Write-Host "Initial terminal ready: $($initialTerminal.OpenConsole.Name)[$($initialTerminal.OpenConsole.ProcessId)] -> $($initialTerminal.Shell.Name)[$($initialTerminal.Shell.ProcessId)]"

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
