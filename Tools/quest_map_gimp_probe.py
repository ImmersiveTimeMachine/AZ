"""Read GIMP's local native filter API for the real-map treatment; no image files changed."""
from gi.repository import Gimp, Gegl
from pathlib import Path
import json

out = Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_QuestMap_v01/sources/map/gimp-filter-api.json')
image = Gimp.Image.new(16, 16, Gimp.ImageBaseType.RGB)
layer = Gimp.Layer.new(image, 'API probe only', 16, 16, Gimp.ImageType.RGBA_IMAGE, 100, Gimp.LayerMode.NORMAL)
image.insert_layer(layer, None, 0)
result = {'drawable_methods': {}, 'filters': {}, 'layer_modes': {}}
for name in ('desaturate', 'curves_spline', 'levels', 'colorize_hsl', 'append_filter', 'merge_filter', 'get_histogram'):
    result['drawable_methods'][name] = str(getattr(Gimp.Drawable, name, None).__doc__)
for name in ('HSL_COLOR', 'LCH_COLOR', 'SOFTLIGHT', 'OVERLAY', 'NORMAL'):
    result['layer_modes'][name] = str(getattr(Gimp.LayerMode, name, None))
for operation in ('gegl:saturation', 'gegl:exposure', 'gegl:brightness-contrast', 'gegl:noise-rgb', 'gegl:levels'):
    try:
        filter = Gimp.DrawableFilter.new(layer, operation, operation)
        config = filter.get_config()
        result['filters'][operation] = {p.name: {'default': str(config.get_property(p.name)), 'type': p.value_type.name}
                                        for p in config.list_properties()}
    except Exception as error:
        result['filters'][operation] = {'error': str(error)}
out.write_text(json.dumps(result, indent=2), encoding='utf-8')
image.delete()
Gimp.displays_flush()
