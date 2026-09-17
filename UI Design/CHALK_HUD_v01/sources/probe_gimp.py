from gi.repository import Gimp
import json
from pathlib import Path
root = Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v01')
raw = Gimp.fonts_get_list('')
fonts = list(raw[1]) if isinstance(raw, tuple) else list(raw)
data = {'fonts': [x.get_name() for x in fonts if any(q in x.get_name().lower() for q in ('oswald','roboto','bahnschrift'))], 'images': [x.get_name() for x in Gimp.get_images()], 'methods': {k: getattr(Gimp.Image,k).__doc__ for k in ('select_polygon', 'select_rectangle', 'select_ellipse')}, 'group': Gimp.GroupLayer.new.__doc__, 'xcf': Gimp.file_save.__doc__ if hasattr(Gimp,'file_save') else None}
root.joinpath('sources/gimp_probe.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
