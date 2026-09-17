"""Open saved mockups in a dedicated GIMP GUI; leave other sessions intact."""
from gi.repository import Gimp, Gio
from pathlib import Path
import json

root = Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_Mode_Indicators_v01')
for filename in ['CHALK_Mode_01_NATIVE.xcf', 'CHALK_Mode_02_NATIVE.xcf',
                 'CHALK_Mode_03_NATIVE.xcf', 'CHALK_Mode_Comparison_NATIVE.xcf']:
    img = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(str(root / filename)))
    Gimp.Display.new(img)
Gimp.displays_flush()
(root / 'gimp-visible-review.json').write_text(json.dumps([
    {'name': img.get_name(), 'file': img.get_file().get_path(), 'dirty': img.is_dirty()}
    for img in Gimp.get_images()
], indent=2), encoding='utf-8')
