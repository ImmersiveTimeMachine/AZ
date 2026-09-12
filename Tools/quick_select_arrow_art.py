"""Run in GIMP 3 Python: the editable triangle used by the mockup's center arrows."""
from gi.repository import Gimp, Gio
from pathlib import Path
import ast
import json

root = Path('C:/UnrealEngine/Games/AZ')
source = root / 'UI Design/CHALK_HUD_v03/sources/build_native_hud.py'
definitions = ast.parse(source.read_text(encoding='utf-8'))
definitions.body = [n for n in definitions.body if not isinstance(n, ast.Try)]
exec(compile(definitions, str(source), 'exec'), globals())
out = root / 'UI Design/CHALK_HUD_v03/unreal-art'
out.mkdir(parents=True, exist_ok=True)
img = Gimp.Image.new(64, 64, Gimp.ImageBaseType.RGB)
shape(img, None, 'Navigation / upward triangle', [(8, 50), (32, 10), (56, 50)], '#FFFFFF')
for procedure, filename in [('gimp-xcf-save', 'CHALK_ArrowUp_NATIVE.xcf'), ('file-png-export', 'T_HUD_ArrowUp.png')]:
    proc = Gimp.get_pdb().lookup_procedure(procedure)
    config = proc.create_config()
    config.set_property('run-mode', Gimp.RunMode.NONINTERACTIVE)
    config.set_property('image', img)
    config.set_property('file', Gio.File.new_for_path(str(out / filename)))
    if proc.run(config).index(0) != Gimp.PDBStatusType.SUCCESS:
        raise RuntimeError('Could not save ' + filename)
report = {'png': str(out / 'T_HUD_ArrowUp.png'), 'xcf': str(out / 'CHALK_ArrowUp_NATIVE.xcf'),
          'size': [64, 64], 'vector_layers': len(img.get_layers()), 'paths': len(img.get_paths())}
(out / 'arrow-export-receipt.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
img.delete()
print(json.dumps(report))
