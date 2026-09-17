"""Native GIMP art readability comparison, not a simulated gameplay capture."""
from pathlib import Path
import ast,json,math,traceback
from gi.repository import Gimp,Gio
PROJECT=Path('C:/UnrealEngine/Games/AZ')
BASE=PROJECT/'UI Design/CHALK_Throw_Art_v01/sources/build_throw_art.py'
tree=ast.parse(BASE.read_text(encoding='utf-8'))
tree.body=[n for n in tree.body if not isinstance(n,ast.Try)]
exec(compile(tree,str(BASE),'exec'),globals())
ROOT=PROJECT/'UI Design/CHALK_Throw_Art_v02_Sunlight'
(ROOT/'sources').mkdir(parents=True,exist_ok=True)
OUTLINE='#18251D'

def art(img,g,x,y,revised):
    pts=[]
    for i in range(361):
        t=i/360
        pts.append((x+t*450,y+110-390*t*(1-t)+60*t))
    if revised:
        ribbon(img,g,'Charcoal filament keyline',pts,3.4,OUTLINE,80)
        for i in range(25):
            a=i/25;b=a+.31/25
            sample=pts[round(a*360):round(b*360)+1]
            ribbon(img,g,'Charcoal pulse keyline '+str(i),sample,5.5,OUTLINE,80)
    ribbon(img,g,'Sage filament',pts,1.4 if revised else 1.2,SAGE,55 if revised else 31)
    for i in range(25):
        a=i/25;b=a+.31/25
        ribbon(img,g,'Sage pulse '+str(i),pts[round(a*360):round(b*360)+1],3.5 if revised else 3,SAGE,100 if revised else 94)
    cx,cy=x+450,y+170
    if revised:
        shadow=group(img,'Contact / charcoal keyline',g)
        # Vector stroke expansion preserves open centers; no solid target disk.
        for dx,dy in [(-52,-15),(52,-15),(-52,15),(52,15)]:
            sx,sy=(-1 if dx>0 else 1),(-1 if dy>0 else 1)
            h=2.1/2+1
            shape(img,shadow,'Corner keyline',[(cx+dx+sx*a,cy+dy+sy*b) for a,b in
                [(-h,-h),(19,-h),(19,h),(h,h),(h,8),(-h,8)]],OUTLINE,80)
        for a,b in [(.22,math.pi-.22),(math.pi+.22,2*math.pi-.22)]:
            ring_piece(img,shadow,'Ellipse keyline',cx,cy,30,9,a,b,3.3,OUTLINE,80)
        dot=ellipse(img,shadow,cx-6,cy-3.2,12,6.4,OUTLINE,'Center keyline');dot.set_opacity(80)
    contact_parts(img,g,cx,cy,1,ring_alpha=85 if revised else 70)

try:
    img=Gimp.Image.new(1920,980,Gimp.ImageBaseType.RGB);img.undo_disable()
    rect(img,None,'Board / background',0,0,1920,980,'#101715')
    g=group(img,'Quiet Sage / sunlight readability revision')
    label(img,g,'CHALK / QUIET SAGE / DAYLIGHT',55,28,34,WHITE)
    label(img,g,'Same palette and open marker. A charcoal keyline separates the strokes from bright scenery.',56,81,18,'#B8C0BB',True)
    for col,(title,bg) in enumerate([('SUNLIT PAVEMENT','#E4E2DE'),('FOLIAGE','#526449'),('DARK STREET','#17221D')]):
        x=55+col*620
        for row,revised in enumerate([False,True]):
            y=149+row*362
            rect(img,g,title+' / '+str(revised),x,y,590,330,bg)
            label(img,g,title+(' / REVISED' if revised else ' / PREVIOUS'),x+20,y+18,17,
                  '#24332C' if col==0 else WHITE,True)
            art(img,g,x+47,y+76,revised)
    label(img,g,'Color stroke: 3.5px  |  Keyline envelope: 5.5px  |  Filament: 1.4px  |  Depth occlusion retained',55,902,18,WHITE,True)
    label(img,g,'Native art comparison at 1080p reference scale; final game exposure and projection still require a user capture.',55,940,16,'#AAB8B0',True)
    save(img,'QS_Sunlight_Readability.png','file-png-export')
    native_save(img,'QS_Sunlight_Readability_NATIVE')
    img.delete();log('COMPLETE')
except Exception:
    log(traceback.format_exc());raise
