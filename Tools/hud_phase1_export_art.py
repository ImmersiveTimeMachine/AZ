"""Run inside GIMP: render approved HUD vector definitions at import resolution."""
from gi.repository import Gimp, Gio, Gegl
from pathlib import Path
import json
import ast

project=Path('C:/UnrealEngine/Games/AZ')
source_code=project/'UI Design/CHALK_HUD_v03/sources/build_native_hud.py'
definitions=ast.parse(source_code.read_text(encoding='utf-8'))
definitions.body=[node for node in definitions.body if not isinstance(node,ast.Try)]
exec(compile(definitions,str(source_code),'exec'),globals())
out=project/'UI Design/CHALK_HUD_v03/unreal-art'
out.mkdir(parents=True,exist_ok=True)
receipts=[]
for kind,filename,w,h in [('heart','T_HUD_Heart.png',128,128),('mask','T_HUD_HealthMask.png',1024,48)]:
    img=Gimp.Image.new(w,h,Gimp.ImageBaseType.RGB)
    if kind=='heart':
        heart(img,None,6.4,6.4,115.2,'#FFFFFF')
    else:
        # Same full-track contour as the approved native v03 health bar.
        pts=[(0,h*.25),(w*.03,0),(w*.34,h*.08),(w*.7,0),(w,h*.13),
             (w,h*.87),(w*.71,h),(w*.35,h*.92),(0,h)]
        shape(img,None,'Approved full health mask',pts,'#FFFFFF')
    proc=Gimp.get_pdb().lookup_procedure('file-png-export')
    cfg=proc.create_config()
    cfg.set_property('run-mode',Gimp.RunMode.NONINTERACTIVE)
    cfg.set_property('image',img)
    cfg.set_property('file',Gio.File.new_for_path(str(out/filename)))
    result=proc.run(cfg)
    if result.index(0)!=Gimp.PDBStatusType.SUCCESS:
        raise RuntimeError('Export failed '+filename)
    receipts.append({'file':str(out/filename),'width':w,'height':h,'source':str(source_code),'native_vector_render':True})
    img.delete()
(out/'export-receipt.json').write_text(json.dumps(receipts,indent=2),encoding='utf-8')
