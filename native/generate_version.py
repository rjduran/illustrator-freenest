"""Generate Windows resource version constants from the repository VERSION file."""
import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
version = (root / 'VERSION').read_text(encoding='utf-8').strip()
if not re.fullmatch(r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)', version):
    raise SystemExit('VERSION must contain a major.minor.patch release number.')
numbers = [int(part) for part in version.split('.')]
if any(part > 65535 for part in numbers):
    raise SystemExit('Version components must fit Windows version metadata (0-65535).')
text = ('// Generated from VERSION. Do not edit.\n#pragma once\n'
        f'#define FREENEST_VERSION_STRING "{version}"\n'
        f'#define FREENEST_VERSION_RESOURCE_STRING "{version}\\0"\n'
        f'#define FREENEST_ABOUT_VERSION "Version {version}"\n'
        f'#define FREENEST_VERSION_NUMBERS {",".join(map(str, numbers))},0\n')
output = root / 'native/Resources/FreeNestVersion.h'
if not output.exists() or output.read_text(encoding='utf-8') != text:
    output.write_text(text, encoding='utf-8', newline='\n')
print(f'FreeNest version: {version}')
