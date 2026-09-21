"""Read back the saved GIMP source; do not edit or save it."""
from gi.repository import Gimp, Gio
from pathlib import Path
import json

folder = Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_QuestMap_v01/sources/map')
image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(str(folder / 'CHALK_L001_FieldJournal_NATIVE.xcf')))


def read_layers(items):
    values = []
    for item in items:
        filters = item.get_filters() if hasattr(item, 'get_filters') else []
        values.append({'name': item.get_name(), 'visible': item.get_visible(), 'opacity': item.get_opacity(),
                       'dimensions': [item.get_width(), item.get_height()], 'offsets': list(item.get_offsets())[-2:],
                       'filter_count': len(filters), 'mode': int(item.get_mode()),
                       'children': read_layers(item.get_children()) if item.is_group() else []})
    return values


result = {'reopened_saved_xcf': True, 'dimensions': [image.get_width(), image.get_height()],
          'layers': read_layers(image.get_layers())}
(folder / 'map-art-saved-readback.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
image.delete()
Gimp.displays_flush()
