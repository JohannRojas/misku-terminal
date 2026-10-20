#Requires -Version 7.0
[CmdletBinding()]
param(
    [string]$CertificateThumbprint,
    [string]$IdentityDirectory = (Join-Path $env:LOCALAPPDATA 'MiskuSigning'),
    [switch]$ConfigureGitHub
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repository = 'JohannRojas/misku-terminal'
$identityPath = Join-Path $IdentityDirectory 'identity.json'
$backupPath = Join-Path $IdentityDirectory 'signing-backup.clixml'
$identity = $null
if (Test-Path -LiteralPath $identityPath) {
    $identity = Get-Content -LiteralPath $identityPath -Raw | ConvertFrom-Json
    if ($CertificateThumbprint -and $identity.thumbprint -ne $CertificateThumbprint) {
        throw 'A signing identity already exists here. Use a separate directory for an intentional certificate rotation.'
    }
    $CertificateThumbprint = $identity.thumbprint
}

$certificate = if ($CertificateThumbprint) {
    Get-Item -LiteralPath ('Cert:\CurrentUser\My\' + $CertificateThumbprint) -ErrorAction SilentlyContinue
} else { $null }
if ($CertificateThumbprint -and -not $certificate) {
    throw 'The saved private key is missing. Restore the encrypted signing backup before continuing; do not generate a different identity.'
}
if (-not $certificate) {
    # Persist in the current user's private-key store; never add a trusted root.
    $certificate = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=Misku Terminal' `
        -FriendlyName 'Misku Terminal persistent release signing' -CertStoreLocation Cert:\CurrentUser\My `
        -KeyAlgorithm RSA -KeyLength 3072 -HashAlgorithm SHA256 -KeyExportPolicy Exportable `
        -KeyUsage DigitalSignature -NotAfter (Get-Date).AddYears(3)
}
if ($certificate.Subject -ne 'CN=Misku Terminal' -or -not $certificate.HasPrivateKey -or
    $certificate.NotAfter -lt (Get-Date).AddDays(30)) {
    throw 'The certificate must retain the Misku publisher, have a private key, and remain valid for at least 30 days.'
}

New-Item -ItemType Directory -Path $IdentityDirectory -Force | Out-Null
$publicPath = Join-Path $IdentityDirectory 'MiskuTerminal.cer'
Export-Certificate -Cert $certificate -FilePath $publicPath -Force | Out-Null
# Persist the selected identity before any network action, so retries reuse it.
[ordered]@{
    thumbprint = $certificate.Thumbprint
    publisher = $certificate.Subject
    expires = $certificate.NotAfter.ToUniversalTime().ToString('o')
    certificateSha256 = (Get-FileHash -LiteralPath $publicPath).Hash.ToLowerInvariant()
} | ConvertTo-Json | Set-Content -LiteralPath $identityPath -Encoding utf8NoBOM

function Set-MiskuGitHubSecret {
    param([string]$Name, [string]$Value)
    $processInfo = [Diagnostics.ProcessStartInfo]::new()
    $processInfo.FileName = (Get-Command gh -CommandType Application).Source
    $processInfo.UseShellExecute = $false
    $processInfo.CreateNoWindow = $true
    $processInfo.RedirectStandardInput = $true
    $processInfo.RedirectStandardOutput = $true
    $processInfo.RedirectStandardError = $true
    foreach ($argument in @('secret', 'set', $Name, '--repo', $repository)) {
        $processInfo.ArgumentList.Add($argument)
    }
    $process = [Diagnostics.Process]::Start($processInfo)
    try {
        $output = $process.StandardOutput.ReadToEndAsync()
        $errors = $process.StandardError.ReadToEndAsync()
        $process.StandardInput.Write($Value)
        $process.StandardInput.Close()
        $process.WaitForExit()
        # Do not log the input or process output when handling private keys.
        [void]$output.GetAwaiter().GetResult()
        [void]$errors.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) { throw "Could not set $Name. Verify GitHub repository access, then retry this script." }
    } finally { $process.Dispose() }
}

$temporaryPfx = Join-Path ([IO.Path]::GetTempPath()) ('misku-key-export-' + [guid]::NewGuid() + '.pfx')
try {
    $random = [byte[]]::new(32)
    [Security.Cryptography.RandomNumberGenerator]::Fill($random)
    $passwordText = [Convert]::ToBase64String($random)
    $password = ConvertTo-SecureString $passwordText -AsPlainText -Force
    Export-PfxCertificate -Cert $certificate -FilePath $temporaryPfx -Password $password -ChainOption EndEntityCertOnly | Out-Null
    $pfxText = [Convert]::ToBase64String([IO.File]::ReadAllBytes($temporaryPfx))
    # Export-Clixml encrypts BOTH values using Windows DPAPI for this user on
    # this computer. No plaintext private key or password is saved in the repo.
    [pscustomobject]@{
        Thumbprint = $certificate.Thumbprint
        Pfx = (ConvertTo-SecureString $pfxText -AsPlainText -Force)
        Password = $password
    } | Export-Clixml -LiteralPath $backupPath -Force
    if ($ConfigureGitHub) {
        Set-MiskuGitHubSecret -Name MISKU_SIGNING_PFX_BASE64 -Value $pfxText
        Set-MiskuGitHubSecret -Name MISKU_SIGNING_PASSWORD -Value $passwordText
    }
} finally {
    if (Test-Path -LiteralPath $temporaryPfx) { Remove-Item -LiteralPath $temporaryPfx -Force }
    $pfxText = $null
    $passwordText = $null
    if ($password) { $password.Dispose() }
}

Write-Output "Persistent Misku signing identity: $($certificate.Thumbprint)"
Write-Output "Public certificate: $publicPath"
Write-Output "Encrypted recovery backup (this Windows user and computer only): $backupPath"
if ($ConfigureGitHub) { Write-Output 'The two Misku signing secrets are configured in JohannRojas/misku-terminal.' }
