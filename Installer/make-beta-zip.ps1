# Packages the current Release VST3 build + installer into a beta zip.
# Run after a full-solution rebuild (see build notes: rebuild the whole .sln,
# never ISHTAR_VST3 alone). No per-build edits needed - just rerun this.
param(
    [string]$ZipName = "ISHTAR-Beta.zip",
    [string]$OutputDir = $PSScriptRoot
)

$ErrorActionPreference = "Stop"

$repo   = Split-Path $PSScriptRoot -Parent
$bundle = Join-Path $repo "Builds\VisualStudio2022\x64\Release\VST3\ISHTAR.vst3"
$icon   = Join-Path $repo "Builds\VisualStudio2022\icon.ico"
$dll    = Join-Path $bundle "Contents\x86_64-win\ISHTAR.vst3"

if (-not (Test-Path $dll))  { throw "No built VST3 at $dll - rebuild the solution first." }
if (-not (Test-Path $icon)) { throw "No icon.ico at $icon - re-save the project from Projucer." }

# Freshness reminder: stale binaries have bitten us repeatedly on this drive.
$binary = Get-Item $dll
Write-Output ("Packaging binary built {0} ({1} bytes) - check this matches your last rebuild!" -f $binary.LastWriteTime, $binary.Length)

# Make sure the Explorer icon is inside the bundle (a rebuild wipes it).
Copy-Item $icon (Join-Path $bundle "Contents\Resources\ISHTAR.ico") -Force

# Stage: bundle (without desktop.ini - the installer recreates it with the
# right attributes, and Compress-Archive would drop it anyway) + installer.
$staging = Join-Path $env:TEMP "ISHTAR-beta-staging"
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory $staging | Out-Null
Copy-Item $bundle (Join-Path $staging "ISHTAR.vst3") -Recurse -Force
$ini = Join-Path $staging "ISHTAR.vst3\desktop.ini"
if (Test-Path $ini) { attrib -s -h -r "$ini"; Remove-Item $ini -Force }
Copy-Item (Join-Path $PSScriptRoot "Install ISHTAR.bat") $staging

$zip = Join-Path $OutputDir $ZipName
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$staging\*" -DestinationPath $zip
Remove-Item $staging -Recurse -Force

Write-Output "Created $zip"
