"""Run in GIMP 3 Python: export the approved native magazine symbol only."""
from gi.repository import Gimp, Gio
from pathlib import Path
import ast
import json

project = Path('C:/UnrealEngine/Games/AZ')
source = project / 'UI Design/CHALK_HUD_v03/sources/build_native_hud.py'
definitions = ast.parse(source.read_text(encoding='utf-8'))
definitions.body = [node for node in definitions.body if not isinstance(node, ast.Try)]
exec(compile(definitions, str(source), 'exec'), globals())
out = project / 'UI Design/CHALK_HUD_v03/unreal-art'
out.mkdir(parents=True, exist_ok=True)
img = Gimp.Image.new(72, 120, Gimp.ImageBaseType.RGB)
mag_icon(img, None, 0, 0, '#FFFFFF', 4)

for procedure, filename in [('gimp-xcf-save', 'CHALK_Magazine_NATIVE.xcf'),
                            ('file-png-export', 'T_HUD_Magazine.png')]:
    proc = Gimp.get_pdb().lookup_procedure(procedure)
    config = proc.create_config()
    config.set_property('run-mode', Gimp.RunMode.NONINTERACTIVE)
    config.set_property('image', img)
    config.set_property('file', Gio.File.new_for_path(str(out / filename)))
    result = proc.run(config)
    if result.index(0) != Gimp.PDBStatusType.SUCCESS:
        raise RuntimeError('Failed to save ' + filename)

report = {'source': str(source), 'png': str(out / 'T_HUD_Magazine.png'),
          'xcf': str(out / 'CHALK_Magazine_NATIVE.xcf'), 'size': [72, 120],
          'native_vector_layers': 3, 'retained_paths': len(img.get_paths()),
          'description': 'Approved magazine silhouette and two grooves; transparent background; tint in UMG.'}
(out / 'magazine-export-receipt.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
img.delete()
print(json.dumps(report))
