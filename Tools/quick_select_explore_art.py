"""GIMP 3 Python: native editable walking icon for the Explore action."""
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
img = Gimp.Image.new(128, 128, Gimp.ImageBaseType.RGB)
white = '#FFFFFF'
ellipse(img, None, 60, 9, 20, 20, white, 'Explore / head')
shape(img, None, 'Explore / body', [(63, 33), (78, 37), (70, 69), (53, 64)], white)
shape(img, None, 'Explore / rear arm', [(62, 34), (68, 42), (48, 60), (26, 57),
                                     (27, 49), (44, 51)], white)
shape(img, None, 'Explore / forward arm', [(75, 35), (83, 39), (92, 57), (106, 61),
                                        (103, 69), (85, 65), (69, 43)], white)
shape(img, None, 'Explore / rear leg', [(53, 61), (66, 66), (59, 91), (34, 113),
                                     (24, 106), (48, 84)], white)
shape(img, None, 'Explore / forward leg', [(62, 62), (72, 62), (85, 79), (100, 109),
                                        (89, 115), (74, 87), (56, 69)], white)
for procedure, filename in [('gimp-xcf-save', 'CHALK_Explore_NATIVE.xcf'), ('file-png-export', 'T_HUD_Explore.png')]:
    proc = Gimp.get_pdb().lookup_procedure(procedure)
    config = proc.create_config()
    config.set_property('run-mode', Gimp.RunMode.NONINTERACTIVE)
    config.set_property('image', img)
    config.set_property('file', Gio.File.new_for_path(str(out / filename)))
    if proc.run(config).index(0) != Gimp.PDBStatusType.SUCCESS:
        raise RuntimeError('Could not save ' + filename)
report = {'png': str(out / 'T_HUD_Explore.png'), 'xcf': str(out / 'CHALK_Explore_NATIVE.xcf'),
          'size': [128, 128], 'native_vector_layers': len(img.get_layers()), 'paths': len(img.get_paths())}
(out / 'explore-export-receipt.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
img.delete()
print(json.dumps(report))
