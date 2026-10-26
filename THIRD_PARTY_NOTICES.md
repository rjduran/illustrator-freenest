# Third-party components

The root MIT license covers this project's original source, copyright (c) 2026 RJ Duran. It does not replace the licenses or terms of upstream components.

| Component | How used | License / source |
| --- | --- | --- |
| OpenNest, Petras Vestartas | NFP packing engine and bundled VDA engraving font API | MIT; [source](https://github.com/petrasvestartas/OpenNest), [retained notice](packaging/licenses/OpenNest-MIT.txt) |
| Clipper2, Angus Johnson | Polygon operations inside OpenNest | Boost Software License 1.0; [source](https://github.com/AngusJohnson/Clipper2), [notice](packaging/licenses/Boost-Software-License-1.0.txt) |
| Boost.Polygon and its vendored support headers | Minkowski / no-fit polygon operations inside OpenNest | Boost Software License 1.0; [source](https://www.boost.org/), same retained notice |
| Adobe Illustrator SDK | Native host API, metadata generator and sample framework referenced from the user's SDK installation | Adobe SDK terms; obtained separately from Adobe, excluded from this repository |

OpenNest's tested source revision is recorded in [dependencies/opennest.json](dependencies/opennest.json). Build scripts fetch that revision into ignored `vendor/OpenNest`; upstream source is unchanged. Retained credits are in [OpenNest-CREDITS.md](packaging/licenses/OpenNest-CREDITS.md).

The VDA font data is compiled into the upstream engine and accessed through `nfp_text_to_polylines`. We do not redistribute it as a separate installable font or claim separate authorship of it.

The native build references Adobe framework files in place rather than copying SDK source into the repository. Before distributing native binaries, review the SDK agreement supplied with your SDK (`legalnotices/EULAs.pdf`) and any applicable notices for compiled framework code. Do not upload the SDK as a GitHub artifact or commit it to the repository.

Python/Shapely are development/test dependencies, not bundled runtime components of the plugin.
