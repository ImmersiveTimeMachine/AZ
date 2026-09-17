"""Three editable CHALK throwable-preview concepts. GIMP-only UI authoring."""
from pathlib import Path
import ast
import json
import math
import traceback
from gi.repository import Gimp, Gio

PROJECT = Path('C:/UnrealEngine/Games/AZ')
BASE = PROJECT / 'UI Design/CHALK_Mode_Indicators_v01/sources/build_mode_mockups.py'
defs = ast.parse(BASE.read_text(encoding='utf-8'))
defs.body = [n for n in defs.body if not isinstance(n, ast.Try)]
exec(compile(defs, str(BASE), 'exec'), globals())
ROOT = PROJECT / 'UI Design/CHALK_Throw_v01'
WHITE = '#EEEAE0'
SOFT = '#B8C0BB'
DARK = '#101715'
CHOICES = [
    ('01', 'CHALK & EMBER', '#EEEAE0', '#FFBA8C',
     'A fine chalk-white arc. A broken peach ring marks first contact.',
     'Closest to the approved CHALK HUD.'),
    ('02', 'AMBER THREAD', '#E8B183', '#E8B183',
     'Warm dashed trajectory. Inward notches frame the impact point.',
     'More visible among foliage and wet urban detail.'),
    ('03', 'QUIET SAGE', '#B5C8B7', '#B5C8B7',
     'Muted sage pulses. Four chalk corners keep the target open.',
     'A cooler, softer alternative with a distinct target silhouette.'),
]
P0, P1, P2, P3 = (806, 371), (910, 130), (1095, 220), (1193, 694)


def sample_curve(t):
    u = 1 - t
    return (u*u*u*P0[0]+3*u*u*t*P1[0]+3*u*t*t*P2[0]+t*t*t*P3[0],
            u*u*u*P0[1]+3*u*u*t*P1[1]+3*u*t*t*P2[1]+t*t*t*P3[1])


def ribbon(img, parent, name, points, thickness, tint, opacity=100):
    left, right = [], []
    for i, (x, y) in enumerate(points):
        a = points[max(0, i-1)]
        b = points[min(len(points)-1, i+1)]
        dx, dy = b[0]-a[0], b[1]-a[1]
        length = max(.001, math.hypot(dx, dy))
        nx, ny = -dy/length*thickness/2, dx/length*thickness/2
        left.append((x+nx, y+ny)); right.append((x-nx, y-ny))
    return shape(img, parent, name, left + right[::-1], tint, opacity)


def ring_piece(img, parent, name, cx, cy, rx, ry, a, b, width, tint, opacity=100):
    points = [(cx+rx*math.cos(a+(b-a)*i/40), cy+ry*math.sin(a+(b-a)*i/40)) for i in range(41)]
    return ribbon(img, parent, name, points, width, tint, opacity)


def marker(img, parent, choice, cx, cy, s=1):
    key, title, arc_tint, target_tint, _, _ = choice
    g = group(img, 'Impact / '+title+' / first contact, not blast radius', parent)
    ellipse(img, g, cx-5*s, cy-2.2*s, 10*s, 4.4*s, WHITE, 'Impact / chalk center')
    if key == '01':
        for i, (a,b) in enumerate(((.13,1.4),(1.73,3.0),(3.28,4.53),(4.87,6.14))):
            ring_piece(img,g,'Impact / broken ring '+str(i),cx,cy,53*s,17*s,a,b,2.5*s,target_tint,94)
        line(img,g,cx,cy-34*s,cx,cy-22*s,1.6*s,WHITE,'Impact / small upright pin',90)
        line(img,g,cx-7*s,cy+24*s,cx+7*s,cy+24*s,1.5*s,WHITE,'Impact / ground reference',72)
    elif key == '02':
        for i,(a,b) in enumerate(((.13,1.42),(1.74,3.01),(3.28,4.55),(4.89,6.13))):
            ring_piece(img,g,'Impact / thin shadow ring '+str(i),cx,cy,53*s,17*s,a,b,1.15*s,target_tint,56)
        for i,(dx,dy) in enumerate(((-62,0),(62,0),(0,-24),(0,24))):
            if dx:
                inward=-1 if dx>0 else 1
                points=[(cx+dx*s,cy-4*s),(cx+(dx+inward*9)*s,cy),(cx+dx*s,cy+4*s)]
            else:
                inward=-1 if dy>0 else 1
                points=[(cx-7*s,cy+dy*s),(cx,cy+(dy+inward*5)*s),(cx+7*s,cy+dy*s)]
            ribbon(img,g,'Impact / inward notch '+str(i),points,2*s,target_tint)
    else:
        for i,(dx,dy) in enumerate(((-52,-15),(52,-15),(-52,15),(52,15))):
            sx=-1 if dx>0 else 1;sy=-1 if dy>0 else 1
            points=[(cx+(dx+sx*18)*s,cy+dy*s),(cx+dx*s,cy+dy*s),(cx+dx*s,cy+(dy+sy*7)*s)]
            ribbon(img,g,'Impact / open ground corner '+str(i),points,2.1*s,target_tint)
        ring_piece(img,g,'Impact / soft incomplete ellipse',cx,cy,30*s,9*s,.22,math.pi-.22,1.3*s,WHITE,70)
        ring_piece(img,g,'Impact / soft incomplete ellipse 2',cx,cy,30*s,9*s,math.pi+.22,math.pi*2-.22,1.3*s,WHITE,70)
    return g


def trajectory(img, parent, choice, ox=0, oy=0, s=1):
    key,title,arc_tint,target_tint,_,_=choice
    g=group(img,'Trajectory / '+title+' / native editable paths',parent)
    def pts(a,b,n=60):
        return [(ox+x*s,oy+y*s) for x,y in [sample_curve(a+(b-a)*i/n) for i in range(n+1)]]
    if key=='01':
        ribbon(img,g,'Arc / charcoal contrast edge',pts(.014,.988),5*s,'#08100D',58)
        ribbon(img,g,'Arc / chalk trajectory',pts(.014,.988),2.1*s,arc_tint,94)
        for i,t in enumerate((.81,.89,.96)):
            x,y=sample_curve(t)
            ellipse(img,g,ox+x*s-2*s,oy+y*s-2*s,4*s,4*s,arc_tint,'Arc / descending bead '+str(i))
    elif key=='02':
        for i in range(21):
            a=.018+i*.046;b=min(.988,a+.027)
            ribbon(img,g,'Arc / dash shadow '+str(i),pts(a,b,5),5.7*s,'#0A100D',52)
            ribbon(img,g,'Arc / amber dash '+str(i),pts(a,b,5),2.7*s,arc_tint,96)
    else:
        ribbon(img,g,'Arc / faint connecting filament',pts(.018,.988),1.2*s,arc_tint,31)
        for i in range(28):
            a=.018+i*.035;b=min(.988,a+.011)
            ribbon(img,g,'Arc / sage pulse '+str(i),pts(a,b,3),3*s,arc_tint,94)
    x,y=P3
    marker(img,g,choice,ox+x*s,oy+y*s,s)
    label(img,g,'9.4 m',ox+(x+73)*s,oy+(y-10)*s,17*s,WHITE,True,'Impact / illustrative distance')
    return g


def stone_icon(img,parent,x,y,s):
    g=group(img,'Stone / native silhouette',parent)
    points=[(2,25),(11,8),(31,2),(49,14),(57,36),(42,51),(18,54),(3,43)]
    shape(img,g,'Stone / rough outer silhouette',[(x+a*s,y+b*s) for a,b in points],WHITE)
    ribbon(img,g,'Stone / fractured face',[(x+12*s,y+13*s),(x+24*s,y+28*s),(x+47*s,y+17*s)],1.4*s,'#39453E',80)
    line(img,g,x+24*s,y+28*s,x+18*s,y+44*s,1.4*s,'#39453E','Stone / lower fracture',80)
    return g


def throw_hud(img,parent,ox=0,oy=0,s=1):
    g=group(img,'HUD / selected throwable / illustrative inventory count',parent)
    # Existing approved bottom-right health and item vocabulary.
    stone_icon(img,g,ox+1514*s,oy+899*s,.8*s)
    label(img,g,'STONE',ox+1584*s,oy+905*s,27*s,WHITE,name='Item / actual display name')
    label(img,g,'x3',ox+1745*s,oy+910*s,22*s,SOFT,name='Item / available count, example only')
    label(img,g,'RMB  THROW     LMB  CANCEL',ox+1514*s,oy+954*s,14*s,WHITE,True,
          'Controls / contextual aim-only hint / proposed cancel binding')
    heart(img,g,ox+1512*s,oy+982*s,20*s,WHITE)
    bar(img,g,ox+1547*s,oy+987*s,240*s,10*s,.72,value=WHITE)
    return g


def scene(img,parent,ox=0,oy=0,s=1):
    item=Gimp.file_load_layer(Gimp.RunMode.NONINTERACTIVE,img,
          Gio.File.new_for_path(str(ROOT/'sources/montreal_throw_plate.png')))
    img.insert_layer(item,parent,0)
    item.set_name('GENERATED SCENE / illustrative stone preparation pose')
    item.scale(round(1920*s),round(1080*s),False)
    item.set_offsets(round(ox),round(oy))
    item.set_lock_content(True)
    return item


def invalid_example(img,parent,choice):
    g=group(img,'80 / BLOCKED example / hidden / alternate state',parent)
    points=[sample_curve(i*.006) for i in range(73)]
    ribbon(img,g,'Blocked / truncated path',points,2.5,'#E6AE82',94)
    x,y=points[-1]
    line(img,g,x-7,y-7,x+7,y+7,2.4,WHITE,'Blocked / crossing mark A')
    line(img,g,x-7,y+7,x+7,y-7,2.4,WHITE,'Blocked / crossing mark B')
    label(img,g,'OBSTRUCTED',x+20,y-13,19,WHITE,name='Blocked / text cue, not color alone')
    g.set_visible(False)
    return g


def build_one(choice):
    key,title,*_=choice
    img=Gimp.Image.new(1920,1080,Gimp.ImageBaseType.RGB);img.undo_disable()
    scene(img,None)
    g=group(img,'20 / AIMING / show this state')
    trajectory(img,g,choice)
    throw_hud(img,None)
    invalid_example(img,None,choice)
    notes=group(img,'90 / REVIEW NOTES / hidden, not gameplay UI')
    label(img,notes,key+' / '+title,60,38,30,WHITE)
    label(img,notes,'Stone example. Trajectory ends at first contact. Counts and distance are illustrative.',
          62,84,17,SOFT,True)
    notes.set_visible(False)
    name='CHALK_Throw_'+key
    save(img,name+'.png','file-png-export')
    counts=save_xcf_verified(img,name+'_NATIVE.xcf')
    img.delete()
    return counts


def build_compare():
    img=Gimp.Image.new(1920,1635,Gimp.ImageBaseType.RGB);img.undo_disable()
    base=group(img,'00 / REVIEW BOARD / not gameplay UI')
    rect(img,base,'Board / charcoal',0,0,1920,1635,'#111817')
    label(img,base,'CHALK / THROW STUDIES',54,35,44,WHITE)
    label(img,base,'One shared action. Three treatments for the trajectory and first-contact marker.',
          56,100,20,SOFT,True)
    label(img,base,'LMB prepare + aim loop  /  RMB throw  /  LMB cancel proposed',56,141,17,SOFT,True)
    for index,choice in enumerate(CHOICES):
        key,title,arc_tint,target_tint,description,note=choice
        y=211+index*460
        row=group(img,key+' / '+title)
        scene(img,row,54,y,0.3958333333)
        trajectory(img,row,choice,54,y,0.3958333333)
        throw_hud(img,row,54,y,0.3958333333)
        label(img,row,key+' / '+title,881,y+10,37,WHITE)
        label(img,row,description,883,y+72,18,SOFT,True)
        label(img,row,note,883,y+103,18,SOFT,True)
        rect(img,row,'Swatch / trajectory',886,y+160,20,20,arc_tint)
        label(img,row,'ARC  '+arc_tint,918,y+159,15,SOFT,True)
        rect(img,row,'Swatch / contact',1160,y+160,20,20,target_tint)
        label(img,row,'CONTACT  '+target_tint,1192,y+159,15,SOFT,True)
        label(img,row,'IMPACT MARKER / DETAIL',885,y+224,15,SOFT,True)
        marker(img,row,choice,1183,y+312,2.25)
        label(img,row,'Open center keeps the surface visible.',1424,y+274,16,SOFT,True)
        label(img,row,'No glow or solid painted target disk.',1424,y+300,16,SOFT,True)
        label(img,row,'Shape carries state as well as color.',1424,y+326,16,SOFT,True)
        if index<2:line(img,row,54,y+444,1858,y+444,1,'#34413C','Review / row divider')
    save(img,'CHALK_Throw_Comparison.png','file-png-export')
    counts=save_xcf_verified(img,'CHALK_Throw_Comparison_NATIVE.xcf')
    img.delete()
    return counts


def main():
    Gimp.context_push();Gimp.context_set_antialias(True);Gimp.context_set_feather(False)
    receipt={}
    for choice in CHOICES:receipt[choice[0]]=build_one(choice)
    receipt['comparison']=build_compare()
    (ROOT/'saved-file-receipt.json').write_text(json.dumps(receipt,indent=2))
    Gimp.context_pop()
    print(json.dumps(receipt));log('DONE')


try:
    main()
except Exception:
    log(traceback.format_exc());raise
