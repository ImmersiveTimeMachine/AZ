"""GIMP 3: render the supplied pickup mesh silhouette as editable HUD vector art."""
from gi.repository import Gimp, Gio, Gegl
from pathlib import Path
import json

root = Path('C:/UnrealEngine/Games/AZ')
source = root / 'Saved/Pistol/icon-contours.json'
out = root / 'UI Design/CHALK_HUD_v03/unreal-art'
out.mkdir(parents=True, exist_ok=True)
geometry = json.loads(source.read_text())
width, height = geometry['width'], geometry['height']
image = Gimp.Image.new(width, height, Gimp.ImageBaseType.RGB)
path = Gimp.Path.new(image, 'Pistols_B side silhouette / editable mesh projection')
image.insert_path(path, None, 0)
count = 0
for contour in geometry['contours']:
    # Match the established rifle icon: muzzle faces left on every UI surface.
    points = [(width - x, y) for x, y in contour['points']]
    stroke = path.bezier_stroke_new_moveto(*points[0])
    for point in points[1:]:
        path.bezier_stroke_lineto(stroke, *point)
    path.stroke_close(stroke)
    count += 1
path.set_visible(False)
layer = Gimp.VectorLayer.new(image, path)
image.insert_layer(layer, None, 0)
layer.set_name('Pistol silhouette / supplied mesh geometry')
layer.set_enable_fill(True)
layer.set_fill_color(Gegl.Color.new('white'))
layer.set_stroke_color(Gegl.Color.new('rgba(0,0,0,0)'))
layer.set_stroke_width(0)
layer.refresh()
for procedure, filename in [('gimp-xcf-save', 'CHALK_Pistol_NATIVE.xcf'),
                            ('file-png-export', 'T_HUD_Pistol.png')]:
    proc = Gimp.get_pdb().lookup_procedure(procedure)
    config = proc.create_config()
    config.set_property('run-mode', Gimp.RunMode.NONINTERACTIVE)
    config.set_property('image', image)
    config.set_property('file', Gio.File.new_for_path(str(out / filename)))
    result = proc.run(config)
    if result.index(0) != Gimp.PDBStatusType.SUCCESS:
        raise RuntimeError('Pistol icon export failed: ' + filename)
receipt = dict(source=str(source), png=str(out / 'T_HUD_Pistol.png'),
               xcf=str(out / 'CHALK_Pistol_NATIVE.xcf'), width=width, height=height,
               native_vector_layers=1, paths=len(image.get_paths()), mesh_contours=count,
               muzzle_direction='left', preserves_mesh_proportions=True)
(out / 'pistol-export-receipt.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
image.delete()
print(json.dumps(receipt))
