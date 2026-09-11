# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 SamG-Coder
[CmdletBinding()]
param([switch]$Restore)
$ErrorActionPreference = 'Stop'
if (Get-Process FalloutNV -ErrorAction SilentlyContinue) {
    throw 'Close Fallout: New Vegas before changing intro video settings.'
}
$gameDirectory = Split-Path -Parent $PSScriptRoot
if (!(Test-Path -LiteralPath (Join-Path $gameDirectory 'FalloutNV.exe'))) {
    throw 'Run this script from IsometricModdingTool inside the Fallout New Vegas installation.'
}
$stateDirectory = Join-Path $PSScriptRoot 'runtime/intro-videos'
$statePath = Join-Path $stateDirectory 'settings.json'
$moviePath = Join-Path $gameDirectory 'Data/Video/FNVIntro.bik'
$backupMovie = Join-Path $stateDirectory 'FNVIntro.bik'
$settingsDirectory = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games/FalloutNV'
if (!(Test-Path -LiteralPath $settingsDirectory)) { throw 'Fallout New Vegas user settings were not found.' }
if (-not ('MojaveIntroIni' -as [type])) {
    Add-Type @'
using System.Runtime.InteropServices;
using System.Text;
public static class MojaveIntroIni {
    [DllImport("kernel32", CharSet=CharSet.Unicode)]
    public static extern uint GetPrivateProfileString(string section, string key, string fallback, StringBuilder value, uint size, string file);
    [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern bool WritePrivateProfileString(string section, string key, string value, string file);
}
'@
}
function Write-IntroSetting($file, $key, $value) {
    $info = Get-Item -LiteralPath $file
    $wasReadOnly = $info.IsReadOnly
    try {
        if ($wasReadOnly) { $info.IsReadOnly = $false }
        if (![MojaveIntroIni]::WritePrivateProfileString('General', $key, $value, $file)) { throw 'Could not write intro INI setting.' }
    } finally { if ($wasReadOnly) { $info.IsReadOnly = $true } }
}
if ($Restore) {
    if (!(Test-Path -LiteralPath $statePath)) { Write-Output 'No saved intro settings to restore.'; return }
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    if ((Test-Path -LiteralPath $backupMovie) -and (Test-Path -LiteralPath $moviePath)) {
        throw 'A new FNVIntro.bik already exists. The saved original is retained; resolve the duplicate before restoring.'
    }
    foreach ($entry in $state) {
        $value = if ($entry.existed) { [string]$entry.value } else { $null }
        Write-IntroSetting $entry.file $entry.key $value
    }
    if (Test-Path -LiteralPath $backupMovie) { Move-Item -LiteralPath $backupMovie -Destination $moviePath }
    Remove-Item -LiteralPath $statePath
    Write-Output 'Original intro videos and settings restored.'
    return
}
New-Item -ItemType Directory -Path $stateDirectory -Force | Out-Null
if (!(Test-Path -LiteralPath $statePath)) {
    $state = @(
        foreach ($name in @('Fallout.ini', 'FalloutPrefs.ini', 'FalloutCustom.ini')) {
            $file = Join-Path $settingsDirectory $name
            if (!(Test-Path -LiteralPath $file)) { continue }
            foreach ($key in @('SIntroSequence', 'SMainMenuMovieIntro', 'sIntroMovie')) {
                $buffer = New-Object Text.StringBuilder 2048
                [void][MojaveIntroIni]::GetPrivateProfileString('General', $key, '__MISSING__', $buffer, 2048, $file)
                @{file=$file; key=$key; existed=($buffer.ToString() -ne '__MISSING__'); value=$buffer.ToString()}
            }
        }
    )
    if (!$state.Count) { throw 'No game INI files found; launch the game once first.' }
    $state | ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8
} else { $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json }
foreach ($entry in $state) {
    Write-IntroSetting $entry.file $entry.key '0'
}
if (Test-Path -LiteralPath $moviePath) {
    if (Test-Path -LiteralPath $backupMovie) { throw 'An intro backup already exists; it has not been overwritten.' }
    Move-Item -LiteralPath $moviePath -Destination $backupMovie
}
Write-Output 'Startup videos and the New Game movie are disabled. Character creation is unchanged.'
