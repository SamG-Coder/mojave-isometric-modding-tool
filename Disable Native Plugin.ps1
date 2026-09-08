$ErrorActionPreference = 'Stop'
$toolDirectory = $PSScriptRoot
$gameDirectory = Split-Path -Parent $toolDirectory
if (Get-Process -Name FalloutNV -ErrorAction SilentlyContinue) {
    throw 'Exit Fallout New Vegas first. In-game, F8 restores the ordinary camera and controls immediately.'
}
$pluginPath = Join-Path $gameDirectory 'Data\NVSE\Plugins\MojaveIsoNative.dll'
if (Test-Path -LiteralPath $pluginPath) {
    $destination = Join-Path $toolDirectory ('backups\disabled-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-MojaveIsoNative.dll')
    New-Item -ItemType Directory -Path (Join-Path $toolDirectory 'backups') -Force | Out-Null
    Move-Item -LiteralPath $pluginPath -Destination $destination
    Write-Output 'Native plugin disabled. Use Install built plugin in the tool to enable it again.'
} else { Write-Output 'Native plugin is already disabled.' }
