# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 SamGCoder
# Standalone compatibility test. Does not install a renderer or change game settings.
param([string]$RuntimePath=(Join-Path (Split-Path $PSScriptRoot -Parent) 'runtime/dlss5/candidate/host64'))
$ErrorActionPreference='Stop'
$RuntimePath=(Resolve-Path -LiteralPath $RuntimePath).ProviderPath
foreach ($name in @('nvngx_dlss.dll','nvngx_dlssnr.dll')) {
 $signature=Get-AuthenticodeSignature -LiteralPath (Join-Path $RuntimePath $name)
 if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=NVIDIA Corporation') {
  throw "NVIDIA signature verification failed for $name. Test not started."
 }
}
$helper=Join-Path $RuntimePath 'MojaveIsoNeuralHost.exe'
if (!(Test-Path -LiteralPath $helper)) { throw 'DLSS5-Feeder host is not provisioned.' }
$log=Join-Path $RuntimePath 'dlss5-feed-host.log'
$neural=Join-Path $RuntimePath 'ReShade.log'
# Archive old logs so a previous successful run cannot satisfy this check.
foreach ($path in @($log,$neural)) {
 if (Test-Path -LiteralPath $path) { Move-Item -LiteralPath $path -Destination ($path+'.'+[guid]::NewGuid().ToString('N')+'.previous') }
}
$process=Start-Process -FilePath $helper -ArgumentList '--test' -WorkingDirectory $RuntimePath -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(60000)) { $process.Kill(); throw 'DLSS test timed out after 60 seconds.' }
if ($process.ExitCode -ne 0) { throw "DLSS test failed (exit $($process.ExitCode)). See $log" }
if (!(Select-String -LiteralPath $log -SimpleMatch '--test finished: 300/300 evaluates succeeded' -Quiet) -or
    !(Select-String -LiteralPath $neural -SimpleMatch 'inline feature 18 evaluation succeeded' -Quiet)) {
 throw 'The logs do not confirm both 300 successful frames and actual neural rendering.'
}
Write-Output 'PASS: standalone DLSS 5 neural rendering confirmed. Game integration is a separate, unverified test.'
