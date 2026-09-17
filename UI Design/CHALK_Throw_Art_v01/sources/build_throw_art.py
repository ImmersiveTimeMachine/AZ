"""Native GIMP production art derived from the approved Quiet Sage03 vectors."""
from pathlib import Path
import ast
import json
import math
import traceback
from gi.repository import Gimp, Gio

PROJECT = Path('C:/UnrealEngine/Games/AZ')
BASE = PROJECT / 'UI Design/CHALK_Throw_v01/sources/build_throw_mockups.py'
tree = ast.parse(BASE.read_text(encoding='utf-8'))
tree.body = [n for n in tree.body if not isinstance(n, ast.Try)]
exec(compile(tree, str(BASE), 'exec'), globals())
ROOT = PROJECT / 'UI Design/CHALK_Throw_Art_v01'
SAGE, WHITE = '#B5C8B7', '#EEEAE0'
for folder in ['sources', 'unreal-art']:
    (ROOT / folder).mkdir(parents=True, exist_ok=True)


def native_save(img, name):
    img.undo_enable()
    save(img, name + '.xcf', 'gimp-xcf-save')
    reopened = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,
                             Gio.File.new_for_path(str(ROOT / (name + '.xcf'))))
    receipt = audit(reopened)
    assert receipt['counts']['vector'] > 0
    assert receipt['path_count'] == receipt['counts']['vector']
    (ROOT / (name + '_layers.json')).write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    reopened.delete()


def contact_parts(img, parent, cx, cy, scale=1, colors=(SAGE, WHITE, WHITE), ring_alpha=70):
    outer = group(img, 'R / four open L corners / sage', parent)
    for i, (dx, dy) in enumerate([(-52, -15), (52, -15), (-52, 15), (52, 15)]):
        sx, sy = (-1 if dx > 0 else 1), (-1 if dy > 0 else 1)
        h = 2.1 / 2
        outline = [(-h,-h),(18,-h),(18,h),(h,h),(h,7),(-h,7)]
        pts = [(cx+(dx+sx*x)*scale,cy+(dy+sy*y)*scale) for x,y in outline]
        shape(img, outer, 'Corner '+str(i)+' / constant-width L', pts, colors[0])
    inner = group(img, 'G / incomplete ellipse / warm white', parent)
    for i, (a, b) in enumerate([(.22, math.pi-.22), (math.pi+.22, 2*math.pi-.22)]):
        ring_piece(img, inner, 'Ellipse arc ' + str(i), cx, cy, 30*scale, 9*scale,
                   a, b, 1.3*scale, colors[1], ring_alpha)
    dot = group(img, 'B / center oval / warm white', parent)
    ellipse(img, dot, cx-5*scale, cy-2.2*scale, 10*scale, 4.4*scale,
            colors[2], 'First contact / center')


def masks():
    img = Gimp.Image.new(1024, 512, Gimp.ImageBaseType.RGB)
    img.undo_disable()
    rect(img, None, 'Black / zero coverage', 0, 0, 1024, 512, '#000000')
    g = group(img, 'PACKED DATA / R corners / G ring / B center')
    contact_parts(img, g, 512, 256, 8, ('#FF0000', '#00FF00', '#0000FF'), 100)
    save(img, 'unreal-art/T_QS_ContactMasks.png', 'file-png-export')
    native_save(img, 'QS_ContactMasks_NATIVE')
    img.delete()

    for kind in ['Blocked', 'BodyContact']:
        img = Gimp.Image.new(256, 256, Gimp.ImageBaseType.RGB)
        img.undo_disable()
        rect(img, None, 'Black / zero coverage', 0, 0, 256, 256, '#000000')
        g = group(img, kind + ' / compact 32px glyph / R coverage')
        if kind == 'Blocked':
            line(img, g, 72, 72, 184, 184, 14, '#FF0000', 'Blocked / diagonal A')
            line(img, g, 72, 184, 184, 72, 14, '#FF0000', 'Blocked / diagonal B')
        else:
            for i,(dx,dy) in enumerate([(-9,-9),(9,-9),(-9,9),(9,9)]):
                sx,sy=(-1 if dx>0 else 1),(-1 if dy>0 else 1)
                ribbon(img,g,'Body contact / corner '+str(i),
                       [(128+(dx+sx*4)*8,128+dy*8),(128+dx*8,128+dy*8),
                        (128+dx*8,128+(dy+sy*4)*8)],12,'#FF0000')
            ellipse(img,g,116,116,24,24,'#0000FF','Body contact / center')
        save(img, 'unreal-art/T_QS_'+kind+'Mask.png', 'file-png-export')
        native_save(img, 'QS_'+kind+'_NATIVE')
        img.delete()


def curve(img, parent, x, y, width, height, tint=SAGE):
    # A readable production swatch, not a replacement ballistic solver.
    p = [(x, y+height*.64), (x+width*.23,y-height*.40),
         (x+width*.76,y-height*.24), (x+width,y+height)]
    def points(a,b,n=120):
        pts=[]
        for i in range(n+1):
            t=a+(b-a)*i/n;u=1-t
            pts.append((sum([u*u*u*p[0][0],3*u*u*t*p[1][0],3*u*t*t*p[2][0],t*t*t*p[3][0]]),
                        sum([u*u*u*p[0][1],3*u*u*t*p[1][1],3*u*t*t*p[2][1],t*t*t*p[3][1]])))
        return pts
    ribbon(img,parent,'Arc / continuous sage filament',points(.0,1),1.2,tint,31)
    for i in range(28):
        a=i/28;b=min(1,a+.31/28)
        ribbon(img,parent,'Arc / short sage pulse '+str(i),points(a,b,5),3,tint,94)
    return x+width,y+height


def board():
    img=Gimp.Image.new(1920,1080,Gimp.ImageBaseType.RGB);img.undo_disable()
    rect(img,None,'Background / charcoal',0,0,1920,1080,'#101715')
    g=group(img,'PRODUCTION ART / editable native vector and text')
    label(img,g,'CHALK / QUIET SAGE',64,39,36,WHITE)
    label(img,g,'Throwable art kit 01  /  approved shapes, production masks and Unreal materials',65,95,18,'#AAB8B0',True)
    for x,bg,heading in [(64,'#1E2B27','DARK SURFACE'),(998,'#A5ABA2','BRIGHT SURFACE')]:
        rect(img,g,heading+' / flat comparison surface',x,161,856,398,bg)
        label(img,g,heading,x+24,181,16,WHITE if x==64 else '#24332C',True)
        ex,ey=curve(img,g,x+91,256,603,213)
        contact_parts(img,g,ex,ey)
        label(img,g,'9.4 m',ex+72,ey-10,17,WHITE,True,'Illustrative range / runtime data')
        label(img,g,'1.2px filament  /  3px accents  /  no bloom',x+24,525,15,
              WHITE if x==64 else '#24332C',True)
    label(img,g,'FIRST CONTACT / 3x DETAIL',67,600,20,WHITE)
    contact_parts(img,g,295,729,3)
    label(img,g,'Sage corners + warm-white broken ellipse and center',67,808,16,'#AAB8B0',True)
    label(img,g,'BLOCKED / BODY CONTACT',720,600,20,WHITE)
    line(img,g,770,697,810,737,4,WHITE,'Blocked glyph / A')
    line(img,g,770,737,810,697,4,WHITE,'Blocked glyph / B')
    label(img,g,'OBSTRUCTED',840,703,20,WHITE)
    for i,(dx,dy) in enumerate([(-18,-18),(18,-18),(-18,18),(18,18)]):
        sx,sy=(-1 if dx>0 else 1),(-1 if dy>0 else 1)
        ribbon(img,g,'Compact contact / '+str(i),[(1100+dx+sx*8,717+dy),(1100+dx,717+dy),
               (1100+dx,717+dy+sy*8)],3,SAGE)
    ellipse(img,g,1097,714,6,6,WHITE,'Body glyph center')
    label(img,g,'No ground disk through a body',720,808,16,'#AAB8B0',True)
    label(img,g,'EQUIPPED / HUD',1321,600,20,WHITE)
    icon=Gimp.file_load_layer(Gimp.RunMode.NONINTERACTIVE,img,
         Gio.File.new_for_path(str(ROOT/'sources/T_FragGrenadeIcon.png')))
    img.insert_layer(icon,g,0);icon.set_name('Existing game grenade icon / reused without repainting')
    scale=70/max(icon.get_width(),icon.get_height());icon.scale(round(icon.get_width()*scale),round(icon.get_height()*scale),False)
    icon.set_offsets(1321,686)
    label(img,g,'FRAG GRENADE',1412,694,27,WHITE)
    label(img,g,'x2',1776,699,22,'#B8C0BB')
    label(img,g,'RELEASE RMB  THROW     LMB  CANCEL',1321,769,14,WHITE,True)
    heart(img,g,1321,809,20,WHITE);bar(img,g,1356,814,452,10,.72,value=WHITE)
    label(img,g,'Count and hints come from gameplay state',1321,859,16,'#AAB8B0',True)
    rect(img,g,'Footer separator',64,935,1792,1,'#3F5146')
    label(img,g,'#B5C8B7  SAGE',64,963,20,SAGE)
    label(img,g,'#EEEAE0  WARM WHITE',357,963,20,WHITE)
    label(img,g,'Art: Codex  /  runtime wiring: Claude  /  gameplay preview remains first contact',807,966,16,'#AAB8B0',True)
    save(img,'QS_Production_Art_Board.png','file-png-export')
    native_save(img,'QS_Production_Art_Board_NATIVE')
    img.delete()


try:
    masks();board()
    (ROOT/'sources/production-receipt.json').write_text(json.dumps({
        'contact_texture':[1024,512], 'contact_quad_reference_pixels':[128,64],
        'contact_visible_reference_pixels':[104,30],
        'mask_channels':{'R':'sage corners','G':'white ellipse; alpha applied in material','B':'white center'},
        'compact_texture':[256,256], 'compact_quad_reference_pixels':[32,32],
        'native_vectors_preserved':True,'palette_srgb':{'sage':SAGE,'warm_white':WHITE}
    },indent=2),encoding='utf-8')
    log('COMPLETE')
except Exception:
    log(traceback.format_exc());raise
