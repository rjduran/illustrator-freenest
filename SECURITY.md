# Security reporting

This is an unreleased prototype, not a security-supported production release. Native plugins run inside Illustrator; use synthetic/test documents when evaluating new changes.

Before publishing this repository, the maintainer should enable GitHub private vulnerability reporting or choose a private contact channel. No reporting address is configured in this local-only repository, and no address is invented here.

Once a private channel is available, report suspected memory-safety issues, unsafe dependency loading or untrusted file handling there. Avoid posting private documents, credentials, SDK files or a working exploit publicly. Ordinary reproducible functional bugs can use the bug-report template.

Builds fetch a pinned public OpenNest revision. The runtime engine is loaded from beside the plugin, with Windows system-library search restrictions. The repository excludes downloaded dependencies and binaries; maintainers should review dependencies and SDK terms before releasing build artifacts.
