[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackagePath,
    [Parameter(Mandatory)][string]$CertificateOutputPath,
    [string]$CertificateThumbprint
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$sdk = 'C:\Program Files (x86)\Windows Kits\10\bin'
$signTool = Get-ChildItem -LiteralPath $sdk -Filter signtool.exe -Recurse |
    Where-Object FullName -like '*\x64\signtool.exe' | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $signTool) { throw 'SignTool is missing.' }
$imported = $false
$cert = $null
$pfxPath = $null
try {
    if ($CertificateThumbprint) {
        $cert = Get-Item -LiteralPath ('Cert:\CurrentUser\My\' + $CertificateThumbprint)
    } else {
        if (-not $env:MISKU_SIGNING_PFX_BASE64 -or -not $env:MISKU_SIGNING_PASSWORD) {
            throw 'Configure the persistent MISKU_SIGNING_PFX_BASE64 and MISKU_SIGNING_PASSWORD secrets before publishing updates.'
        }
        $pfxPath = Join-Path ([IO.Path]::GetTempPath()) ('misku-signing-' + [guid]::NewGuid() + '.pfx')
        [IO.File]::WriteAllBytes($pfxPath, [Convert]::FromBase64String($env:MISKU_SIGNING_PFX_BASE64))
        $password = ConvertTo-SecureString -String $env:MISKU_SIGNING_PASSWORD -AsPlainText -Force
        $cert = Import-PfxCertificate -FilePath $pfxPath -Password $password -CertStoreLocation Cert:\CurrentUser\My
        $imported = $true
    }
    if (@($cert).Count -ne 1 -or $cert.Subject -ne 'CN=Misku Terminal' -or -not $cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date).AddDays(14)) {
        throw 'The signing certificate must match Misku, contain a private key and remain valid for at least 14 days.'
    }
    $usage = $cert.Extensions | Where-Object { $_.Oid.Value -eq '2.5.29.37' }
    if (-not @($usage.EnhancedKeyUsages | Where-Object Value -eq '1.3.6.1.5.5.7.3.3').Count) { throw 'A code-signing certificate is required.' }
    & $signTool.FullName sign /fd SHA256 /s My /sha1 $cert.Thumbprint $PackagePath
    if ($LASTEXITCODE -ne 0) { throw 'MSIX signing failed.' }
    Export-Certificate -Cert $cert -FilePath $CertificateOutputPath -Force | Out-Null
    # Validate the signature independently of the runner's machine trust store.
    Add-Type -AssemblyName System.Security.Cryptography.Pkcs
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead((Get-Item -LiteralPath $PackagePath).FullName)
    try {
        $stream = $zip.GetEntry('AppxSignature.p7x').Open()
        $memory = [IO.MemoryStream]::new()
        try { $stream.CopyTo($memory); $bytes = $memory.ToArray() } finally { $stream.Dispose(); $memory.Dispose() }
        if ([Text.Encoding]::ASCII.GetString($bytes, 0, 4) -ne 'PKCX') { throw 'Invalid MSIX signature header.' }
        $cms = [Security.Cryptography.Pkcs.SignedCms]::new()
        $cms.Decode($bytes[4..($bytes.Length - 1)])
        $cms.CheckSignature($true)
        if ($cms.SignerInfos.Count -ne 1 -or $cms.SignerInfos[0].Certificate.Thumbprint -ne $cert.Thumbprint) { throw 'Unexpected MSIX signer.' }
    } finally { $zip.Dispose() }
    Write-Output "Signed Misku with certificate $($cert.Thumbprint), valid until $($cert.NotAfter.ToString('yyyy-MM-dd'))."
} finally {
    if ($pfxPath -and (Test-Path -LiteralPath $pfxPath)) { Remove-Item -LiteralPath $pfxPath -Force }
    if ($imported -and $cert) { Remove-Item -LiteralPath ('Cert:\CurrentUser\My\' + $cert.Thumbprint) -DeleteKey }
    $env:MISKU_SIGNING_PFX_BASE64 = $null
    $env:MISKU_SIGNING_PASSWORD = $null
}
