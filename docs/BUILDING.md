# Building and testing

## Prerequisites

- Windows x64, Git and PowerShell.
- Visual Studio 2022 or 2026 with Desktop development with C++ and CMake tools.
- Python 3.12+.
- For the `.aip` only: Adobe Illustrator 2024 SDK and Illustrator 2024 for live tests. Obtain the SDK separately from [Adobe's developer portal](https://developer.adobe.com/illustrator/).

The engine and native geometry tests do not require the Adobe SDK. Other host versions and macOS are not validated.

## Build the native plugin

From the repository root:

```powershell
./native/build-native.ps1 -IllustratorSDKRoot 'C:\SDKs\Adobe Illustrator 2024 SDK'
```

Alternatively set `ILLUSTRATOR_SDK_ROOT`, or copy `native/build.local.example.psd1` to `native/build.local.psd1` and fill in your local paths. That file is ignored by Git. Explicit arguments override local preferences. The SDK root must contain `samplecode/common/includes/Plugin.hpp`.

The build script chooses v143 for Visual Studio 2022 and v145 for Visual Studio 2026. Override with `-PlatformToolset` if needed. Opening the Visual Studio project directly defaults to v143; supply the SDK root as an MSBuild property or environment variable.

OpenNest is fetched at the revision in `dependencies/opennest.json`. Builds stop if an existing checkout has a different revision or tracked changes; local work is not reset. Native checks run before files are packaged in `native/package/FreeNest`.

No installation occurs by default on a fresh checkout. To deploy deliberately, close Illustrator and run:

```powershell
./native/deploy-native.ps1
# Or build and deploy together:
./native/build-native.ps1 -IllustratorSDKRoot 'C:\SDKs\Adobe Illustrator 2024 SDK' -Deploy
```

The default destination is Illustrator 2024's `Plug-ins/Development` folder. Use `-PluginDirectory` to override it. The Windows account running the script needs write access to that folder. The deployment refuses to replace files while Illustrator is running and verifies copied hashes.

Local developers can set `DeployAfterBuild = $true` in ignored `native/build.local.psd1`. `-SkipDeploy` always overrides automatic copying. `-CreateArchive` optionally produces a ZIP; no script publishes to GitHub.

## Run SDK-free checks

```powershell
./build.ps1
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmake -S native/tests -B native/build/tests -A x64
& $cmake --build native/build/tests --config Release
Copy-Item build/engine/Release/nfp_nest.dll native/build/tests/Release/
./native/build/tests/Release/NestNativeChecks.exe
python -m pip install --target build/test-deps -r requirements-test.txt
python native/tests/test_multisheet.py
python native/tests/test_sheets.py
python native/tests/test_holes.py
python native/tests/test_labels.py
python tools/check_source_tree.py
```

The GitHub Actions workflow performs these checks on a Windows runner without downloading, uploading or compiling against the Adobe SDK. Its remote execution cannot be verified until the repository is hosted; the same commands are checked locally.

## Test inside Illustrator

Use Illustrator documents from `examples/illustrator-tests`, or create simple closed paths in a new document, and test:

1. Selecting closed paths without rejecting the layer container.
2. Dimensions matching the active artboard, units matching each document when opened, reversible conversions, and label options inheriting nesting units without separate radio buttons. Check that the default stroke remains 0.1 pt across unit switches.
3. Original geometry and appearance, copying onto correctly positioned output artboards.
4. Multiple sheets, unplaced items, maximum count and cancellation.
5. Named-path VDA labels, all four colors, text size and stroke weight.
6. Grouped and separate-layer label placement, including standalone labeling.
7. Undo, redo and failure rollback without losing original artwork.
8. Capture an L-shaped or curved sheet, deselect it, and nest parts with **Use captured boundary as sheet** checked. Verify the notch stays empty, the boundary curves are preserved, and unchecking the option restores rectangular nesting. Test unit switches, repeated irregular sheets, and the boundary artwork checkbox.
9. Select a compound part with holes and a smaller part that fits a hole. Check one placement per object, preserved cutouts/curves, hole clearance, fill rules, hole-safe labels and compound Undo/Redo. Disconnected/nested islands and overlapping contours must be rejected.
10. Observe elapsed/estimated time during nesting and labeling, including early solver completion, finishing beyond the calculation limit and cancellation. Check the separate Paths/Labels completion lists and disabled-label reporting.
11. Confirm the units dropdown contains only in, mm, cm, pt, px, m, ft. Enable **Label sheet boundary** and verify one sheet-name label below each sheet, outside its bounds, including custom sheets and separate label layers. Packing space must remain unchanged; disabling boundary artwork must disable sheet labels.
12. Open Window > FreeNest; check all three buttons against the equivalent Object commands, including cancellation and no-document states. Dock, undock, tab-group, collapse, close and reopen the panel. Check workspace restoration, light/dark themes, display scaling and Illustrator shutdown. A modal command must block duplicate panel commands.

Automated geometry checks do not establish Adobe's host behavior. Record live checks explicitly before a release.

Panel icon sources are the two SVG files in `native/Resources`. After editing their polygon artwork, run `python native/generate_panel_icons.py` to regenerate the checked-in PNG byte arrays in `native/Source/NestPanelIcons.h`, then rebuild. No additional image library or runtime asset installation is required.

For canvas-edge regression testing, position a source artboard near the right canvas edge and nest enough parts for multiple sheets. Output should wrap below the source, aligned with its left edge, without crossing canvas limits or existing boards. Include selected source paths with bounds inside the initial artboard: these must not create an intermediate row or send output to the far-left canvas. Repeat near the lower-right corner, with a shifted ruler origin and outside sheet labels enabled. If no free space remains, the command must leave the document unchanged.

## Prepare a versioned release

Run `./prepare-release.ps1` from the repository root. Optional `-IllustratorSDKRoot` and `-PlatformToolset` arguments override local build preferences. The command audits tracked source, builds the plugin without deploying, runs native and independent geometry checks, and packages a candidate under `dist/<version>/`. Python installs the pinned test dependency into ignored `build/test-deps`.

`VERSION` contains the major.minor.patch number. Windows resource constants are generated automatically by MSBuild from that file, including when building the Visual Studio project directly. `native/Resources/FreeNestVersion.h` is generated and ignored. `-CreateArchive` on the ordinary native build uses the same packager, but only the preparation command runs all independent checks.

The download has an explicit file list: FreeNest.aip, nfp_nest.dll, INSTALL.md, RELEASE_NOTES.md, release.json, LICENSE, THIRD_PARTY_NOTICES.md and retained third-party licenses. release.json records the version, target host, source commit (when one exists), and whether the source had uncommitted changes. Local tests, SDK files, private configuration and test artwork are excluded. Each version has a separate output directory. Re-running a version replaces its local candidate; never replace an already-published version with different binaries.

Before publication, finish the live Illustrator checks, finalize `release-notes/<version>.md`, commit the intended source, then rerun preparation on that clean commit. Confirm release.json identifies that commit with sourceDirty=false. Create the matching v<version> tag and GitHub release only when ready, then upload the ZIP, RELEASE_NOTES.md and SHA256SUMS.txt from dist/<version>. No script creates tags or publishes releases.
