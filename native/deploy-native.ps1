param(
    [string]$PluginDirectory='C:\Program Files\Adobe\Adobe Illustrator 2024\Plug-ins\Development',
    [string]$SourceDirectory=(Join-Path $PSScriptRoot 'package\FreeNest')
)
$ErrorActionPreference='Stop'
$localPath=Join-Path $PSScriptRoot 'build.local.psd1'
if (!$PSBoundParameters.ContainsKey('PluginDirectory') -and (Test-Path -LiteralPath $localPath)) {
    $local=Import-PowerShellDataFile -LiteralPath $localPath
    if ($local.PluginDirectory) {$PluginDirectory=$local.PluginDirectory}
}
$files=@('FreeNest.aip','nfp_nest.dll')
foreach ($name in $files) {
    if (!(Test-Path -LiteralPath (Join-Path $SourceDirectory $name) -PathType Leaf)) {
        throw "Build file missing: $name. Run build-native.ps1 first."
    }
}
if (Get-Process Illustrator -ErrorAction SilentlyContinue) {
    throw "The build is ready, but Illustrator is running. Close Illustrator, then run deploy-native.ps1 to copy it to $PluginDirectory."
}
New-Item -ItemType Directory -Path $PluginDirectory -Force | Out-Null
foreach ($name in $files) {
    $sourcePath=Join-Path $SourceDirectory $name
    $targetPath=Join-Path $PluginDirectory $name
    Copy-Item -LiteralPath $sourcePath -Destination $targetPath -Force
    if ((Get-FileHash -LiteralPath $sourcePath).Hash -ne (Get-FileHash -LiteralPath $targetPath).Hash) {
        throw "Copied file verification failed: $targetPath"
    }
}
Write-Host "Copied and verified FreeNest.aip and nfp_nest.dll in $PluginDirectory"
