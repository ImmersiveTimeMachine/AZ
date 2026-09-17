"""Create CHALK HUD study with native editable GIMP layers. No Unreal edits.

Run inside GIMP 3 Python, through MCP execute or the GIMP batch interpreter.
All output lives beside this file's sources directory. v01 is a draft study.
"""
from gi.repository import Gimp, Gio, Gegl
from pathlib import Path
import json
import math
import traceback

ROOT = Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v01')
SRC = ROOT / 'sources'
WHITE = '#EEEAE0'
SOFT = '#B9B9B9'
PEACH = '#FFBA8C'
RED = '#E07768'
DARK = '#101515'
FONT = 'Oswald Light'
BODY = 'Roboto Regular'


def log(message):
    with (SRC / 'build_progress.log').open('a', encoding='utf-8') as f:
        f.write(message + '\n')


def color(value):
    return Gegl.Color.new(value)


def group(img, name, parent=None):
    item = Gimp.GroupLayer.new(img, name)
    img.insert_layer(item, parent, 0)
    return item


def layer(img, parent, name, w=None, h=None, x=0, y=0, opacity=100):
    item = Gimp.Layer.new(img, name, int(w or img.get_width()), int(h or img.get_height()),
                          Gimp.ImageType.RGBA_IMAGE, opacity, Gimp.LayerMode.NORMAL)
    img.insert_layer(item, parent, 0)
    item.fill(Gimp.FillType.TRANSPARENT)
    item.set_offsets(int(x), int(y))
    return item


def fill(img, item, value):
    Gimp.context_set_foreground(color(value))
    item.edit_fill(Gimp.FillType.FOREGROUND)
    Gimp.Selection.none(img)


def rect(img, parent, name, x, y, w, h, value, opacity=100):
    item = layer(img, parent, name, w, h, x, y, opacity)
    Gimp.Selection.none(img)
    fill(img, item, value)
    return item


def text(img, parent, label, x, y, size, value=WHITE, font=FONT, opacity=100, name=None):
    f = Gimp.Font.get_by_name(font)
    if f is None:
        raise RuntimeError('Required font missing: ' + font)
    item = Gimp.TextLayer.new(img, label, f, float(size), Gimp.Unit.pixel())
    img.insert_layer(item, parent, 0)
    item.set_color(color(value))
    item.set_offsets(int(x), int(y))
    item.set_opacity(opacity)
    if name:
        item.set_name(name)
    return item


def polygon(img, item, pts, value):
    img.select_polygon(Gimp.ChannelOps.REPLACE, [float(c) for p in pts for c in p])
    fill(img, item, value)


def line(img, item, x1, y1, x2, y2, width, value):
    dx, dy = x2-x1, y2-y1
    length = max(0.001, math.hypot(dx, dy))
    ax, ay = -dy/length*width/2, dx/length*width/2
    polygon(img, item, [(x1+ax,y1+ay),(x2+ax,y2+ay),(x2-ax,y2-ay),(x1-ax,y1-ay)], value)


def ellipse(img, item, x, y, w, h, value):
    img.select_ellipse(Gimp.ChannelOps.REPLACE, x, y, w, h)
    fill(img, item, value)


def heart(img, parent, x, y, size=20, value=WHITE):
    item = layer(img, parent, 'Health / heart glyph', size, size, x, y)
    polygon(img, item, [(x+size*.5,y+size*.9),(x+size*.05,y+size*.4),
                       (x+size*.08,y+size*.2),(x+size*.27,y+size*.12),
                       (x+size*.5,y+size*.28),(x+size*.73,y+size*.12),
                       (x+size*.92,y+size*.2),(x+size*.95,y+size*.4)], value)
    return item


def imported(img, parent, name, filename, x, y, w=None, h=None, tint=None, opacity=100):
    item = Gimp.file_load_layer(Gimp.RunMode.NONINTERACTIVE, img, Gio.File.new_for_path(str(SRC / filename)))
    img.insert_layer(item, parent, 0)
    item.set_name(name)
    if w is not None:
        item.scale(int(w), int(h or item.get_height()*w/item.get_width()), False)
    item.set_offsets(int(x), int(y))
    item.set_opacity(opacity)
    if tint:
        item.set_lock_alpha(True)
        Gimp.Selection.none(img)
        fill(img, item, tint)
        item.set_lock_alpha(False)
    return item


def bar(img, parent, x, y, width, height, percent, rough=False, value=WHITE):
    mask = 'T_PB_Mask_880x16_H.png' if rough else 'T_Mask4_256x16.png'
    bg = imported(img, parent, 'Health / full track', mask, x,y,width,height,'#717875',52)
    fg = imported(img, parent, 'Health / filled %d percent' % round(percent*100), mask,x,y,width,height,value)
    img.select_rectangle(Gimp.ChannelOps.REPLACE, x+width*percent,y-2,width*(1-percent)+2,height+4)
    fg.edit_clear()
    Gimp.Selection.none(img)
    return bg,fg


def scrim(img, parent, x, y, w, h, opacity=52):
    item = layer(img,parent,'Contrast / soft charcoal scrim',w,h,x,y,opacity)
    img.select_rectangle(Gimp.ChannelOps.REPLACE,x+25,y+25,w-50,h-50)
    Gimp.Selection.feather(img,32)
    fill(img,item,DARK)
    return item


def key(img,parent,label,x,y,size=32):
    rect(img,parent,'Input / '+label+' keycap',x,y,size,size,'#141A1A',80)
    geo = layer(img,parent,'Input / keycap outline',size,size,x,y)
    for a,b,c,d in [(x,y,x+size,y),(x,y,x,y+size),(x+size-1,y,x+size-1,y+size),(x,y+size-1,x+size,y+size-1)]:
        line(img,geo,a,b,c,d,1,SOFT)
    item=text(img,parent,label,0,0,size*.61,WHITE,BODY)
    item.set_offsets(round(x+(size-item.get_width())/2),round(y+(size-item.get_height())/2)-1)


def mag_icon(img,parent,x,y,value=WHITE,scale=1):
    geo=layer(img,parent,'Magazine / silhouette',round(18*scale),round(30*scale),x,y)
    p=[(1,1),(15,1),(15,12),(17,22),(13,28),(3,27),(1,16)]
    polygon(img,geo,[(x+a*scale,y+b*scale) for a,b in p],value)
    for a in (5,9):
        line(img,geo,x+a*scale,y+6*scale,x+(a+1)*scale,y+22*scale,scale,'#303634')
    return geo


def pickup(img,parent):
    p=group(img,'Context / nearby pickup - toggle off in exploration',parent)
    scrim(img,p,1020,605,362,112,64)
    key(img,p,'E',1051,630,34)
    text(img,p,'M16 MAGAZINE',1102,624,25)
    text(img,p,'30 / 30',1103,660,18,SOFT,BODY)
    return p


def footer_hint(img,parent):
    p=group(img,'Context / inventory hint - fades after onboarding',parent)
    key(img,p,'I',96,995,25)
    text(img,p,'INVENTORY',132,994,19,SOFT)


def quiet(img,critical=False):
    title='A2 / QUIET SURVIVAL - critical health' if critical else 'A / QUIET SURVIVAL - recommended'
    p=group(img,title)
    scrim(img,p,1455,820,399,215,48)
    imported(img,p,'Weapon / actual inventory M16 icon','Rifle_PrimaryIcon.png',1507,833,174,87,WHITE,91)
    text(img,p,'M16',1697,855,20,SOFT)
    text(img,p,'17',1513,904,58,WHITE,FONT,name='Ammo / inserted magazine rounds')
    text(img,p,'/ 30',1576,931,25,SOFT,FONT,name='Ammo / magazine capacity')
    mag_icon(img,p,1704,927,WHITE,.75)
    text(img,p,'2',1730,918,34,WHITE,FONT,name='Ammo / two spare magazines')
    text(img,p,'MAGS',1760,931,17,SOFT,FONT)
    heart(img,p,1514,981,18,RED if critical else WHITE)
    bar(img,p,1547,987,241,10,.19 if critical else .72,value=RED if critical else WHITE)
    if critical:
        text(img,p,'LOW HEALTH',1657,1010,19,RED,FONT)
        # Readable word + heart + shorter fill ensure urgency isn't color-only.
    else:
        footer_hint(img,p)
        pickup(img,p)
    return p


def field(img):
    p=group(img,'B / FIELD KIT - inventory family')
    scrim(img,p,62,856,294,185,47)
    geo=layer(img,p,'Health / circular meter',120,120,90,887)
    cx,cy,r=150,947,45
    # Native geometry: a near-full ring with one intentional gap.
    for i in range(112):
        a=math.radians(130+i*2.5)
        b=math.radians(130+(i+.88)*2.5)
        value=WHITE if i<81 else '#59625F'
        line(img,geo,cx+r*math.cos(a),cy+r*math.sin(a),cx+r*math.cos(b),cy+r*math.sin(b),5,value)
    heart(img,p,139,935,22)
    text(img,p,'HEALTH',215,922,22,SOFT)
    text(img,p,'72',215,951,31,WHITE)
    text(img,p,'/ 100',256,963,19,SOFT)
    scrim(img,p,1485,836,365,198,55)
    imported(img,p,'Weapon / inventory rifle','Rifle_PrimaryIcon.png',1558,849,153,77,WHITE)
    text(img,p,'17',1545,924,57)
    text(img,p,'/ 30',1610,950,25,SOFT)
    rect(img,p,'Ammo / separator',1700,930,1,68,'#747D78',65)
    mag_icon(img,p,1726,929,WHITE,.7)
    text(img,p,'30',1750,927,22,WHITE)
    mag_icon(img,p,1726,967,SOFT,.7)
    text(img,p,'17',1750,965,22,SOFT)
    text(img,p,'SPARES',1730,888,18,SOFT)
    pickup(img,p)
    return p


def raw_chalk(img):
    p=group(img,'C / RAW CHALK - stronger material texture')
    scrim(img,p,1395,832,459,209,57)
    imported(img,p,'Weapon / inventory rifle','Rifle_PrimaryIcon.png',1432,865,163,82,WHITE)
    text(img,p,'17',1616,855,80,WHITE,'Oswald Regular')
    text(img,p,'/ 30',1705,900,31,SOFT)
    heart(img,p,1437,968,19)
    bar(img,p,1473,973,306,16,.72,True)
    text(img,p,'2 SPARE MAGAZINES',1621,1010,18,SOFT)
    pickup(img,p)
    footer_hint(img,p)
    return p


def png(img, name):
    proc=Gimp.get_pdb().lookup_procedure('file-png-export')
    cfg=proc.create_config()
    cfg.set_property('run-mode',Gimp.RunMode.NONINTERACTIVE)
    cfg.set_property('image',img)
    cfg.set_property('file',Gio.File.new_for_path(str(ROOT/name)))
    result=proc.run(cfg)
    if result.index(0)!=Gimp.PDBStatusType.SUCCESS:
        raise RuntimeError('PNG export failed '+name+': '+str(result.index(0)))
    log('Exported '+name)


def xcf(img,name):
    proc=Gimp.get_pdb().lookup_procedure('gimp-xcf-save')
    cfg=proc.create_config()
    cfg.set_property('run-mode',Gimp.RunMode.NONINTERACTIVE)
    cfg.set_property('image',img)
    cfg.set_property('file',Gio.File.new_for_path(str(ROOT/name)))
    result=proc.run(cfg)
    if result.index(0)!=Gimp.PDBStatusType.SUCCESS:
        raise RuntimeError('XCF save failed '+name)
    log('Saved '+name)


def choose(options, active):
    for p in options:
        p.set_visible(p == active)


def board_image(filename,img,parent,x,y,w):
    item=Gimp.file_load_layer(Gimp.RunMode.NONINTERACTIVE,img,Gio.File.new_for_path(str(ROOT/filename)))
    img.insert_layer(item,parent,0)
    item.set_name(filename)
    item.scale(w,round(w*1080/1920),False)
    item.set_offsets(x,y)
    return item


def build_board():
    img=Gimp.Image.new(2560,1800,Gimp.ImageBaseType.RGB)
    rect(img,None,'Board / charcoal paper',0,0,2560,1800,'#151C1D')
    text(img,None,'CHALK',80,38,54,WHITE,'Oswald Regular')
    text(img,None,'HUD STUDY / 01',265,62,23,PEACH,BODY)
    text(img,None,'THREE DIRECTIONS  /  EDITABLE GIMP DRAFT  /  1920 x 1080',1490,63,19,SOFT,BODY)
    rect(img,None,'Board / hairline',80,121,2400,1,'#49524F')
    text(img,None,'A   QUIET SURVIVAL',80,151,31)
    text(img,None,'RECOMMENDED',1060,161,18,PEACH,BODY)
    board_image('A_Quiet_Survival.png',img,None,80,211,1160)
    text(img,None,'One compact corner. Health, inserted magazine, spare count.',80,880,22,SOFT,BODY)
    text(img,None,'B   FIELD KIT',1320,151,31)
    board_image('B_Field_Kit.png',img,None,1320,211,1160)
    text(img,None,'Inventory-style health ring. Individual spare-magazine counts.',1320,880,22,SOFT,BODY)
    text(img,None,'C   RAW CHALK',80,967,31)
    board_image('C_Raw_Chalk.png',img,None,80,1027,1160)
    text(img,None,'Rough pack texture, larger ammunition, stronger material identity.',80,1697,22,SOFT,BODY)
    text(img,None,'A   CLOSER LOOK',1320,967,31)
    # Crop a merged screenshot for a genuinely readable enlarged detail.
    detail=Gimp.file_load_layer(Gimp.RunMode.NONINTERACTIVE,img,Gio.File.new_for_path(str(ROOT/'A_Quiet_Survival.png')))
    img.insert_layer(detail,None,0)
    detail.set_name('A / enlarged corner detail (preview only)')
    detail.resize(400,230,-1450,-810)
    detail.scale(800,460,False)
    detail.set_offsets(1370,1008)
    text(img,None,'17 / 30',1320,1482,30,WHITE)
    text(img,None,'Rounds in the inserted magazine / its capacity',1478,1496,22,SOFT,BODY)
    text(img,None,'2 MAGS',1320,1534,30,WHITE)
    text(img,None,'Separate spare magazines; details stay in inventory',1478,1548,22,SOFT,BODY)
    rect(img,None,'Palette / chalk',1320,1613,32,32,WHITE)
    rect(img,None,'Palette / neutral',1364,1613,32,32,SOFT)
    rect(img,None,'Palette / inventory peach',1408,1613,32,32,PEACH)
    rect(img,None,'Palette / critical',1452,1613,32,32,RED)
    text(img,None,'Oswald + Roboto  /  inventory palette',1512,1618,21,SOFT,BODY)
    text(img,None,'Concept scene. Illustrative values. HUD behavior still to be implemented.',1320,1697,20,SOFT,BODY)
    png(img,'CHALK_HUD_Comparison.png')
    xcf(img,'CHALK_HUD_Comparison.xcf')
    return img


def main():
    log('BEGIN')
    Gimp.context_push()
    Gimp.context_set_antialias(True)
    Gimp.context_set_feather(False)
    img=Gimp.Image.new(1920,1080,Gimp.ImageBaseType.RGB)
    img.undo_disable()
    imported(img,None,'BACKGROUND / generated Montreal concept - replace with gameplay capture',
             'montreal_concept_plate.png',0,0,1920,1080)
    c=raw_chalk(img)
    b=field(img)
    a2=quiet(img,True)
    a=quiet(img)
    opts=[a,a2,b,c]
    for option,name in [(a,'A_Quiet_Survival.png'),(a2,'A2_Critical_Health.png'),(b,'B_Field_Kit.png'),(c,'C_Raw_Chalk.png')]:
        choose(opts,option)
        png(img,name)
    choose(opts,a)
    # An overlay export supports later compositing over real gameplay screenshots.
    background=img.get_layers()[-1]
    background.set_visible(False)
    png(img,'A_Quiet_Survival_Overlay.png')
    background.set_visible(True)
    img.undo_enable()
    xcf(img,'CHALK_HUD_v01.xcf')
    log('HUD complete')
    board=build_board()
    manifest={'canvas':[1920,1080], 'font_primary':FONT,'font_secondary':BODY,
              'images':[{'name':x.get_name(),'id':x.get_id()} for x in (img,board)],
              'hud_groups':[{ 'name':p.get_name(),'visible':p.get_visible(),'children':len(p.get_children())} for p in opts],
              'native_text_layers':sum(1 for p in opts for t in p.get_children() if isinstance(t,Gimp.TextLayer))}
    (ROOT/'verification.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    Gimp.context_pop()
    Gimp.displays_flush()
    log('DONE')


try:
    main()
except Exception:
    log(traceback.format_exc())
    raise
