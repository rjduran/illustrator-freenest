# Security reporting

FreeNest is pre-release software. Use test documents when evaluating new builds.

If **Report a vulnerability** is available under the repository's **Security → Advisories** page, use it to report security issues privately. If the button is unavailable, open an issue asking for a private contact channel and leave out the technical details.

Include the FreeNest, Illustrator and Windows versions and reproduction steps in the private report. Avoid posting private artwork, credentials, SDK files or exploit details publicly. Use the bug-report template for ordinary functional issues.

Builds fetch a pinned public OpenNest revision. The runtime engine is loaded from beside the plugin, with Windows system-library search restrictions. The repository excludes downloaded dependencies and binaries; maintainers should review dependencies and SDK terms before releasing build artifacts.
