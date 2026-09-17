from gi.repository import Gimp, Gio
from pathlib import Path
import json
root=Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_v01')
for name in ['CHALK_Throw_01_NATIVE.xcf','CHALK_Throw_02_NATIVE.xcf','CHALK_Throw_03_NATIVE.xcf','CHALK_Throw_Comparison_NATIVE.xcf']:
    img=Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,Gio.File.new_for_path(str(root/name)))
    Gimp.Display.new(img)
Gimp.displays_flush()
(root/'gimp-visible-review.json').write_text(json.dumps([
    {'name':img.get_name(),'file':img.get_file().get_path(),'dirty':img.is_dirty()}
    for img in Gimp.get_images()],indent=2))
