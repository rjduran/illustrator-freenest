"""Audit tracked files before sharing the repository. Does not scan ignored local files."""
import pathlib,re,subprocess,sys

root=pathlib.Path(__file__).resolve().parents[1]
arguments=['git','ls-files','-z']
if '--include-untracked' in sys.argv: arguments+=['--cached','--others','--exclude-standard']
paths=subprocess.check_output(arguments,cwd=root).decode('utf-8').split('\0')
paths=[p for p in paths if p]
if not paths:
    sys.exit('No tracked files: stage the intended source tree before auditing it.')
issues=[]
# Ignore rules also apply to files that were staged before a rule was added.
ignored=subprocess.run(['git','check-ignore','--no-index','--stdin'],cwd=root,
                       input='\n'.join(paths)+'\n',text=True,capture_output=True)
if ignored.returncode not in (0,1):
    sys.exit('Cannot check repository exclusions: '+ignored.stderr)
for name in ignored.stdout.splitlines():
    issues.append(f'{name}: excluded local file is tracked')
binary_suffixes={'.aip','.dll','.exe','.pdb','.obj','.lib','.zip','.pyc','.pipl','.ai','.ait','.nest'}
patterns=[
    ('personal Windows path',re.compile(r'[A-Za-z]:[\\/]+Users[\\/]+[^\\/\s]+')),
    ('private key',re.compile(r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----')),
    ('GitHub credential',re.compile(r'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{50,})')),
]
for name in paths:
    path=pathlib.PurePosixPath(name)
    illustrator_fixture=(path.suffix.lower()=='.ai' and path.parent==pathlib.PurePosixPath('examples/illustrator-tests'))
    if (path.suffix.lower() in binary_suffixes and not illustrator_fixture) or any(part in {'vendor','build','package','dist','legacy','.vs','__pycache__','illustratorapi','legalnotices'} for part in path.parts):
        issues.append(f'{name}: generated, vendor or SDK content is tracked')
    if name.endswith('build.local.psd1'):
        issues.append(f'{name}: local configuration is tracked')
    data=(root/name).read_bytes()
    if len(data)>2_000_000 and not illustrator_fixture:
        issues.append(f'{name}: unexpectedly large source file')
    # Illustrator fixtures can be binary/PDF; inspect readable content without treating them as source.
    try: text=data.decode('utf-8-sig',errors='replace' if illustrator_fixture else 'strict')
    except UnicodeDecodeError:
        issues.append(f'{name}: source is not UTF-8');continue
    for label,pattern in patterns:
        if pattern.search(text): issues.append(f'{name}: possible {label}')
if issues:
    print('\n'.join(issues));sys.exit(1)
print(f'PASS source audit: {len(paths)} source files; no generated binaries, SDK trees, local config, obvious personal paths or credential patterns')
