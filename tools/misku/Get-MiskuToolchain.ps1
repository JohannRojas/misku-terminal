[CmdletBinding()]
param(
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Find-CommandPath {
    param([Parameter(Mandatory)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) {
        return $command.Source
    }

    return $null
}

function Find-VisualStudio {
    $vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $path = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath 2>$null | Select-Object -First 1
        if ($path) {
            return $path
        }
    }

    return $null
}

function Find-Inspect {
    $sdkRoot = 'C:\Program Files (x86)\Windows Kits\10\bin'
    if (-not (Test-Path $sdkRoot)) {
        return $null
    }

    $inspect = Get-ChildItem $sdkRoot -Recurse -Filter inspect.exe -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\inspect\.exe$' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1

    if ($inspect) {
        return $inspect.FullName
    }

    return $null
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$visualStudio = Find-VisualStudio

$msbuild = $null
$deployAppRecipe = $null
if ($visualStudio) {
    $msbuildCandidate = Join-Path $visualStudio 'MSBuild\Current\Bin\MSBuild.exe'
    if (Test-Path $msbuildCandidate) {
        $msbuild = $msbuildCandidate
    }

    $deployCandidate = Join-Path $visualStudio 'Common7\IDE\DeployAppRecipe.exe'
    if (Test-Path $deployCandidate) {
        $deployAppRecipe = $deployCandidate
    }
}

if (-not $msbuild) {
    $msbuild = Find-CommandPath MSBuild.exe
}

$packageProject = Join-Path $repoRoot 'src\cascadia\CascadiaPackage\CascadiaPackage.wapproj'
$debugRecipe = Join-Path $repoRoot 'src\cascadia\CascadiaPackage\bin\x64\Debug\CascadiaPackage.build.appxrecipe'

$toolchain = [pscustomobject]@{
    RepoRoot             = $repoRoot
    VisualStudio         = $visualStudio
    MSBuild              = $msbuild
    DeployAppRecipe      = $deployAppRecipe
    Inspect              = Find-Inspect
    WPR                  = Find-CommandPath wpr.exe
    Git                  = Find-CommandPath git.exe
    Ripgrep              = Find-CommandPath rg.exe
    PowerShell           = Find-CommandPath pwsh.exe
    VSCode               = Find-CommandPath code.cmd
    PackageProject       = $packageProject
    DebugAppxRecipe      = $debugRecipe
}

if ($Quiet) {
    $toolchain
} else {
    $toolchain | Format-List
}
