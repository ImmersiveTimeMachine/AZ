"""Native editable GIMP layout studies; no Unreal edits or generated bitmap UI."""
from gi.repository import Gimp, Gio, Gegl
from pathlib import Path
import ast
import math
import json

project = Path('C:/UnrealEngine/Games/AZ')
source = project / 'UI Design/CHALK_HUD_v03/sources/build_native_hud.py'
tree = ast.parse(source.read_text(encoding='utf-8'))
tree.body = [n for n in tree.body if not isinstance(n, ast.Try)]
exec(compile(tree, str(source), 'exec'), globals())
ROOT = project / 'UI Design/CHALK_QuestMap_v01'
(ROOT / 'sources').mkdir(parents=True, exist_ok=True)
SAGE = '#B5C8B7'
MUTED = '#929D96'
INK = '#17201E'


def save_image(img, stem):
    path = ROOT / (stem + '_NATIVE.xcf')
    assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, img, Gio.File.new_for_path(str(path)))
    proc = Gimp.get_pdb().lookup_procedure('file-png-export')
    cfg = proc.create_config()
    cfg.set_property('run-mode', Gimp.RunMode.NONINTERACTIVE)
    cfg.set_property('image', img)
    cfg.set_property('file', Gio.File.new_for_path(str(ROOT / (stem + '.png'))))
    assert proc.run(cfg).index(0) == Gimp.PDBStatusType.SUCCESS
    return {'xcf': str(path), 'png': str(ROOT/(stem+'.png')),
            'layers': len(img.get_layers()), 'paths': len(img.get_paths()), 'size': [1920, 1080]}


def thinbox(img, parent, name, x, y, w, h, col=MUTED, alpha=55):
    for i,(a,b,c,d) in enumerate(((x,y,x+w,y),(x+w,y,x+w,y+h),(x+w,y+h,x,y+h),(x,y+h,x,y))):
        line(img,parent,a,b,c,d,1,col,name+' edge '+str(i),alpha)


def symbol(img, parent, kind, x, y, col=PEACH, size=16):
    if kind == 'quest':
        outline_diamond(img,parent,'Quest destination',x,y,size,col)
        rect(img,parent,'Quest centre',x-2,y-2,4,4,col)
    elif kind == 'waypoint':
        for sx,sy in ((-1,-1),(1,-1),(-1,1),(1,1)):
            line(img,parent,x+sx*size,y+sy*size,x+sx*(size-7),y+sy*size,2,col)
            line(img,parent,x+sx*size,y+sy*size,x+sx*size,y+sy*(size-7),2,col)
        ellipse(img,parent,x-3,y-3,6,6,col)
    else:
        shape(img,parent,'Player position',[(x,y-size),(x+size*.7,y+size*.55),(x,y+size*.2),(x-size*.7,y+size*.55)],col)


def district(img, parent, x, y, w, h, paper=False):
    g=group(img,'Map / illustrative district geometry',parent)
    bg='#C4C5B4' if paper else '#29342E'
    block='#B5B8A6' if paper else '#354339'
    edge='#929E89' if paper else '#586657'
    road='#D7D5C4' if paper else '#202A25'
    words='#425048' if paper else '#A9B4A6'
    rect(img,g,'Map base',x,y,w,h,bg)
    # All geometry is native vector and illustrative; map calibration comes
    # from the actual level in the runtime stage, not from this layout study.
    for iy in range(5):
        for ix in range(8):
            bx=x+w*(.055+ix*.117); by=y+h*(.08+iy*.172)
            bw=w*.082; bh=h*.115
            shape(img,g,'Building block %s-%s'%(ix,iy),[(bx,by),(bx+bw,by),(bx+bw,by+bh*.7),(bx+bw*.72,by+bh*.7),(bx+bw*.72,by+bh),(bx,by+bh)],block)
            line(img,g,bx,by,bx+bw,by,1,edge)
            if (ix+iy)%3==0:
                rect(img,g,'Courtyard',bx+bw*.18,by+bh*.17,bw*.4,bh*.28,bg)
    for fx in (.16,.39,.63,.86):
        line(img,g,x+w*fx,y+12,x+w*fx,y+h-12,12,road)
        line(img,g,x+w*fx,y+12,x+w*fx,y+h-12,1,edge,opacity=70)
    for fy in (.225,.565,.91):
        line(img,g,x+12,y+h*fy,x+w-12,y+h*fy,15,road)
        line(img,g,x+12,y+h*fy,x+w-12,y+h*fy,1,edge,opacity=70)
    line(img,g,x+w*.05,y+h*.87,x+w*.92,y+h*.08,25,road,'Main avenue')
    line(img,g,x+w*.05,y+h*.87,x+w*.92,y+h*.08,1.5,edge,'Main avenue centre')
    rect(img,g,'School grounds',x+w*.65,y+h*.30,w*.14,h*.17,'#8F9C83' if paper else '#44523D')
    text(img,g,'ÉCOLE',round(x+w*.668),round(y+h*.315),23,words,'Oswald Light')
    if w < 1700:
        text(img,g,'RUE OUEST',round(x+w*.17),round(y+h*.925),17,words,BODY)
        text(img,g,'SECTEUR 01',round(x+26),round(y+22),19,words,BODY)
    # An approximate objective area uses a broken outline, distinct from pins.
    cx,cy=x+w*.265,y+h*.70;rx,ry=w*.092,h*.11
    area=ellipse(img,g,cx-rx,cy-ry,rx*2,ry*2,PEACH,'Search area / soft fill');area.set_opacity(8)
    for j in range(18):
        a=j*math.tau/18;b=a+math.tau/28
        line(img,g,cx+rx*math.cos(a),cy+ry*math.sin(a),cx+rx*math.cos(b),cy+ry*math.sin(b),2,PEACH if not paper else '#9C664A','Search area dash')
    pins=group(img,'Navigation / semantic symbols',parent)
    symbol(img,pins,'player',x+w*.45,y+h*.66,WHITE if not paper else '#334A40',16)
    symbol(img,pins,'quest',x+w*.70,y+h*.38,PEACH if not paper else '#9C664A',17)
    symbol(img,pins,'waypoint',x+w*.48,y+h*.23,SAGE if not paper else '#3D655A',16)
    if not paper:
        rect(img,pins,'Tracked goal label backing',x+w*.70+24,y+h*.38-17,168,36,INK,94)
        text(img,pins,'MAINTENANCE ROOM',round(x+w*.70+35),round(y+h*.38-12),17,WHITE,FONT)
    ng=group(img,'North and distance scale',parent)
    text(img,ng,'N',round(x+w-46),round(y+22),25,WHITE if not paper else INK,FONT)
    shape(img,ng,'North arrow',[(x+w-39,y+61),(x+w-45,y+78),(x+w-39,y+74),(x+w-33,y+78)],WHITE if not paper else INK)
    line(img,ng,x+w-160,y+h-32,x+w-40,y+h-32,2,words)
    line(img,ng,x+w-160,y+h-38,x+w-160,y+h-26,2,words)
    line(img,ng,x+w-40,y+h-38,x+w-40,y+h-26,2,words)
    text(img,ng,'100 m',round(x+w-115),round(y+h-61),16,words,BODY)
    return g


def frame(img, number, title, subtitle):
    g=group(img,'00 / shared CHALK inventory shell')
    rect(img,g,'Screen backing',0,0,1920,1080,'#101817')
    text(img,g,'CHALK',60,36,46,WHITE,'Oswald Regular')
    for tx,caption,col in [(492,'INVENTORY',MUTED),(704,'MAP',PEACH),(824,'CRAFTING',MUTED)]:
        text(img,g,caption,tx,54,25,col,FONT)
    rect(img,g,'Selected page underline',697,99,75,2,PEACH)
    line(img,g,58,113,1862,113,1,'#4B5851')
    text(img,g,number+' / '+title,60,135,23,PEACH,FONT)
    text(img,g,subtitle,720,140,18,MUTED,BODY)
    line(img,g,58,986,1862,986,1,'#4B5851')
    text(img,g,'SCROLL  Zoom     DRAG  Pan     ENTER  Track     RMB  Personal marker',60,1005,18,WHITE,BODY)
    text(img,g,'ESC  Back',1692,1004,20,WHITE,FONT)
    text(img,g,'LAYOUT STUDY  •  ILLUSTRATIVE MAP AND QUEST CONTENT  •  INPUT LABELS PROVISIONAL',60,1046,12,MUTED,BODY)


def journal(img,x,y,w,h,compact=False):
    g=group(img,'Journal / editable quest text and rows')
    rect(img,g,'Journal backing',x,y,w,h,'#16211E',96)
    text(img,g,'JOURNAL',x+27,y+21,32,WHITE,FONT)
    text(img,g,'STORY',x+29,y+83,16,PEACH,BODY)
    rect(img,g,'Selected quest band',x+14,y+115,w-28,83,'#354038')
    rect(img,g,'Selected quest edge',x+14,y+115,3,83,PEACH)
    text(img,g,'RESTORE THE RADIO',x+30,y+128,28,WHITE,FONT)
    text(img,g,'Tracked  /  1 of 3 steps',x+30,y+166,16,MUTED,BODY)
    text(img,g,'Find the maintenance room',x+30,y+237,22,PEACH,FONT)
    text(img,g,'Bring the spare fuse',x+30,y+283,20,WHITE,FONT)
    text(img,g,'Check the storeroom  (optional)',x+30,y+327,18,MUTED,FONT)
    line(img,g,x+27,y+388,x+w-27,y+388,1,'#4B5851')
    text(img,g,'SIDE QUESTS',x+29,y+417,16,MUTED,BODY)
    text(img,g,'A NEIGHBOUR’S REQUEST',x+30,y+456,24,WHITE,FONT)
    text(img,g,'Available',x+30,y+491,16,MUTED,BODY)
    if not compact:
        text(img,g,'ARCHIVE',x+29,y+572,16,MUTED,BODY)
        text(img,g,'Completed and past tasks',x+30,y+607,19,MUTED,FONT)
    return g


Gimp.context_push()
receipts=[]
try:
    Gimp.context_set_antialias(True)
    for variant in (1,2,3):
        img=Gimp.Image.new(1920,1080,Gimp.ImageBaseType.RGB)
        if variant==1:
            frame(img,'01','FIELD JOURNAL','A persistent journal beside a large, quiet map')
            district(img,None,524,193,1338,752)
            journal(img,58,193,444,752)
        elif variant==2:
            frame(img,'02','MAP FIRST','A wide map with a compact tracked-quest card')
            district(img,None,58,193,1804,752)
            g=group(img,'Tracked quest / floating compact card')
            rect(img,g,'Card backing',89,223,446,218,INK,96)
            text(img,g,'STORY  /  TRACKED',113,248,16,PEACH,BODY)
            text(img,g,'RESTORE THE RADIO',113,282,32,WHITE,FONT)
            text(img,g,'Find the maintenance room',113,334,22,WHITE,FONT)
            text(img,g,'1 / 3 steps    •    OPEN JOURNAL',113,391,16,MUTED,BODY)
            f=group(img,'Filters / compact footer')
            rect(img,f,'Filters backing',89,861,722,52,INK,96)
            text(img,f,'STORY    SIDE QUESTS    KNOWN PLACES    MY MARKER',109,877,16,MUTED,BODY)
        else:
            frame(img,'03','DISTRICT NOTES','A worn civic map paired with a restrained journal')
            district(img,None,58,193,1290,752,True)
            journal(img,1370,193,492,752)
        Gimp.displays_flush()
        receipts.append(save_image(img,'CHALK_QuestMap_%02d'%variant))
        img.delete()
    (ROOT/'sources/mockup-receipt.json').write_text(json.dumps(receipts,indent=2),encoding='utf-8')
finally:
    Gimp.context_pop()
    Gimp.displays_flush()
