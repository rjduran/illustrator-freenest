param([string]$CMake)
$ErrorActionPreference = 'Stop'
if (!$CMake) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $CMake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if (!(Test-Path -LiteralPath $CMake)) { throw 'CMake not found. Supply -CMake with its full path.' }
Push-Location $PSScriptRoot
try {
    $dependency=Get-Content -LiteralPath dependencies/opennest.json -Raw | ConvertFrom-Json
    $pin=$dependency.revision
    if (!(Test-Path vendor/OpenNest/.git)) {
        New-Item -ItemType Directory -Force vendor | Out-Null
        git clone --no-checkout $dependency.repository vendor/OpenNest
        if ($LASTEXITCODE) { throw 'Clone failed.' }
        git -C vendor/OpenNest checkout --detach $pin
        if ($LASTEXITCODE) { throw 'Pinned checkout failed.' }
    }
    $revision = git -C vendor/OpenNest rev-parse HEAD
    if ($LASTEXITCODE) {throw 'Cannot read the OpenNest checkout.'}
    if ($revision -ne $pin) { throw 'OpenNest revision differs from the tested revision.' }
    $changes=git -C vendor/OpenNest status --porcelain
    if ($LASTEXITCODE -or $changes) {throw 'OpenNest has local changes; preserve them before building a release.'}
    & $CMake -S vendor/OpenNest/src/opennest_cpp -B build/engine -A x64
    if ($LASTEXITCODE) { throw 'Configuration failed.' }
    & $CMake --build build/engine --config Release --parallel 4
    if ($LASTEXITCODE) { throw 'Compilation failed.' }
    Write-Host 'OpenNest engine built in build/engine/Release.'
} finally { Pop-Location }
