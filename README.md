# FreeNest for Illustrator

Free, open-source polygon nesting and single-stroke object labels for Adobe Illustrator, powered by [OpenNest](https://github.com/petrasvestartas/OpenNest).

**Status: unreleased development build.** The current host target is **Illustrator 2024 (28.x) on Windows x64**. Build from source using the separately obtained Illustrator SDK; no packaged public release is available yet. Other Illustrator versions, macOS, and production release packaging are not verified. This is an independent project, not an Adobe product.

## Features

- Nest closed paths and compound paths with holes while preserving original artwork and Bézier geometry. Smaller parts can nest in available holes.
- Initialize sheet dimensions and units from the current document; choose spacing, margins, rotation and a time limit.
- Place copies on output artboards, with optional overflow across multiple sheets.
- Capture a simple closed path as an irregular sheet, including concave or curved boundaries. Output artboards surround the shape; optional overflow repeats it.
- Generate object-name labels using OpenNest's VDA single-stroke font, independently or during nesting.
- Choose label height, regular/bold font, stroke weight, and Black, Blue, Red or Magenta.
- Default labels are blue, 0.1 pt, with a 0.125 in (3.175 mm) nominal maximum height.
- Group labels with their polygons or put label groups on a separate layer.
- Centered modal dialogs, elapsed time, approximate remaining time and a completion report with separate Paths and Labels lists.
- Cooperative cancellation and containment, rotation and spacing validation before document edits.

Ordinary groups, disconnected compound islands, nested islands, sheet holes, clipping masks, open paths, raster art and live text are unsupported. Compound parts with one outer contour and disjoint enclosed holes are supported. Labels currently accept printable ASCII object names up to 128 characters. Heuristic packing is not guaranteed globally optimal.

## Build and install

Install Visual Studio 2022 or 2026 with C++/CMake tools, Git, Python 3.12+ and the separately obtained Illustrator 2024 SDK. From the repository root, run:

    ./native/build-native.ps1 -IllustratorSDKRoot 'C:/SDKs/Adobe Illustrator 2024 SDK'

The tested OpenNest revision is fetched automatically. Output goes to native/package/FreeNest. The Adobe SDK is referenced in place and is not included.

A fresh checkout builds without installing or publishing. After closing Illustrator, deliberately install with:

    ./native/deploy-native.ps1

The default destination is Illustrator 2024's Plug-ins/Development folder. Keep FreeNest.aip and nfp_nest.dll together, restart Illustrator, and choose **Object > FreeNest > Nest Selected Paths** or **Object > FreeNest > Label Selected Paths**.

For one-click access, open **Window > FreeNest**. Its compact native panel contains buttons for nesting, labeling and capturing a sheet boundary. Drag the panel by its title tab to dock it beside other Illustrator panels; the buttons open the same dialogs as the Object menu commands.

## Versioned releases

The first release is **0.1.0**, currently an unpublished candidate. `VERSION` is the single version source for the About dialog, Windows plugin metadata and release ZIP filename.

Run `./prepare-release.ps1` to build, run native and independent geometry checks, and prepare `dist/<version>/` with a Windows ZIP, release notes and SHA-256 checksum. The Adobe SDK and test dependencies are required on the build machine; the SDK is not included in the download. Preparation never installs or publishes the plugin, creates a commit or makes a tag.

Release notes live in `release-notes/<version>.md`. Before a new version, update `VERSION` and add matching notes. Re-running preparation replaces the local candidate for that version; after publishing, use a new version for changes. When ready for GitHub, commit the tested source, tag it `v<version>`, and attach the ZIP, notes and checksum to that version's release manually. The current GitHub workflow runs checks only.

## Use FreeNest

The commands are under **Object > FreeNest**:

- **Nest Selected Paths** opens the nesting modal for the selected closed paths or compound parts. Originals stay in place; results go on new output artboards and a **FreeNest Results** layer.
- **Label Selected Paths** opens the labeling modal. Labels use object names from Illustrator's Layers panel. Grouped placement wraps each source polygon and label in a group; separate-layer placement leaves the source hierarchy intact.
- **Use Selected Path as Sheet** captures one simple closed path as an irregular sheet. Deselect the boundary, select the parts, then open the nesting modal with **Use captured boundary as sheet** checked. Capture again after editing the boundary; the snapshot lasts only for the loaded plugin session.

Output artboards are rectangular even when the usable sheet is irregular. Additional sheets repeat the same shape. Uncheck **Include sheet boundary** to omit the drawn boundary; packing is still constrained to the sheet.

### Initial defaults

| Setting | Default |
| --- | --- |
| Units | Current document ruler units; label options inherit the selected units |
| Rectangular sheet size | Active artboard width and height |
| Contour spacing | 0.125 in (3.175 mm) |
| Sheet-edge margin | 0.125 in (3.175 mm) |
| Include sheet boundary | On |
| Label sheet boundary | Off; optional sheet-name label outside the sheet |
| Additional artboards / labels during nesting | Off |
| Label height | 0.125 in (3.175 mm) nominal maximum; shrinks to fit |
| Label appearance | Blue RGB (0, 0, 255), 0.1 pt stroke |
| Label placement | Group with each polygon |

See the [feature guide](native/README.md) for all controls and examples. Up to 500 selected parts are supported; each compound object counts as one part. Compound holes must be disjoint and fully enclosed. Touching/overlapping contours are unsupported. Spacing excludes artwork strokes and effects. Remaining time is approximate; validation, labeling and applying artwork can extend the calculation time.

[Build and test instructions](docs/BUILDING.md) | [Feature guide](native/README.md)

## Repository layout

| Path | Purpose |
| --- | --- |
| native/Source | Illustrator integration, dialogs, geometry and solver adapter |
| native/Resources | Dialog resources; PiPL metadata is generated locally |
| native/tests | Native checks and independent geometry checks |
| dependencies/opennest.json | Pinned upstream revision |
| examples/illustrator-tests | Shared Illustrator documents for manual functionality tests |
| packaging | Packaging inputs and license notices |
| vendor, build, native/package | Ignored dependencies and generated output |

## Verification and limitations

Native and independent geometry checks cover rectangular/irregular sheets, containment, non-overlap, spacing, rotations, multiple sheets, curve flattening, compound fill rules, nesting inside holes, cancellation, units and VDA label clearance.

Development builds have been exercised in Illustrator. The [live Illustrator checklist](docs/BUILDING.md#test-inside-illustrator) remains the release checklist for selection, dialogs, appearance, undo/redo and rollback; automated geometry tests do not verify all host behavior.

The [Windows CI workflow](.github/workflows/checks.yml) requires no Adobe SDK and has no publishing steps. It has not run on GitHub yet. Follow the live-check checklist before distributing binaries.

Packing styles include **Narrow strip** (default), **Compact area**, and **Tight fit**. Optional **Center finished layouts** centers each completed nest without moving sheet boundaries. Irregular sheets retain their original layout if centering would violate the boundary or margin.

## About FreeNest

Choose **Object > FreeNest > About FreeNest** to view the current development version, author, MIT license and OpenNest credit. The About dialog is available with no document open.

## License and credits

FreeNest for Illustrator is created and maintained by **RJ Duran**. Original project code is copyright (c) 2026 RJ Duran and uses the [MIT license](LICENSE). OpenNest is developed by Petras Vestartas and includes geometry components with their own notices. See [third-party notices](THIRD_PARTY_NOTICES.md) and retained credits. The Adobe SDK is a separate prerequisite under Adobe's terms; it is not relicensed by this project.

External contributions are not currently being accepted. See [SECURITY.md](SECURITY.md) for security reporting.
