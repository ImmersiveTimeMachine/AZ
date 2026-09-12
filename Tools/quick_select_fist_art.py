"""Run in GIMP 3 Python: editable native fist silhouette for intrinsic slot 0."""
from gi.repository import Gimp, Gio
from pathlib import Path
import ast
import json

project = Path('C:/UnrealEngine/Games/AZ')
source = project / 'UI Design/CHALK_HUD_v03/sources/build_native_hud.py'
definitions = ast.parse(source.read_text(encoding='utf-8'))
definitions.body = [n for n in definitions.body if not isinstance(n, ast.Try)]
exec(compile(definitions, str(source), 'exec'), globals())
out = project / 'UI Design/CHALK_HUD_v03/unreal-art'
out.mkdir(parents=True, exist_ok=True)
img = Gimp.Image.new(128, 128, Gimp.ImageBaseType.RGB)

def contour(name, start, segments, fill_color):
    path = Gimp.Path.new(img, name + ' / editable path')
    img.insert_path(path, None, 0)
    stroke = path.bezier_stroke_new_moveto(*start)
    for points in segments:
        if len(points) == 2:
            path.bezier_stroke_lineto(stroke, *points)
        else:
            path.bezier_stroke_cubicto(stroke, *points)
    path.stroke_close(stroke)
    return vec(img, None, name, path, fill_color)

contour('Fists / knuckles, palm and wrist', (24, 60), [
    (24, 34), (24, 26, 28, 22, 34, 22), (40, 22, 44, 27, 44, 34),
    (44, 27), (44, 17, 48, 14, 54, 14), (60, 14, 64, 19, 64, 27),
    (64, 29), (64, 21, 68, 18, 74, 18), (80, 18, 84, 23, 84, 31),
    (84, 36), (84, 28, 88, 26, 94, 26), (100, 26, 104, 31, 104, 38),
    (104, 61), (114, 60, 117, 67, 112, 77), (98, 92), (96, 111),
    (41, 111), (39, 92), (26, 85, 20, 76, 20, 66), (20, 62, 21, 61, 24, 60)
], '#FFFFFF')
for x, top, bottom in [(44, 34, 63), (64, 29, 58), (84, 36, 57)]:
    line(img, None, x, top, x, bottom, 3, '#303634', 'Fists / finger groove ' + str(x))
contour('Fists / thumb separation', (52, 60), [
    (53, 54, 58, 53, 63, 55), (82, 60), (99, 53),
    (107, 50, 112, 57, 109, 65), (95, 81),
    (90, 87, 81, 87, 76, 82), (57, 70), (52, 68, 50, 63, 52, 60)
], '#303634')
contour('Fists / folded thumb', (55, 61), [
    (56, 57, 59, 56, 63, 58), (82, 64), (100, 56),
    (105, 54, 108, 58, 106, 63), (93, 79),
    (88, 83, 83, 84, 78, 79), (59, 67), (56, 66, 54, 64, 55, 61)
], '#FFFFFF')

for procedure, filename in [('gimp-xcf-save', 'CHALK_Fists_NATIVE.xcf'), ('file-png-export', 'T_HUD_Fists.png')]:
    proc = Gimp.get_pdb().lookup_procedure(procedure)
    config = proc.create_config()
    config.set_property('run-mode', Gimp.RunMode.NONINTERACTIVE)
    config.set_property('image', img)
    config.set_property('file', Gio.File.new_for_path(str(out / filename)))
    if proc.run(config).index(0) != Gimp.PDBStatusType.SUCCESS:
        raise RuntimeError('Could not save ' + filename)
report = {'png': str(out / 'T_HUD_Fists.png'), 'xcf': str(out / 'CHALK_Fists_NATIVE.xcf'),
          'size': [128, 128], 'native_vector_layers': len(img.get_layers()),
          'retained_paths': len(img.get_paths()), 'source': str(project / 'Tools/quick_select_fist_art.py')}
(out / 'fists-export-receipt.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
img.delete()
print(json.dumps(report))
