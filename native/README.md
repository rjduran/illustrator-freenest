# FreeNest for Illustrator — native working build

Created and maintained by RJ Duran. Copyright (c) 2026 RJ Duran. Original project code uses the MIT license included in the package.

Windows x64 plugin for **Illustrator 2024 (28.x)**. Native nesting, selection, settings and output have been exercised in Illustrator. Use the [live Illustrator checklist](../docs/BUILDING.md#test-inside-illustrator) to verify dialogs, compound output, appearance, cancellation, undo/redo and rollback before a release.

## Working directory and deployment

`build-native.ps1` builds the engine and plugin, runs native checks and updates `package/FreeNest`. Installation is opt-in on a fresh checkout. Use `-Deploy`, or set `DeployAfterBuild = $true` in ignored `build.local.psd1`, to copy `FreeNest.aip` and `nfp_nest.dll` into:

`C:\Program Files\Adobe\Adobe Illustrator 2024\Plug-ins\Development`

Close Illustrator before copying. If it is running, the build stays ready; run `deploy-native.ps1` after closing it. Other plugins in Development are untouched. Both copied files are verified by hash. ZIP creation is optional (`-CreateArchive`); `-SkipDeploy` builds without installing.


## About FreeNest

Choose **Object > FreeNest > About FreeNest** to open a compact, centered project information dialog. It includes the development version, RJ Duran attribution, MIT license, OpenNest/VDA credit and supported host platform. It is available even without an open document. Live Illustrator verification is still required for the compact dialog layout.

## Dockable command panel

Choose **Window → FreeNest** to show or hide the compact native command panel. Drag its title tab to dock it alongside other Illustrator panels, or leave it floating. The collapsed-panel icon has light and dark variants. Buttons open the same **Nest Selected Paths**, **Label Selected Paths**, and **Use Selected Path as Sheet** commands available under Object → FreeNest.

The panel uses Illustrator's native panel frame and ships inside `FreeNest.aip`; no separate CEP extension or installer is needed. Buttons are disabled with no document open and while a FreeNest command is running. Selection checks and settings remain in the existing dialogs. The initial panel is approximately 250 × 128 pixels at 100% display scaling. Its width can be resized; its height fits the three buttons with an 8-pixel bottom margin, scaled for the display. Docking, workspace restoration, theme changes and high-DPI behavior still require live Illustrator verification.

## Nest selected paths

1. Select simple closed paths or compound paths with enclosed holes, ungrouping ordinary groups first. Each compound path counts as one part. Symbol instances can be selected with paths; each instance counts as a single part.
2. Choose **Object → FreeNest → Nest Selected Paths**.
3. Sheet width and height start from the **active artboard** on every invocation. The units dropdown offers **in, mm, cm, pt, px, m, ft** and starts with the current document ruler units when available. Picas use points, Q uses millimeters, feet/inches use inches and yards use feet. Switching units converts displayed lengths without changing their physical sizes.
4. Set contour spacing, sheet edge margin, permitted orientations and calculation time. Contour spacing and sheet edge margin both default to **0.125 in** (3.175 mm).
5. Optionally enable **Create additional artboards as needed**. The maximum defaults to 10 and accepts 1–500. Only sheets that receive parts become artboards. With this option off, the solver uses one sheet.
6. Optionally enable **Label copies with object names**, using **Options** to configure label height and font.
7. **Include sheet boundary** is checked by default. Uncheck it to omit the drawn sheet outlines; output artboards are still created. Optionally check **Label sheet boundary** to add `FreeNest Sheet 1`, `FreeNest Sheet 2`, etc. below each sheet, outside its bounding box with a 3 mm gap. It uses the label height, font, color, stroke weight and layer-placement options; text shrinks to the sheet width when needed. Sheet labeling works independently of object-name labeling, defaults off, and is disabled when boundary artwork is off. It does not reserve or reduce any nesting area. The label is outside the output artboard, so artboard-only exports may omit it.
8. Click **Nest**. The centered progress dialog shows elapsed time, an approximate countdown based on the calculation limit, and labeling progress. It can finish early; preparation, validation, labeling and applying artwork can extend total time. Once the calculation limit is reached, the dialog reports finishing instead of claiming zero time remains. Cancellation is supported before document changes.

Results are copies on an **FreeNest Results** layer. Output artboards are named **FreeNest Sheet 1**, **FreeNest Sheet 2**, and so on. The first row is top-aligned with the source artboard and starts 20 mm to its right, moved farther right if necessary to clear existing artboards and source artwork. Rows fill the available horizontal canvas space, with 20 mm gaps. At the right canvas edge, placement wraps below the source artboard, aligning the new row with its left edge. Further rows continue below it; output is never sent to the far-left or top of the canvas. If the local area fills up, move the source artboard up or left or reduce the sheet count or size. Existing artboards and selected source bounds are preserved, and outside sheet labels are included in the reserved footprint. All output positions are planned before any artboards are added. The first output artboard becomes active. Originals remain in place.

The completion message uses separate **Paths** and **Labels** sections, with one list item per count or outcome. Label counts cover placed parts only; disabled labeling is reported explicitly. A narrow packing layout is valid; globally optimal packing is not guaranteed. If canvas space or the host artboard limit prevents creating the requested output, newly created output layers/artboards are rolled back.

The nesting dialog has four visible sections: **Sheet Material**, **Packing**, **Layout**, and **Labels**, with Units at the top and Nest/Cancel at the bottom. Sheet Material includes the captured-boundary option and pairs width/height and spacing/margin side by side. Packing contains style, orientations and calculation limit. Layout contains additional artboards and their maximum count, sheet boundary artwork, and centering. Labels contains object-name and sheet labels, with a **Label Options** button for appearance and placement. All controls remain visible without switching tabs.

## Symbol instances

Select instances on the artboard, then run **Nest Selected Paths**. FreeNest packs symbols made from one closed path by that path's contour, after applying the instance's existing transform. This lets six-sided symbols fit in staggered rows. Like ordinary path nesting, contour spacing measures the cut paths and excludes stroke width. Multi-piece symbols, clipping, effects, and other unsupported definitions fall back to their visible rectangles; the completion report counts both modes. Nested copies remain linked. Labels use the instance name or symbol name and fit within the packing footprint. Captured sheet boundaries still require a simple closed path.

## Packing style and centering

The nesting dialog offers **Narrow strip** (default), **Compact area**, and **Tight fit**. These select OpenNest's gravity, bounding-box-area, and convex-hull-area objectives respectively. Narrow strip favors less width; compact area considers width and height together; tight fit can favor interlocking outlines. Results are heuristic, and different styles can produce the same layout. All-rectangle jobs retain OpenNest's specialized fast packing path.

**Center finished layouts** defaults off. When enabled, each sheet's placed-part bounding box is centered independently. All parts and their labels move together, preserving rotations, holes and contour spacing; sheet boundaries and outside sheet labels remain fixed. For an irregular sheet, the full translation is applied only if every part still meets boundary and margin checks. Otherwise that sheet stays in its original position and the completion report lists it as not centered. This is bounding-box centering, not a center-of-mass calculation.

## Irregular sheet boundaries

1. Select exactly one simple, closed Illustrator path for the sheet. Concave shapes and curves are supported; groups, compound paths, holes and self-intersections are not.
2. Choose **Object → FreeNest → Use Selected Path as Sheet**. This captures the shape without changing the document.
3. Deselect the boundary, select only the parts, then choose **Object → FreeNest → Nest Selected Paths**.
4. Leave **Use captured boundary as sheet** checked. Width and height show the boundary's bounding dimensions and cannot be edited in this mode. Uncheck it to return to rectangular sheets; the active artboard supplies their initial dimensions.
5. Set spacing, edge margin, rotations and optional labels, then click **Nest**.

The captured shape is a snapshot for the loaded plugin session. Capture it again after editing the source path; it is not linked to that object or saved with the document. It remains available when switching documents until replaced or Illustrator is restarted. Keep the original boundary out of the parts selection.

Results are copies on new rectangular artboards sized around the irregular sheet. The parts are constrained to the actual captured outline, including concave notches. This does not nest directly into the original boundary on the source artboard. **Include sheet boundary** draws a copy of the captured path, preserving its curves; unchecking it omits the drawn outline while still constraining the packing. Additional artboards repeat the same captured sheet shape.

The solver uses flattened curves; custom-sheet edge clearance includes 0.2 pt for the part and boundary approximation. A separate containment check rejects crossings and insufficient clearance before document changes. Holes and multiple different sheet shapes are not supported in this first version. The capture/menu workflow still needs a live Illustrator check.

## Parts with holes

Use an Illustrator **compound path** containing one outer contour and one or more fully enclosed, disjoint holes. Select the compound object as a whole. FreeNest normalizes duplicate compound/child entries from Illustrator's selection so the part is processed once. Each compound object receives one placement and one object-name label.

The compound fill rule is respected: even-odd contours become holes; under the nonzero rule, opposite-direction interior contours become holes and same-direction contours remain filled. Disconnected outer islands, nested material islands, and touching/overlapping contours are rejected. Sheet-boundary capture still accepts a single path without holes.

The solver can place smaller parts in those holes while maintaining contour spacing. Validation compares the filled material and every outer/hole edge. Output duplicates the compound structure and transforms all original anchors and Bézier handles together. Labels stay outside the cutouts. Live Illustrator checks are still needed for compound selection, appearance and undo behavior.

Small cutouts do not need hole-aware search if no selected part could fit them with the permitted rotations and clearance. In that case, packing uses the outer outlines while output keeps every original cutout. This retains OpenNest's fast rectangular packing when applicable. If a hole could receive a part, FreeNest first computes a conservative outer-outline candidate, then spends the remaining calculation budget on hole-aware search. Both candidates pass the same filled-material validation; selection prefers more placed parts, fewer used sheets, more placed material, then a smaller bounding footprint. This protects the conservative result if hole-aware search times out with fewer placements. The baseline uses at most one second or 20% of the calculation limit, whichever is smaller.

## Single-stroke object labels

Name paths in Illustrator's **Layers** panel. The label text is the object name; automatically generated path names and whitespace-only names are skipped.

- **Object → FreeNest → Label Selected Paths** opens a centered modal dialog with label size, font, color, stroke weight and placement settings. Click **Label** to label selected paths without nesting, or **Cancel** to leave the artwork unchanged. Source geometry stays unchanged.
- In the nesting dialog, **Label copies with object names** labels the nested copies. Both placement modes receive the same part translation and rotation; original source paths remain untouched.
- The centered label-options dialog inherits the units selected in the nesting modal; standalone labeling starts with the current document ruler units. There are no separate unit radio buttons. It offers maximum height and **Regular** or **Bold** OpenNest VDA font. Maximum height controls text size; the default is 0.125 in (3.175 mm or 9 pt), displayed in the selected units.
- **Label placement** offers **Group with each polygon** (default) or **Separate labels layer**. Grouped nesting puts each copy and its label subgroup in a part group. Grouped standalone labeling wraps each original polygon and its new label subgroup on the polygon's existing layer; it changes the hierarchy but not the geometry. Separate mode puts label groups on an **FreeNest Labels** layer, leaving the polygons on their existing/results layer.
- **Label color (RGB)** offers Black (0,0,0), Blue (0,0,255), Red (255,0,0), and Magenta (255,0,255). **Blue** is the default. **Stroke weight (pt)** defaults to **0.1 pt** and is independent of the selected length units.
- Text is generated by OpenNest's built-in `nfp_text_to_polylines` function as unfilled Illustrator paths using the selected solid stroke color and weight. No system font installation or text outlining is required. See [OpenNest example 08](https://petrasvestartas.github.io/OpenNest/api/cpp/08_text/).
- Labels are positioned inside the path, with clearance adjusted for the chosen stroke weight and curve approximation. They shrink as needed down to 0.5 mm nominal height, or the requested height if smaller. Labels that cannot fit are skipped.
- Completion reports count labels added and paths skipped because they are unnamed, too small, or have unsupported names. The current adapter supports printable ASCII names up to 128 characters; unsupported characters are skipped rather than misrendered.
- Ordinary groups and multiline names are not supported. Compound-path labels use the compound object name and stay on material outside its holes. Engraving paths stay inside the part and do not change its nesting footprint. Verify your fabrication tool's color/layer mapping when exporting.

For a first test, name a rectangle `Panel-01` and a concave part `Bracket-A`, then run **Label Selected Paths**.

## Scope and behavior

- 1–500 individual, simple, closed paths; one copy per selected path; one or multiple identical sheets, rectangular or captured from a simple closed path.
- Concave paths and cubic Bézier curves are supported. Original anchors, handles and stroke widths are preserved in the copies. Only the solver outlines are flattened.
- Fixed, half-turn, quarter-turn, 45-degree and 30-degree orientations; no mirroring or scaling.
- Spacing refers to contours, excluding stroke widths and effects. Curve tolerance is 0.1 pt, allowed for by adding 0.2 pt between parts and 0.1 pt at sheet edges.
- Ordinary groups, disconnected compound islands, nested islands, touching/overlapping holes, sheet holes, clipping masks, open paths, text and raster artwork are not supported. Gradient/pattern positioning and live effects are unverified.
- The solver and label generator work on copied geometry. Illustrator API calls stay on the main thread. Cancellation and calculation limits are cooperative; preparation, validation and applying results add time.
- Spacing, margins, rotation, time, multi-artboard, sheet-boundary and label preferences persist within the loaded plugin session. Units reset to the current document ruler units, and width and height reset to the current active artboard each time. Preferences do not survive an application restart.
- Changes are named for Undo/Redo. Host undo grouping and the latest labeling UI/output still require live checks. To discard a trial nesting layout, remove its results layer and output artboards.

## Verification

The same native core used by the plugin is checked with the compiled engine DLL:

- Rectangle and irregular packing, spacing, containment, non-overlap, allowed rotations, oversized/unplaced parts and maximum sheet limits.
- Overflow across three sheets, a two-sheet cap, and irregular parts on multiple sheets; independent Shapely checks validate each sheet separately.
- Cubic flattening, source preservation, quarter-turn coordinates, rejected self-intersections, selected-layer filtering and threaded cancellation.
- Output artboard alignment and clearance in offset coordinates.
- Document-unit scales and reversible length conversion, including unchanged label height.
- VDA regular/bold text, buffer sizes, height scaling, whitespace, unsupported characters and tiny-part skipping. Independent checks cover 0.1 pt and 2 pt stroke-width clearance inside a concave part and after a 90-degree placement. Native checks also verify all four exact RGB values, blue/0.1 pt defaults, grouped default placement and the unchanged initial text size.

Run the extra geometry checks with:

```
python native/tests/test_multisheet.py
python native/tests/test_labels.py
```

They use Shapely from `build/test-deps` (install with `python -m pip install --target build/test-deps shapely` from the project root).

## Build dependencies and credits

Visual Studio C++ tools, CMake, Python and the local Adobe Illustrator 2024 SDK are required. `build-native.ps1 -IllustratorSDKRoot <path>` supplies the SDK location; environment and ignored local configuration alternatives are described in [the build guide](../docs/BUILDING.md). The build selects v143 for Visual Studio 2022 or v145 for Visual Studio 2026, with an optional override. SDK files are referenced in place and are not included in this repository.

OpenNest is pinned to `be5456b840edbf8615dc7d84b2acbc43f330d772`. The plugin loads the engine DLL beside its own module without starting a helper process. OpenNest, Clipper2 and Boost notices accompany the working package. Adobe SDK source and headers are not redistributed.
