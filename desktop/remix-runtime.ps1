# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 SamGCoder
param([ValidateSet('Off','On','Setup')][string]$Mode='Off',[string]$Root=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$game=Split-Path $Root -Parent
$store=Join-Path $Root 'runtime/remix'
$profile=Join-Path $store 'profile'
$cache=Join-Path $store 'runtime-1.5.2'
$manifest=Join-Path $store 'installed.json'
$interposer=Join-Path $game 'd3d9.dll'
try {
 if (Get-Process FalloutNV -ErrorAction SilentlyContinue) { throw 'Close New Vegas before changing the RTX runtime.' }
 New-Item -ItemType Directory -Force -Path $store | Out-Null
 if ($Mode -eq 'Off') {
  if ((Test-Path -LiteralPath $manifest) -and (Test-Path -LiteralPath $interposer)) {
   $owned=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
   if ((Get-FileHash -LiteralPath $interposer).Hash -ne $owned.sha256) { throw 'The renderer DLL has changed externally; it was preserved.' }
   Move-Item -LiteralPath $interposer -Destination (Join-Path $store 'd3d9.disabled.dll') -Force
  }
  $active=Join-Path $store 'active'
  if (Test-Path -LiteralPath $active) { Remove-Item -LiteralPath $active }
  exit 0
 }
 New-Item -ItemType Directory -Force -Path $profile | Out-Null
 if (!(Test-Path -LiteralPath (Join-Path $cache 'd3d9.dll'))) {
  $zip=Join-Path $store 'remix-1.5.2-release.zip'
  if (!(Test-Path -LiteralPath $zip)) { Invoke-WebRequest -UseBasicParsing 'https://github.com/NVIDIAGameWorks/rtx-remix/releases/download/remix-1.5.2/remix-1.5.2-release.zip' -OutFile $zip }
  if ((Get-FileHash -LiteralPath $zip).Hash.ToLower() -ne 'cc424be4dd1a0c6fd922bc6a7f8e5f6582baea7043a38afa6686d8b6faabad01') { throw 'RTX download checksum mismatch. No runtime installed.' }
  Expand-Archive -LiteralPath $zip -DestinationPath $cache -Force
 }
 $hash=(Get-FileHash -LiteralPath (Join-Path $cache 'd3d9.dll')).Hash
 if ((Test-Path -LiteralPath $interposer) -and (Get-FileHash -LiteralPath $interposer).Hash -ne $hash) { throw 'Another d3d9 renderer is installed. It was preserved; RTX setup stopped.' }
 $trex=Join-Path $game '.trex'
 if ((Test-Path -LiteralPath $trex) -and !(Test-Path -LiteralPath $manifest)) { throw 'An existing .trex runtime is present. It was preserved; RTX setup stopped.' }
 if (!(Test-Path -LiteralPath $trex)) { Copy-Item -LiteralPath (Join-Path $cache '.trex') -Destination $trex -Recurse }
 $helper=Join-Path $game 'NvRemixLauncher32.exe'
 $helperSource=Join-Path $cache 'NvRemixLauncher32.exe'
 if ((Test-Path -LiteralPath $helper) -and (Get-FileHash -LiteralPath $helper).Hash -ne (Get-FileHash -LiteralPath $helperSource).Hash) { throw 'A different Remix launcher is installed; it was preserved.' }
 Copy-Item -LiteralPath $helperSource -Destination $helper -Force
 @{version='1.5.2';sha256=$hash} | ConvertTo-Json | Set-Content -LiteralPath $manifest -Encoding UTF8
 $config=Join-Path $profile 'rtx.conf'
 if (!(Test-Path -LiteralPath $config)) {
  @'
# Generated experimental New Vegas profile. User edits are retained.
rtx.enableRaytracing = True
rtx.captureHotKey = F24
rtx.captureShowMenuOnHotkey = False
rtx.captureEnableMultiframe = False
rtx.captureOverwriteExistingCapture = False
rtx.useVertexCapture = True
'@ | Set-Content -LiteralPath $config -Encoding ASCII
 }
 Copy-Item -LiteralPath (Join-Path $cache 'd3d9.dll') -Destination $interposer -Force
 '1' | Set-Content -LiteralPath (Join-Path $store 'active') -Encoding ASCII
 exit 0
} catch {
 New-Item -ItemType Directory -Force -Path $store | Out-Null
 $_.Exception.Message | Set-Content -LiteralPath (Join-Path $store 'error.txt')
 exit 1
}
