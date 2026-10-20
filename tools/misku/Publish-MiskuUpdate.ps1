[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ArtifactDirectory,
    [Parameter(Mandatory)][string]$Commit,
    [ValidateSet('stable', 'preview')][string]$Channel
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repository = 'JohannRojas/misku-terminal'
$feedPath = Join-Path $ArtifactDirectory 'update.json'
$feed = Get-Content -LiteralPath $feedPath -Raw | ConvertFrom-Json
$version = ([version]$feed.version).ToString(3)
if ($feed.preview -ne ($Channel -eq 'preview')) { throw 'Release channel does not match the feed.' }
$tag = "misku-v$version"
$packagePath = Join-Path $ArtifactDirectory "MiskuTerminal-$version-x64.msix"
if ((Get-FileHash -LiteralPath $packagePath).Hash.ToLowerInvariant() -ne $feed.sha256) { throw 'Release package checksum mismatch.' }
$channelTag = "misku-update-$Channel"
# Prevent a late or manually dispatched older build from replacing a newer feed.
$existingDirectory = Join-Path ([IO.Path]::GetTempPath()) ('misku-feed-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $existingDirectory | Out-Null
try {
    & gh release view $channelTag --repo $repository --json tagName *> $null
    $channelExists = $LASTEXITCODE -eq 0
    if ($channelExists) {
        & gh release download $channelTag --repo $repository --pattern update.json --dir $existingDirectory
        if ($LASTEXITCODE -ne 0) { throw 'Cannot verify the currently published channel; refusing to overwrite it.' }
        $existing = Get-Content -LiteralPath (Join-Path $existingDirectory 'update.json') -Raw | ConvertFrom-Json
        if ([version]$existing.version -gt [version]$feed.version) { throw 'Refusing to downgrade the update channel.' }
        if ([version]$existing.version -eq [version]$feed.version -and $existing.sha256 -ne $feed.sha256) { throw 'A published version is immutable; increment the version.' }
    }
    $notes = Join-Path $existingDirectory 'notes.md'
    @"
Misku Terminal $version, built from commit $Commit.

Install MiskuTerminal.appinstaller to keep your existing Misku installation and settings. Updates can be checked and prepared from About inside Misku. Preparing an update does not close active terminals.

For this development distribution, trust the included MiskuTerminal.cer once in Local Computer > Trusted People. The signing identity is reused for subsequent versions.

MSIX SHA256: $($feed.sha256)
Certificate SHA256: $((Get-FileHash -LiteralPath (Join-Path $ArtifactDirectory 'MiskuTerminal.cer')).Hash.ToLowerInvariant())
"@ | Set-Content -LiteralPath $notes -Encoding utf8NoBOM
    $releaseJson = & gh release view $tag --repo $repository --json isDraft,targetCommitish 2>$null
    $releaseExists = $LASTEXITCODE -eq 0
    $release = if ($releaseExists) { $releaseJson | ConvertFrom-Json } else { $null }
    if ($releaseExists -and $release.targetCommitish -ne $Commit) {
        throw 'This version belongs to a different commit. Choose a new version.'
    }
    if (-not $releaseExists) {
        & gh release create $tag --repo $repository --target $Commit --title "Misku Terminal $version" --notes-file $notes --draft
        if ($LASTEXITCODE -ne 0) { throw 'Release draft creation failed.' }
    }
    $assets = @(Get-ChildItem -LiteralPath $ArtifactDirectory -File | Where-Object Extension -in @('.msix','.appx','.cer','.appinstaller','.sha256','.zip') | ForEach-Object FullName)
    if ($releaseExists -and -not $release.isDraft) {
        # A retry after publication may finish discovery, but must first prove
        # that every immutable asset is identical. Never clobber a live package.
        $publishedDirectory = Join-Path $existingDirectory 'published'
        & gh release download $tag --repo $repository --dir $publishedDirectory
        if ($LASTEXITCODE -ne 0) { throw 'Cannot verify the already published assets.' }
        foreach ($asset in $assets) {
            $published = Join-Path $publishedDirectory ([IO.Path]::GetFileName($asset))
            if (-not (Test-Path -LiteralPath $published) -or
                (Get-FileHash -LiteralPath $asset).Hash -ne (Get-FileHash -LiteralPath $published).Hash) {
                throw 'A published version is immutable; increment the version.'
            }
        }
    } else {
        & gh release upload $tag @assets --repo $repository --clobber
        if ($LASTEXITCODE -ne 0) { throw 'Release artifact upload failed.' }
        if ($Channel -eq 'preview') { & gh release edit $tag --repo $repository --draft=false --prerelease --latest=false }
        else { & gh release edit $tag --repo $repository --draft=false --prerelease=false --latest }
        if ($LASTEXITCODE -ne 0) { throw 'Release publication failed.' }
    }
    if (-not $channelExists) {
        & gh release create $channelTag --repo $repository --target $Commit --title "Misku $Channel update channel" --notes 'Update metadata for Misku. Install versioned releases for release notes and packages.' --prerelease --latest=false
        if ($LASTEXITCODE -ne 0) { throw 'Update channel creation failed.' }
    }
    # Publish discovery LAST. Every URL in this feed is already available.
    & gh release upload $channelTag (Join-Path $ArtifactDirectory 'MiskuTerminal.appinstaller') --repo $repository --clobber
    if ($LASTEXITCODE -ne 0) { throw 'App Installer channel update failed.' }
    & gh release upload $channelTag $feedPath --repo $repository --clobber
    if ($LASTEXITCODE -ne 0) { throw 'Update feed publication failed.' }
} finally {
    $resolved = [IO.Path]::GetFullPath($existingDirectory)
    $temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected temporary feed directory.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
