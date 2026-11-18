param(
    [string]$IllustratorSDKRoot=$env:ILLUSTRATOR_SDK_ROOT,
    [string]$PluginDirectory='C:\Program Files\Adobe\Adobe Illustrator 2024\Plug-ins\Development',
    [switch]$SkipDeploy,
    [switch]$CreateArchive,
    [switch]$Deploy,
    [string]$PlatformToolset
)
$ErrorActionPreference='Stop'
$local=@{}
$localPath=Join-Path $PSScriptRoot 'build.local.psd1'
if (Test-Path -LiteralPath $localPath) {$local=Import-PowerShellDataFile -LiteralPath $localPath}
if (!$IllustratorSDKRoot) {$IllustratorSDKRoot=$local.IllustratorSDKRoot}
if (!$IllustratorSDKRoot) {throw 'Supply -IllustratorSDKRoot, set ILLUSTRATOR_SDK_ROOT, or create native/build.local.psd1 from its example.'}
if (!$PSBoundParameters.ContainsKey('PluginDirectory') -and $local.PluginDirectory) {$PluginDirectory=$local.PluginDirectory}
if (!$PlatformToolset -and $local.PlatformToolset) {$PlatformToolset=$local.PlatformToolset}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) {throw 'Visual Studio C++ tools were not found.'}
if (!$PlatformToolset) {
    $vsVersion=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
    $PlatformToolset=if ([version]$vsVersion -ge [version]'18.0') {'v145'} else {'v143'}
}
$msbuild=Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
$cmake=Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (!(Test-Path -LiteralPath (Join-Path $IllustratorSDKRoot 'samplecode\common\includes\Plugin.hpp'))) {throw 'Illustrator 2024 SDK was not found.'}
Push-Location $PSScriptRoot
try {
    & ../build.ps1 -CMake $cmake
    if ($LASTEXITCODE) {throw 'Engine build failed.'}
    python generate_pipl.py $IllustratorSDKRoot
    if ($LASTEXITCODE) {throw 'Plugin metadata generation failed.'}
    & $msbuild FreeNest.vcxproj /p:Configuration=Release /p:Platform=x64 "/p:IllustratorSDKRoot=$IllustratorSDKRoot" "/p:PlatformToolset=$PlatformToolset" /m:4 /verbosity:minimal
    if ($LASTEXITCODE) {throw 'Native plugin build failed.'}
    $version=(Get-Content -LiteralPath ../VERSION -Raw).Trim()
    $pluginVersion=(Get-Item -LiteralPath build/win/x64/Release/FreeNest.aip).VersionInfo
    if ($pluginVersion.FileVersion -ne $version -or $pluginVersion.ProductVersion -ne $version) {
        throw "The built plugin version does not match VERSION ($version). Rebuild before packaging or deploying."
    }
    & $cmake -S tests -B build/tests -A x64
    if ($LASTEXITCODE) {throw 'Native test configuration failed.'}
    & $cmake --build build/tests --config Release
    if ($LASTEXITCODE) {throw 'Native test build failed.'}
    Copy-Item ../build/engine/Release/nfp_nest.dll build/tests/Release/
    & ./build/tests/Release/NestNativeChecks.exe
    if ($LASTEXITCODE) {throw 'Native checks failed.'}
    $destination=Join-Path $PSScriptRoot 'package\FreeNest'
    New-Item -ItemType Directory -Force $destination | Out-Null
    Copy-Item build/win/x64/Release/FreeNest.aip $destination
    Copy-Item ../build/engine/Release/nfp_nest.dll $destination
    Copy-Item README.md $destination
    Copy-Item ../packaging/licenses $destination -Recurse -Force
    Copy-Item ../LICENSE $destination
    if ($CreateArchive) {
        & ../tools/package-release.ps1
    }
    Write-Host "Native build ready in $destination"
    if (!$SkipDeploy -and ($Deploy -or $local.DeployAfterBuild)) { & ./deploy-native.ps1 -PluginDirectory $PluginDirectory -SourceDirectory $destination }
} finally {Pop-Location}
