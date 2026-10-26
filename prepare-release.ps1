param(
    [string]$IllustratorSDKRoot=$env:ILLUSTRATOR_SDK_ROOT,
    [string]$PlatformToolset
)
$ErrorActionPreference='Stop'
Push-Location $PSScriptRoot
try {
    python tools/check_source_tree.py --include-untracked
    if ($LASTEXITCODE) {throw 'Source audit failed.'}
    $buildArgs=@{SkipDeploy=$true}
    if ($IllustratorSDKRoot) {$buildArgs.IllustratorSDKRoot=$IllustratorSDKRoot}
    if ($PlatformToolset) {$buildArgs.PlatformToolset=$PlatformToolset}
    & ./native/build-native.ps1 @buildArgs
    python -m pip install --upgrade --target build/test-deps -r requirements-test.txt
    if ($LASTEXITCODE) {throw 'Test dependency installation failed.'}
    foreach ($test in @('test_multisheet.py','test_sheets.py','test_holes.py','test_labels.py')) {
        python "native/tests/$test"
        if ($LASTEXITCODE) {throw "Independent check failed: $test"}
    }
    & ./tools/package-release.ps1
} finally {Pop-Location}
