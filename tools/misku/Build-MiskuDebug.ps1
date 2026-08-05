[CmdletBinding()]
param(
    [ValidateSet('x64', 'arm64', 'Win32')]
    [string]$Platform = 'x64',

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('Dev', 'Release')]
    [string]$Branding = 'Dev',

    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Quote-ProcessArgument {
    param([Parameter(Mandatory)][string]$Argument)

    if ($Argument -notmatch '[\s"]') {
        return $Argument
    }

    $escaped = $Argument -replace '(\\*)"', '$1$1\"'
    $escaped = $escaped -replace '(\\+)$', '$1$1'
    return '"' + $escaped + '"'
}


$tools = & (Join-Path $PSScriptRoot 'Get-MiskuToolchain.ps1') -Quiet
if (-not $tools.MSBuild) {
    throw 'MSBuild.exe was not found. Install Visual Studio with the C++ desktop and Windows application packaging workloads.'
}

if (-not (Test-Path $tools.PackageProject)) {
    throw "Package project not found: $($tools.PackageProject)"
}

$solutionDir = $tools.RepoRoot.TrimEnd('\') + '\'
$logName = "msbuild-misku-$Platform-$Configuration"

$arguments = @(
    $tools.PackageProject,
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    "/p:WindowsTerminalBranding=$Branding",
    "/p:SolutionDir=$solutionDir",
    '/p:MiskuLowMemoryBuild=true',
    '/p:AppxSymbolPackageEnabled=false',
    '/m:1',
    '/v:minimal',
    "/flp:logfile=$logName.log;verbosity=normal",
    "/bl:$logName.binlog"
)


if ($Clean) {
    $arguments = @('/t:Clean;Build') + $arguments
}

Write-Host "Building Misku Terminal ($Branding branding) $Configuration $Platform..."
Write-Host $tools.MSBuild

$processInfo = [System.Diagnostics.ProcessStartInfo]::new()
$processInfo.FileName = $tools.MSBuild
$processInfo.WorkingDirectory = $tools.RepoRoot
$processInfo.UseShellExecute = $false
$processInfo.RedirectStandardOutput = $true
$processInfo.RedirectStandardError = $true

$processInfo.Environment.Clear()
Get-ChildItem Env: | Where-Object { $_.Name -ne 'path' } | ForEach-Object {
    $processInfo.Environment[$_.Name] = $_.Value
}
$processInfo.Environment['PATH'] = $env:PATH

$processInfo.Arguments = ($arguments | ForEach-Object { Quote-ProcessArgument $_ }) -join ' '

$process = [System.Diagnostics.Process]::Start($processInfo)
while (-not $process.StandardOutput.EndOfStream) {
    Write-Host $process.StandardOutput.ReadLine()
}

$stderr = $process.StandardError.ReadToEnd()
$process.WaitForExit()

if ($stderr) {
    Write-Host $stderr
}

if ($process.ExitCode -ne 0) {
    throw "MSBuild failed with exit code $($process.ExitCode). See $logName.log and $logName.binlog."
}

Write-Host "Build completed. Logs: $logName.log, $logName.binlog"
