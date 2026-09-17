"""Native GIMP revision: approved A + contextual quick access, without Unreal changes."""
from pathlib import Path
import ast

PREVIOUS = Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v01')
helper_tree = ast.parse((PREVIOUS/'sources/build_hud_gimp.py').read_text(encoding='utf-8'))
# Load the previous draft's drawing helpers without running its original render job.
helper_tree.body = [n for n in helper_tree.body if not isinstance(n, ast.Try)]
exec(compile(helper_tree, str(PREVIOUS/'sources/build_hud_gimp.py'), 'exec'), globals())
ROOT = Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v02')
SRC = PREVIOUS/'sources'


def log(msg):
    with (ROOT/'sources/build_progress.log').open('a',encoding='utf-8') as stream:
        stream.write(msg+'\n')


def center_label(img,parent,label,cx,y,size,value=WHITE,font=FONT):
    item=text(img,parent,label,0,y,size,value,font)
    item.set_offsets(round(cx-item.get_width()/2),y)
    return item


def border(img,parent,x,y,w,h,value,thickness=1):
    item=layer(img,parent,'Card / fine border',w,h,x,y)
    for x1,y1,x2,y2 in [(x,y,x+w,y),(x,y,x,y+h),(x+w-1,y,x+w-1,y+h),(x,y+h-1,x+w,y+h-1)]:
        line(img,item,x1,y1,x2,y2,thickness,value)


def medkit(img,parent,x,y,value=WHITE):
    icon=layer(img,parent,'Care / med kit silhouette',66,52,x,y)
    polygon(img,icon,[(x+4,y+16),(x+60,y+16),(x+64,y+20),(x+62,y+48),(x+3,y+48),(x+1,y+20)],value)
    for a,b,c,d in [(x+24,y+7,x+42,y+7),(x+24,y+7,x+24,y+15),(x+42,y+7,x+42,y+15)]:
        line(img,icon,a,b,c,d,3,value)
    polygon(img,icon,[(x+29,y+22),(x+37,y+22),(x+37,y+28),(x+44,y+28),(x+44,y+36),(x+37,y+36),(x+37,y+43),(x+29,y+43),(x+29,y+36),(x+22,y+36),(x+22,y+28),(x+29,y+28)],'#28312F')


def pistol(img,parent,x,y,value=WHITE):
    icon=layer(img,parent,'Sidearm / placeholder silhouette',91,57,x,y)
    polygon(img,icon,[(x+5,y+10),(x+81,y+10),(x+85,y+13),(x+85,y+24),(x+39,y+24),(x+36,y+34),(x+28,y+35),(x+24,y+53),(x+9,y+52),(x+18,y+28),(x+8,y+26)],value)
    polygon(img,icon,[(x+33,y+25),(x+43,y+25),(x+41,y+31),(x+33,y+31)],'#202A27')
    line(img,icon,x+76,y+6,x+80,y+6,4,value)


def smoke(img,parent,x,y,value=WHITE):
    icon=layer(img,parent,'Utility / smoke canister placeholder',46,69,x,y)
    polygon(img,icon,[(x+12,y+17),(x+33,y+17),(x+37,y+22),(x+36,y+61),(x+31,y+66),(x+13,y+66),(x+9,y+61),(x+9,y+24)],value)
    line(img,icon,x+17,y+12,x+29,y+12,5,value)
    line(img,icon,x+29,y+9,x+39,y+20,4,value)
    polygon(img,icon,[(x+10,y+36),(x+36,y+36),(x+36,y+44),(x+10,y+44)],'#28312F')


def dots(img,parent,x,y,active=0):
    item=layer(img,parent,'Category / two stable slots',21,6,x,y)
    for i in range(2):
        ellipse(img,item,x+i*13,y,4,4,PEACH if i==active else '#68736D')


def tile(img,parent,name,x,y,selected=False):
    g=group(img,name,parent)
    rect(img,g,'Card / charcoal',x,y,168,128,'#16201D',93 if selected else 84)
    border(img,g,x,y,168,128,PEACH if selected else '#64706A',2 if selected else 1)
    if selected:
        rect(img,g,'Focus / peach leading edge',x,y,4,128,PEACH)
    return g


def quick_selector(img):
    g=group(img,'02 / QUICK SELECT - shown only while invoked')
    # The backdrop treatment is a modal state, not permanent HUD decoration.
    rect(img,g,'Modal / subtle scene dim',0,0,1920,1080,'#07100E',24)
    center_label(img,g,'QUICK SELECT',1118,247,23,SOFT)
    # Stable category directions, one currently focused item per category.
    t=tile(img,g,'UP / CARE - selected med kit',1034,326,True)
    text(img,t,'CARE',1048,335,16,PEACH)
    dots(img,t,1166,344)
    medkit(img,t,1081,362,WHITE)
    text(img,t,'2',1171,406,27,WHITE)
    t=tile(img,g,'LEFT / LONG GUN',846,474)
    text(img,t,'LONG GUN',860,483,16,SOFT)
    imported(img,t,'M16 / existing inventory icon','Rifle_PrimaryIcon.png',857,508,145,72,WHITE)
    text(img,t,'17 / 30',944,570,22,WHITE)
    t=tile(img,g,'RIGHT / SIDEARM - illustrative item',1222,474)
    text(img,t,'SIDEARM',1236,483,16,SOFT)
    pistol(img,t,1261,515)
    text(img,t,'6 / 12',1334,570,22,WHITE)
    t=tile(img,g,'DOWN / UTILITY - carried crafted item',1034,622)
    text(img,t,'UTILITY',1048,631,16,SOFT)
    dots(img,t,1166,640)
    smoke(img,t,1095,657)
    text(img,t,'1',1171,705,27,WHITE)
    # Directional marks stay quiet; focus is communicated by border and name.
    geo=layer(img,g,'Navigation / four direction marks',68,68,1084,504)
    for pts in [[(1118,506),(1114,512),(1122,512)],[(1118,570),(1114,564),(1122,564)],[(1086,538),(1092,534),(1092,542)],[(1150,538),(1144,534),(1144,542)]]:
        polygon(img,geo,pts,'#8E9992')
    center_label(img,g,'MED KIT',1118,779,34,WHITE)
    center_label(img,g,'Recover health over time',1118,825,20,SOFT,BODY)
    center_label(img,g,'Release to equip  /  Cancel to close',1118,878,18,SOFT,BODY)
    return g


def create_board():
    board=Gimp.Image.new(2560,1680,Gimp.ImageBaseType.RGB)
    rect(board,None,'Board / charcoal',0,0,2560,1680,'#151C1D')
    text(board,None,'CHALK',80,38,54,WHITE,'Oswald Regular')
    text(board,None,'HUD STUDY / 02',265,62,23,PEACH,BODY)
    text(board,None,'A REFINED  /  CONTEXTUAL QUICK ACCESS',1800,63,20,SOFT,BODY)
    rect(board,None,'Board / divider',80,121,2400,1,'#49524F')
    text(board,None,'01   NORMAL PLAY',80,151,31)
    text(board,None,'02   QUICK SELECT OPEN',1320,151,31)
    board_image('01_Normal_Play.png',board,None,80,211,1160)
    board_image('02_Quick_Select.png',board,None,1320,211,1160)
    text(board,None,'Health + current weapon. No permanent I hint.',80,888,24,SOFT,BODY)
    text(board,None,'Weapons and supplies appear only when requested.',1320,888,24,SOFT,BODY)
    rect(board,None,'Board / second divider',80,964,2400,1,'#49524F')
    text(board,None,'THE SAME QUIET HUD. MORE DEPTH BEHIND IT.',80,1007,37,WHITE)
    cols=[(80,'01 / PLAY','Health and equipped weapon.','Context prompts when needed.','Temporary feedback for an active effect.'),
          (900,'02 / QUICK ACCESS','Up: care. Down: utility.','Left: long gun. Right: sidearm.','Choose first; use with a separate action.'),
          (1720,'03 / INVENTORY + CRAFTING','Recipes, ingredients and item details.','Equipment stats and progression.','Crafted items return to their useful category.')]
    for x,heading,a,b,c in cols:
        text(board,None,heading,x,1090,29,PEACH)
        for j,label in enumerate((a,b,c)):
            text(board,None,label,x,1150+j*45,22,SOFT,BODY)
    text(board,None,'WHY THIS FITS CHALK',80,1363,26,WHITE)
    text(board,None,'Civilian supplies, chalk-white symbols, restrained inventory typography, and room to watch the world.',80,1420,25,SOFT,BODY)
    text(board,None,'Mockup only. Item types, effects and counts are illustrative. Final input bindings and crafting rules remain to be designed.',80,1576,20,SOFT,BODY)
    png(board,'CHALK_HUD_v02_Review.png')
    xcf(board,'CHALK_HUD_v02_Review.xcf')


def revise():
    log('BEGIN V02')
    Gimp.context_push()
    Gimp.context_set_antialias(True)
    Gimp.context_set_feather(False)
    img=Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,Gio.File.new_for_path(str(PREVIOUS/'CHALK_HUD_v01.xcf')))
    img.undo_disable()
    for item in list(img.get_layers()):
        if item.is_group() and not item.get_name().startswith('A /'):
            img.remove_layer(item)
    a=next(item for item in img.get_layers() if item.is_group())
    a.set_name('01 / NORMAL PLAY - A refined')
    for item in a.get_children():
        if item.get_name().startswith('Context /'):
            item.set_visible(False)
    q=quick_selector(img)
    q.set_visible(False)
    png(img,'01_Normal_Play.png')
    q.set_visible(True)
    a.set_opacity(42)
    png(img,'02_Quick_Select.png')
    # A normal-play document is the starting view; quick-select is a toggleable group.
    q.set_visible(False)
    a.set_opacity(100)
    img.undo_enable()
    xcf(img,'CHALK_HUD_v02.xcf')
    q.set_visible(True)
    a.set_opacity(42)
    xcf(img,'CHALK_Quick_Select_v02.xcf')
    create_board()
    Gimp.context_pop()
    log('DONE V02')


try:
    revise()
except Exception:
    log(traceback.format_exc())
    raise
