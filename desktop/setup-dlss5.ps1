# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 SamGCoder
param([string]$Root=(Split-Path $PSScriptRoot -Parent),[string]$Cache,[switch]$SkipTest)
$ErrorActionPreference='Stop'
if(Get-Process FalloutNV,dlss5-feed-host64 -ErrorAction SilentlyContinue){throw 'Close New Vegas and the DLSS helper before provisioning.'}
if(!$Cache){$Cache=Join-Path $Root 'runtime/dlss5/downloads'}
$target=Join-Path $Root 'runtime/dlss5/candidate/host64'
$stage=Join-Path $Root ('runtime/dlss5/staging-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $Cache,$stage,$target | Out-Null
Add-Type -AssemblyName System.IO.Compression
$pieces=@(
 @{name='feeder';url='https://github.com/jlrouzies-fr/DLSS5-Feeder/releases/download/v0.15.1/DLSS5-Feeder-0.15.1.zip';archive='2e44e81e691e75e532b9b7babc278a12615cb7f0fd9ef854da50e6ef17b272f4';entry='dlss5-feed-host64.exe';output='dlss5-feed-host64.exe';hash='034e6cdf382e6ea9164afd63c5b8a8aa0312894cb342593bc933e9fa435f8d02'},
 @{name='reshade';url='https://reshade.me/downloads/ReShade_Setup_6.8.0_Addon.exe';archive='afe4c8f13048306307983b8b3d41d5bf00a86820440b0e57dea10950e1176445';entry='ReShade64.dll';output='dxgi.dll';hash='0cee63f9c9f13f3ac909c5b4903f4dbb4b719a7ab3b4f13b0deaf83c814b94f7'},
 @{name='renodx';url='https://github.com/RankFTW/rhi-repo/releases/download/renodx-dlss5-4.5/renodx-dlss5_4.5.zip';archive='c6626cf227b07b31d417f45c43f072d754b8a85fd63ed8761ca817823fc539ca';entry='renodx-dlss5.addon64';output='renodx-dlss5.addon64';hash='e1c28fde0922b12fc10734e58c3d24a36808e575247f4fd4f36226540d7ee023'},
 @{name='dlss';url='https://github.com/RankFTW/rhi-repo/releases/download/dlss-310.9.1/nvngx_dlss_310.9.1.zip';archive='aaba83b288bd145c3808e8d7a0ba03cc8c8676d18ad984b1bfa6563046a3ba37';entry='nvngx_dlss.dll';output='nvngx_dlss.dll';hash='3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983'},
 @{name='dlssnr';url='https://github.com/RankFTW/rhi-repo/releases/download/dlssnr-310.8.0/nvngx_dlssnr_310.8.0.zip';archive='388c0a7912e15ec911b9c9e11a692142b11fe387ddf2b637d8c358138fffb3ac';entry='nvngx_dlssnr.dll';output='nvngx_dlssnr.dll';hash='e16bcf15e16e13f527491cdf7845b2fe6521a738d8f7c9c721866a8496e1fc8e'}
)
foreach($piece in $pieces){
 $archive=Join-Path $Cache ($piece.name+'.archive')
 if(!(Test-Path -LiteralPath $archive)){Invoke-WebRequest -UseBasicParsing $piece.url -OutFile $archive}
 if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $piece.archive){throw "Archive checksum mismatch: $($piece.name)"}
 $bytes=[IO.File]::ReadAllBytes($archive);$offset=-1
 # ReShade embeds an archive aligned to a PE file boundary; its installer is not executed.
 for($i=0;$i -le $bytes.Length-4;$i+=512){if($bytes[$i]-eq 0x50 -and $bytes[$i+1]-eq 0x4b -and $bytes[$i+2]-eq 3 -and $bytes[$i+3]-eq 4){$offset=$i;break}}
 if($offset-lt 0){throw "No archive payload: $($piece.name)"}
 if($offset){$payload=New-Object byte[] ($bytes.Length-$offset);[Array]::Copy($bytes,$offset,$payload,0,$payload.Length)}else{$payload=$bytes}
 $stream=[IO.MemoryStream]::new($payload,$false);$zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Read)
 try{
  $entries=@($zip.Entries | Where-Object {($_.FullName -split '[/\\]')[-1] -eq $piece.entry})
  if($entries.Count-ne 1){throw "Expected one $($piece.entry) in $($piece.name)"}
  $out=Join-Path $stage $piece.output;$inputStream=$entries[0].Open();$outputStream=[IO.File]::Create($out)
  try{$inputStream.CopyTo($outputStream)}finally{$inputStream.Dispose();$outputStream.Dispose()}
 }finally{$zip.Dispose();$stream.Dispose()}
 if((Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash -ne $piece.hash){throw "Binary checksum mismatch: $($piece.output)"}
 if($piece.name -in @('dlss','dlssnr')){
  $signature=Get-AuthenticodeSignature -LiteralPath $out
  if($signature.Status-ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=NVIDIA Corporation'){throw "Invalid NVIDIA signature: $($piece.output)"}
 }
}
foreach($piece in $pieces){
 $path=Join-Path $target $piece.output
 if((Test-Path -LiteralPath $path) -and (Get-FileHash -LiteralPath $path).Hash-ne $piece.hash){Copy-Item -LiteralPath $path -Destination ($path+'.'+[guid]::NewGuid().ToString('N')+'.backup')}
 Copy-Item -LiteralPath (Join-Path $stage $piece.output) -Destination $path -Force
}
$ini=Join-Path $target 'ReShade.ini'
if(!(Test-Path -LiteralPath $ini)){"[GENERAL]`r`nEffectSearchPaths=.\`r`nTextureSearchPaths=.\`r`n" | Set-Content -LiteralPath $ini -Encoding ASCII}
$pieces | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $Root 'runtime/dlss5/components.json') -Encoding UTF8
if(!$SkipTest){& (Join-Path $PSScriptRoot 'test-dlss5.ps1') -RuntimePath $target}
Write-Output 'Independent DLSS helper provisioned. Enable Experimental DLSS 5 in the game settings and restart. No Remix or game-folder renderer DLL was installed.'
