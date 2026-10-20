[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackagePath,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [ValidateSet('stable', 'preview')][string]$Channel = 'stable',
    [string]$XamlDependencyPath,
    [string]$CertificatePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$package = Get-Item -LiteralPath $PackagePath
$archive = [IO.Compression.ZipFile]::OpenRead($package.FullName)
try {
    $entry = $archive.GetEntry('AppxManifest.xml')
    if (-not $entry) { throw 'The MSIX does not contain an app manifest.' }
    $reader = [IO.StreamReader]::new($entry.Open())
    try { [xml]$manifest = $reader.ReadToEnd() } finally { $reader.Dispose() }
} finally { $archive.Dispose() }
$identity = $manifest.Package.Identity
if ($identity.Name -ne 'MiskuTerminal' -or $identity.Publisher -ne 'CN=Misku Terminal' -or $identity.ProcessorArchitecture -ne 'x64') {
    throw 'Updates must retain the MiskuTerminal x64 identity and publisher.'
}
if ($manifest.Package.Applications.Application.Executable -ne 'misku.exe') { throw 'Unexpected application executable.' }
$version = [version]$identity.Version
if ($version.Revision -ne 0 -or $version.ToString(4) -cne $identity.Version -or
    @(@($version.Major,$version.Minor,$version.Build) | Where-Object { $_ -lt 0 -or $_ -gt 65535 }).Count -gt 0) {
    throw 'MSIX version must have four canonical numeric fields, with revision zero.'
}
$displayVersion = $version.ToString(3)
$fileName = "MiskuTerminal-$displayVersion-x64.msix"
$repository = 'https://github.com/JohannRojas/misku-terminal'
$releaseRoot = "$repository/releases/download/misku-v$displayVersion"
$channelRoot = "$repository/releases/download/misku-update-$Channel"
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$outputRoot = (Resolve-Path -LiteralPath $OutputDirectory).Path
$targetPackage = Join-Path $outputRoot $fileName
if ($package.FullName -ne $targetPackage) { Copy-Item -LiteralPath $package.FullName -Destination $targetPackage -Force }
$hash = (Get-FileHash -LiteralPath $targetPackage -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $fileName" | Set-Content -LiteralPath "$targetPackage.sha256" -Encoding ascii

$dependencies = ''
$required = @($manifest.GetElementsByTagName('PackageDependency'))
if ($required.Count) {
    if (-not $XamlDependencyPath) { throw 'The MSIX declares WinUI; provide its x64 dependency package.' }
    $dependencyArchive = [IO.Compression.ZipFile]::OpenRead((Get-Item -LiteralPath $XamlDependencyPath).FullName)
    try {
        $reader = [IO.StreamReader]::new($dependencyArchive.GetEntry('AppxManifest.xml').Open())
        try { [xml]$dependencyManifest = $reader.ReadToEnd() } finally { $reader.Dispose() }
    } finally { $dependencyArchive.Dispose() }
    $dependency = $dependencyManifest.Package.Identity
    if ($required.Count -ne 1 -or $required[0].Name -ne 'Microsoft.UI.Xaml.2.8' -or
        $dependency.Name -ne $required[0].Name -or $dependency.Publisher -ne $required[0].Publisher -or
        $dependency.ProcessorArchitecture -ne 'x64' -or [version]$dependency.Version -lt [version]$required[0].MinVersion) {
        throw 'Unexpected or incompatible WinUI dependency.'
    }
    Copy-Item -LiteralPath $XamlDependencyPath -Destination (Join-Path $outputRoot 'Microsoft.UI.Xaml.2.8-x64.appx') -Force
    $publisher = [Security.SecurityElement]::Escape($dependency.Publisher)
    $dependencies = @"
  <Dependencies>
    <Package Name="$($dependency.Name)" Publisher="$publisher" Version="$($dependency.Version)" ProcessorArchitecture="x64" Uri="$releaseRoot/Microsoft.UI.Xaml.2.8-x64.appx" />
  </Dependencies>
"@
}
if ($CertificatePath) {
    Copy-Item -LiteralPath $CertificatePath -Destination (Join-Path $outputRoot 'MiskuTerminal.cer') -Force
}
# The app owns update consent and scheduling. No App Installer background task
# can install an update after the user chooses Later or disables automatic checks.
@"
<?xml version="1.0" encoding="utf-8"?>
<AppInstaller xmlns="http://schemas.microsoft.com/appx/appinstaller/2021" Version="$version" Uri="$channelRoot/MiskuTerminal.appinstaller">
  <MainPackage Name="MiskuTerminal" Publisher="CN=Misku Terminal" Version="$version" ProcessorArchitecture="x64" Uri="$releaseRoot/$fileName" />
$dependencies
</AppInstaller>
"@ | Set-Content -LiteralPath (Join-Path $outputRoot 'MiskuTerminal.appinstaller') -Encoding utf8NoBOM
[ordered]@{
    schemaVersion = 1
    packageName = 'MiskuTerminal'
    publisher = 'CN=Misku Terminal'
    architecture = 'x64'
    version = $version.ToString(4)
    preview = ($Channel -eq 'preview')
    packageUri = "$releaseRoot/$fileName"
    sha256 = $hash
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputRoot 'update.json') -Encoding utf8NoBOM
Write-Output "Update artifacts: $outputRoot ($Channel $displayVersion)"
