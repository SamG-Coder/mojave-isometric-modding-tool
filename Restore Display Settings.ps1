$ErrorActionPreference = 'Stop'
if (Get-Process -Name FalloutNV -ErrorAction SilentlyContinue) { throw 'Exit the game before restoring display settings.' }
$backup = Join-Path $PSScriptRoot 'backups\FalloutPrefs.original.ini'
$destination = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\FalloutNV\FalloutPrefs.ini'
if (-not (Test-Path -LiteralPath $backup)) { throw 'Original settings backup not found.' }
$original = Get-Content -LiteralPath $backup -Raw
$current = Get-Content -LiteralPath $destination -Raw
foreach ($key in @('bFull Screen','iSize W','iSize H')) {
    $pattern = '(?m)^' + [regex]::Escape($key) + '=.*$'
    $match = [regex]::Match($original,$pattern)
    if ($match.Success) { $current = [regex]::Replace($current,$pattern,$match.Value) }
}
Set-Content -LiteralPath $destination -Value $current -Encoding ascii -NoNewline
Write-Output 'Original fullscreen and resolution preferences restored. Other preferences were preserved.'
