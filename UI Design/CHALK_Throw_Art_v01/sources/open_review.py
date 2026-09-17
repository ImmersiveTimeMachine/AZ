from pathlib import Path
import json
from gi.repository import Gimp,Gio
root=Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v01')
img=Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,Gio.File.new_for_path(str(root/'QS_Production_Art_Board_NATIVE.xcf')))
Gimp.Display.new(img)
Gimp.displays_flush()
(root/'gimp-review-open.json').write_text(json.dumps({'file':img.get_file().get_path(),'name':img.get_name(),'dirty':img.is_dirty()},indent=2),encoding='utf-8')
