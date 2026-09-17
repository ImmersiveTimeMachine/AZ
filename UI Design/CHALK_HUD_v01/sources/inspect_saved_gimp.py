from gi.repository import Gimp, Gio
from pathlib import Path
import json

root=Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v01')
result=[]
for filename in ('CHALK_HUD_v01.xcf','CHALK_HUD_Comparison.xcf'):
    img=Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,Gio.File.new_for_path(str(root/filename)))
    def nodes(items):
        out=[]
        for item in items:
            row={'name':item.get_name(),'visible':item.get_visible(),'text':isinstance(item,Gimp.TextLayer)}
            if row['text']:
                row['content']=item.get_text()
                row['font']=item.get_font().get_name()
            if item.is_group():
                row['children']=nodes(item.get_children())
            out.append(row)
        return out
    result.append({'file':filename,'width':img.get_width(),'height':img.get_height(),'layers':nodes(img.get_layers())})
    img.delete()
(root/'saved_file_readback.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
