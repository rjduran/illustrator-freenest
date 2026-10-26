# Package an already-built plugin. Use prepare-release.ps1 for the full build/check workflow.
$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot -Parent
$releaseVersion=(Get-Content -LiteralPath (Join-Path $repoRoot 'VERSION') -Raw).Trim()
if ($releaseVersion -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') {throw 'Invalid VERSION.'}
$notes=Join-Path $repoRoot "release-notes/$releaseVersion.md"
if (!(Test-Path -LiteralPath $notes)) {throw "Missing release notes: release-notes/$releaseVersion.md"}
$plugin=Join-Path $repoRoot 'native/build/win/x64/Release/FreeNest.aip'
$engine=Join-Path $repoRoot 'build/engine/Release/nfp_nest.dll'
foreach ($file in @($plugin,$engine)) {if (!(Test-Path -LiteralPath $file)) {throw 'Build the plugin before packaging.'}}
$fileVersion=(Get-Item -LiteralPath $plugin).VersionInfo.FileVersion
if ($fileVersion -ne $releaseVersion) {throw "Plugin version $fileVersion does not match VERSION $releaseVersion. Rebuild first."}
# Always stage an explicit file list in a new directory; old working-package files cannot leak in.
$stage=Join-Path $repoRoot ('build/release-staging/'+[guid]::NewGuid().ToString())
$bundle=Join-Path $stage 'FreeNest'
New-Item -ItemType Directory -Path $bundle -Force | Out-Null
Copy-Item -LiteralPath $plugin,$engine -Destination $bundle
Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE'),(Join-Path $repoRoot 'THIRD_PARTY_NOTICES.md'),(Join-Path $repoRoot 'packaging/INSTALL.md') -Destination $bundle
Copy-Item -LiteralPath (Join-Path $repoRoot 'packaging/licenses') -Destination $bundle -Recurse
Copy-Item -LiteralPath $notes -Destination (Join-Path $bundle 'RELEASE_NOTES.md')
$sourceCommit=& git -C $repoRoot rev-parse --verify HEAD 2>$null
if ($LASTEXITCODE) {$sourceCommit=$null}
$sourceChanges=& git -C $repoRoot status --porcelain
if ($LASTEXITCODE) {throw 'Cannot read Git status.'}
@{version=$releaseVersion;platform='Windows x64';illustrator='2024 (28.x)';sourceCommit=$sourceCommit;sourceDirty=[bool]$sourceChanges} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $bundle 'release.json') -Encoding utf8
$releaseDir=Join-Path $repoRoot "dist/$releaseVersion"
New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null
$zipName="FreeNest-for-Illustrator-$releaseVersion-Windows-x64.zip"
$zip=Join-Path $releaseDir $zipName
Compress-Archive -LiteralPath $bundle -DestinationPath (Join-Path $stage $zipName)
Copy-Item -LiteralPath (Join-Path $stage $zipName) -Destination $zip -Force
Copy-Item -LiteralPath $notes -Destination (Join-Path $releaseDir 'RELEASE_NOTES.md') -Force
$checksum=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
"$checksum  $zipName" | Set-Content -LiteralPath (Join-Path $releaseDir 'SHA256SUMS.txt') -Encoding ascii
Write-Host "Local release package ready: $zip"
Write-Host 'Not published. Review and test before uploading a release.'
