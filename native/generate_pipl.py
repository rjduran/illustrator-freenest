"""Generate plugin metadata using the user's local Adobe SDK tool."""
import json, pathlib, subprocess, sys
sdk=pathlib.Path(sys.argv[1])
resources=pathlib.Path(__file__).resolve().parent/'Resources'
subprocess.run([sys.executable,str(sdk/'tools/pipl/create_pipl.py'),'-input',
                json.dumps([{'name':'FreeNest','entry_point':'PluginMain'}]),
                '-output','plugin.pipl'],cwd=resources,check=True)
