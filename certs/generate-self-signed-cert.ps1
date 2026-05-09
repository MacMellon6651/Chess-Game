param(
    [string]$DnsName = "localhost",
    [string]$Password = "chess-dev-pass"
)

$ErrorActionPreference = "Stop"

$certFolder = Split-Path -Parent $MyInvocation.MyCommand.Path
$pfxPath = Join-Path $certFolder "localhost.pfx"
$cerPath = Join-Path $certFolder "localhost.cer"

Write-Host "Generating self-signed certificate for $DnsName ..."
$cert = New-SelfSignedCertificate `
    -DnsName $DnsName `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -FriendlyName "Chess WSS Dev Certificate" `
    -NotAfter (Get-Date).AddYears(2) `
    -KeyExportPolicy Exportable `
    -KeyAlgorithm RSA `
    -KeyLength 2048 `
    -HashAlgorithm SHA256

$securePassword = ConvertTo-SecureString -String $Password -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $securePassword | Out-Null
Export-Certificate -Cert $cert -FilePath $cerPath | Out-Null

Write-Host "Certificate exported:"
Write-Host " - $pfxPath"
Write-Host " - $cerPath"
Write-Host ""
Write-Host "Installing certificate to CurrentUser Root store (to remove browser warning)..."
Import-Certificate -FilePath $cerPath -CertStoreLocation "Cert:\CurrentUser\Root" | Out-Null
Write-Host "Done. You can now use wss://localhost:18082"
