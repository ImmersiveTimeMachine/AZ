from gi.repository import Gimp
import json
from pathlib import Path

out = Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/sources/native_vector_api.json')
classes = {
    'VectorLayer': ('new', 'get_path', 'refresh', 'set_enable_fill', 'set_enable_stroke', 'set_fill_color', 'set_stroke_color', 'set_stroke_width', 'set_stroke_width_unit', 'set_stroke_cap_style', 'set_stroke_join_style', 'set_stroke_dash_pattern'),
    'Path': ('new', 'bezier_stroke_new_moveto', 'bezier_stroke_lineto', 'bezier_stroke_cubicto', 'bezier_stroke_new_ellipse', 'stroke_close', 'stroke_new_from_points', 'get_strokes', 'stroke_get_points', 'stroke_scale', 'stroke_translate', 'copy'),
    'Image': ('insert_path', 'insert_layer', 'reorder_item', 'select_item', 'get_paths'),
    'TextLayer': ('new', 'set_markup', 'set_color', 'set_letter_spacing', 'set_font', 'set_font_size'),
}
data = {'version': Gimp.version(), 'images': [{'id': x.get_id(), 'name': x.get_name()} for x in Gimp.get_images()]}
for cls, names in classes.items():
    obj = getattr(Gimp, cls, None)
    data[cls] = {name: getattr(obj, name).__doc__ if hasattr(obj, name) else None for name in names}
for name in ('CapStyle', 'JoinStyle', 'PathStrokeType'):
    obj = getattr(Gimp, name)
    data[name] = {n: int(getattr(obj, n)) for n in dir(obj) if n.isupper() and isinstance(getattr(obj, n), int)}
proc = Gimp.get_pdb().lookup_procedure('plug-in-sel2path')
data['selection_to_path'] = {'available': proc is not None, 'arguments': []}
if proc:
    cfg = proc.create_config()
    for prop in cfg.list_properties():
        val = cfg.get_property(prop.name)
        data['selection_to_path']['arguments'].append({'name': prop.name, 'type': prop.value_type.name, 'default': str(val), 'blurb': prop.blurb})
out.write_text(json.dumps(data, indent=2), encoding='utf-8')
print('Read-only native vector introspection saved: '+str(out))
