"""Three CHALK visual systems, drawn as native editable GIMP text/paths.
Mockups only. No Unreal content or gameplay changes. Existing approved layouts
are the reference; pack menu pages are proposals, not runtime screenshots.
"""
from gi.repository import Gimp, Gio, Gegl
from pathlib import Path
import ast, json, math, traceback

PROJECT = Path('C:/UnrealEngine/Games/AZ')
base = PROJECT / 'UI Design/CHALK_HUD_v03/sources/build_native_hud.py'
tree = ast.parse(base.read_text(encoding='utf-8'))
tree.body = [n for n in tree.body if not isinstance(n, ast.Try)]
exec(compile(tree, str(base), 'exec'), globals())
ROOT = PROJECT / 'UI Design/CHALK_UnifiedUI_v01'
(ROOT / 'sources').mkdir(parents=True, exist_ok=True)
SCENE = PROJECT / 'UI Design/CHALK_HUD_v01/sources/montreal_concept_plate.png'
PORTRAIT = ROOT / 'sources/hero_inventory_current.png'
ART = PROJECT / 'UI Design/CHALK_HUD_v03/unreal-art'
ICONS = {
    'rifle': PROJECT/'UI Design/CHALK_HUD_v01/sources/Rifle_PrimaryIcon.png',
    'pistol': ART/'T_HUD_Pistol.png', 'magazine': ART/'T_HUD_Magazine.png',
    'explore': ART/'T_HUD_Explore.png', 'fight': ART/'T_HUD_Fists.png',
    'grenade': PROJECT/'UI Design/CHALK_Throw_Art_v01/sources/T_FragGrenadeIcon.png',
}
THEMES = [
 dict(id='01',name='QUIET SIGNALS',ru='Тихие сигналы',bg='#121817',panel='#1B2421',inset='#25312B',text='#EEEAE0',muted='#A7B0A7',edge='#5F6B61',story='#E2AE8A',side='#A9BECC',personal='#B5C8A4',danger='#DB8E80',head='Oswald Light',body='Roboto Regular',bold='Roboto Bold',paper=False,rail=False,desc='Ближе к текущему HUD. Тонкие рамки, спокойные акценты.'),
 dict(id='02',name='FIELD NOTES',ru='Полевые записи',bg='#DDD6C4',panel='#EAE3D1',inset='#C9C4B3',text='#282E29',muted='#596355',edge='#949B88',story='#8E573E',side='#466477',personal='#526D48',danger='#913F34',head='Roboto Bold',body='Roboto Regular',bold='Roboto Bold',paper=True,rail=False,desc='Светлые меню и тёмный HUD. Бумага, поля и ясная иерархия.'),
 dict(id='03',name='DISTRICT SIGNS',ru='Городские ориентиры',bg='#101A1F',panel='#1A2B31',inset='#293D44',text='#F1EDE1',muted='#ACBDC0',edge='#637F86',story='#EDB586',side='#B6D7E3',personal='#B6CDA0',danger='#EC9A88',head='Oswald Medium',body='Roboto Regular',bold='Roboto Bold',paper=False,rail=True,desc='Плотные панели, сильные направляющие, чёткие индикаторы.'),
]
REPORT=[]


def png_export(img,path):
    p=Gimp.get_pdb().lookup_procedure('file-png-export');c=p.create_config()
    c.set_property('run-mode',Gimp.RunMode.NONINTERACTIVE);c.set_property('image',img)
    c.set_property('file',Gio.File.new_for_path(str(path)))
    assert p.run(c).index(0)==Gimp.PDBStatusType.SUCCESS


def counts(img):
    out={'text':0,'vector':0,'groups':0,'raster':0}
    def visit(layers):
        for item in layers:
            if isinstance(item,Gimp.GroupLayer):out['groups']+=1;visit(item.get_children())
            elif isinstance(item,Gimp.TextLayer):out['text']+=1
            elif isinstance(item,Gimp.VectorLayer):out['vector']+=1
            else:out['raster']+=1
    visit(img.get_layers());out['paths']=len(img.get_paths());return out


class Draw:
    def __init__(self,img,parent,t,x=0,y=0,s=1):
        self.img,self.p,self.t,self.x,self.y,self.s=img,parent,t,x,y,s
    def xy(self,x,y):return self.x+x*self.s,self.y+y*self.s
    def rect(self,name,x,y,w,h,c=None,a=100):
        return rect(self.img,self.p,name,*self.xy(x,y),w*self.s,h*self.s,c or self.t['panel'],a)
    def line(self,name,x,y,x2,y2,c=None,width=1,a=100):
        return line(self.img,self.p,*self.xy(x,y),*self.xy(x2,y2),width*self.s,c or self.t['edge'],name,a)
    def poly(self,name,pts,c,a=100):
        return shape(self.img,self.p,name,[self.xy(*p) for p in pts],c,a)
    def text(self,label,x,y,size=22,c=None,font=None,name=None):
        return text(self.img,self.p,label,*self.xy(x,y),size*self.s,c or self.t['text'],font or self.t['body'],name=name)
    def right(self,label,x,y,size=22,c=None,font=None):
        a=self.text(label,0,y,size,c,font);a.set_offsets(round(self.x+x*self.s-a.get_width()),round(self.y+y*self.s));return a
    def center(self,label,x,y,size=22,c=None,font=None):
        a=self.text(label,0,y,size,c,font);a.set_offsets(round(self.x+x*self.s-a.get_width()/2),round(self.y+y*self.s));return a
    def ellipse(self,name,x,y,w,h,c):
        return ellipse(self.img,self.p,*self.xy(x,y),w*self.s,h*self.s,c,name)
    def outline(self,name,x,y,w,h,c=None,width=1):
        for k,(x1,y1,x2,y2) in enumerate([(x,y,x+w,y),(x+w,y,x+w,y+h),(x+w,y+h,x,y+h),(x,y+h,x,y)]):self.line(name+str(k),x1,y1,x2,y2,c,width)
    def image(self,path,name,x,y,w,h=None,tint=None,opacity=100):
        a=Gimp.file_load_layer(Gimp.RunMode.NONINTERACTIVE,self.img,Gio.File.new_for_path(str(path)))
        self.img.insert_layer(a,self.p,0);a.set_name(name)
        ow,oh=a.get_width(),a.get_height();hh=h or w*oh/ow
        a.scale(max(1,round(w*self.s)),max(1,round(hh*self.s)),False)
        a.set_offsets(*[round(v) for v in self.xy(x,y)]);a.set_opacity(opacity)
        if tint:
            a.set_lock_alpha(True);Gimp.Selection.none(self.img);fill(self.img,a,tint);a.set_lock_alpha(False)
        return a
    def icon(self,kind,x,y,w,h=None,c=None):
        return self.image(ICONS[kind],kind+' / existing project artwork',x,y,w,h,c or self.t['text'])
    def panel(self,name,x,y,w,h,selected=False):
        self.rect(name,x,y,w,h,self.t['inset'] if selected else self.t['panel'])
        if self.t['rail']:
            self.rect(name+' rail',x,y,5,h,self.t['text'] if selected else self.t['edge'])
        elif self.t['paper']:
            self.line(name+' bottom rule',x,y+h,x+w,y+h,width=2 if selected else 1)
            if selected:self.rect(name+' margin',x,y,4,h,self.t['text'])
        else:
            self.outline(name+' hairline',x,y,w,h,self.t['text'] if selected else self.t['edge'],2 if selected else 1)
    def role(self,kind,x,y,r=13,tracked=False):
        c=self.t[kind];lw=2.5 if tracked else 1.8
        if kind=='story':
            for radius in (r,r*.57):
                p=[(x,y-radius),(x+radius,y),(x,y+radius),(x-radius,y),(x,y-radius)]
                for i in range(4):self.line('Story / double diamond',*p[i],*p[i+1],c,lw)
        elif kind=='side':
            pts=[(x+math.cos(a)*r,y+math.sin(a)*r) for a in [i*math.tau/24 for i in range(25)]]
            for i in range(24):self.line('Side / open circle',*pts[i],*pts[i+1],c,lw)
            self.rect('Side centre',x-2,y-2,4,4,c)
        else:
            for sx,sy in ((-1,-1),(1,-1),(-1,1),(1,1)):
                self.line('Personal / open corner',x+sx*r,y+sy*r,x+sx*(r-6),y+sy*r,c,lw)
                self.line('Personal / open corner',x+sx*r,y+sy*r,x+sx*r,y+sy*(r-6),c,lw)
        if tracked:self.line('Tracked / neutral tick',x+r+6,y-5,x+r+6,y+5,self.t['text'],3)
    def check(self,x,y,done=True):
        c=self.t['text'] if done else self.t['danger']
        if done:
            self.line('Complete / check',x,y+7,x+5,y+12,c,2);self.line('Complete / check',x+5,y+12,x+15,y,c,2)
        else:
            self.line('Failed / cross',x,y,x+12,y+12,c,2);self.line('Failed / cross',x,y+12,x+12,y,c,2)
    def meter(self,x,y,w,value=.72,kind='health'):
        self.rect(kind+' track',x,y,w,7,self.t['edge'],60)
        if value > 0:self.rect(kind+' value',x,y,w*value,7,self.t['text'])
        if self.t['rail']:
            for i in range(1,8):self.rect(kind+' separator',x+w*i/8,y,2,7,self.t['panel'])
    def key(self,label,x,y):
        self.outline('Key '+label,x,y,26,25,self.t['muted']);self.center(label,x+13,y+3,14,self.t['text'],self.t['bold'])


def backdrop(d,scene=False):
    d.rect('Screen ground',0,0,1920,1080,d.t['bg'])
    if scene:
        d.image(SCENE,'Existing approved Montreal concept plate',0,0,1920,1080)
        d.rect('Scene contrast',0,0,1920,1080,'#07100F',15 if not d.t['rail'] else 23)


def header(d,title,subtitle=None):
    d.text('CHALK',56,32,45,d.t['text'],d.t['head'])
    d.text(title,510,46,29,d.t['text'],d.t['head'])
    d.right('LOCAL CAMPAIGN',1860,53,15,d.t['muted'])
    d.line('Header rule',56,105,1864,105,width=3 if d.t['rail'] else 1)
    if subtitle:d.text(subtitle,56,124,17,d.t['muted'])


def footer(d,hint='ESC  Back'):
    d.line('Footer rule',56,1014,1864,1014)
    d.text(hint,58,1032,18,d.t['muted'])


def miniitem(d,kind,x,y,w,h):
    if kind in ICONS:
        ratio={'rifle':.5,'pistol':.75,'magazine':120/72,'grenade':1}.get(kind,1)
        iw=min(w*.80,h*.75/ratio);ih=iw*ratio
        d.icon(kind,x+(w-iw)/2,y+(h-ih)/2,iw,ih)
    elif kind=='bandage':
        d.outline('Bandage pack',x+w*.25,y+h*.30,w*.5,h*.4,d.t['text'],2)
        d.line('Bandage wrap',x+w*.45,y+h*.3,x+w*.55,y+h*.7,d.t['text'],4)
    elif kind=='bottle':
        d.rect('Bottle',x+w*.32,y+h*.26,w*.36,h*.6,d.t['text']);d.rect('Bottle neck',x+w*.41,y+h*.14,w*.18,h*.2,d.t['text'])
        d.rect('Bottle label',x+w*.32,y+h*.48,w*.36,h*.18,d.t['panel'])
    else:
        d.outline('Cloth / component',x+w*.25,y+h*.25,w*.5,h*.5,d.t['text'],2)
        d.line('Cloth fold',x+w*.25,y+h*.25,x+w*.75,y+h*.75,d.t['text'],2)


def inventory(d,popup=False):
    backdrop(d);header(d,'Inventory' if d.t['paper'] else 'INVENTORY')
    d.panel('Character and vitals',56,150,405,810)
    d.text('PERSONAL CONDITION',80,174,21,d.t['muted'],d.t['head'])
    d.image(PORTRAIT,'Current inventory portrait / source retained',95,206,316,444)
    d.text('HEALTH',84,671,20);d.right('72 / 100',435,671,20)
    d.meter(84,707,350,.72)
    d.text('INFECTION',84,751,20);d.right('0%',435,751,20)
    d.meter(84,788,350,0,'Infection')
    d.text('MORTALITY',84,826,20);d.right('LOW',435,826,20)
    p=[(84,906),(147,906),(162,897),(174,922),(187,875),(203,910),(222,906),(300,906),(325,901),(341,906),(434,906)]
    for i in range(len(p)-1):d.line('Vitals / ECG',*p[i],*p[i+1],d.t['muted'],1.7)
    d.text('BACKPACK',510,156,34,d.t['text'],d.t['head']);d.right('124  CAD',1350,169,22,d.t['muted'])
    tabs=['EQUIPPABLES','CONSUMABLES','CRAFTABLES','MAP']
    for i,label in enumerate(tabs):
        x=510+i*214;d.text(label,x+10,229,18,d.t['text'] if i==0 else d.t['muted'],d.t['head'])
        if i==0:d.rect('Active category',x,263,170,3,d.t['text'])
    gx,gy,cell=528,300,80
    d.rect('Backpack / ten by five grid',gx,gy,800,400,d.t['panel'])
    for i in range(11):d.line('Grid column',gx+i*cell,gy,gx+i*cell,gy+400,width=.8,a=60)
    for j in range(6):d.line('Grid row',gx,gy+j*cell,gx+800,gy+j*cell,width=.8,a=60)
    # Keep the selected category truthful: care/recipe materials belong to the
    # other existing tabs, not to the Equippables grid shown in this study.
    items=[('rifle',0,0,4,2,'17/30'),('pistol',5,0,2,2,'12/15'),('magazine',8,0,1,1,'30'),('magazine',9,0,1,1,'12'),('grenade',8,2,1,1,'2')]
    for n,(kind,col,row,cw,ch,qty) in enumerate(items):
        x,y=gx+col*cell,gy+row*cell
        if n==0:d.panel('Focused M16',x+2,y+2,cw*cell-4,ch*cell-4,True)
        miniitem(d,kind,x,y,cw*cell,ch*cell)
        d.right(qty,x+cw*cell-9,y+ch*cell-25,16)
    d.text('M16',529,740,34,d.t['text'],d.t['head']);d.text('5.56 mm  /  Detachable magazine',529,787,19,d.t['muted'])
    d.text('17 rounds loaded. Compatible magazines stay in the backpack.',529,828,18)
    d.text('WEIGHT',529,887,16,d.t['muted']);d.text('3.4 kg',625,882,23)
    d.text('FIRE MODE',862,887,16,d.t['muted']);d.text('AUTO',982,882,23)
    d.panel('Skills and details',1400,150,464,810)
    d.text('CAPABILITIES',1427,176,28,d.t['text'],d.t['head'])
    for i,(label,val) in enumerate([('Strength','4'),('Agility','6'),('Resilience','5'),('Expertise','3')]):
        yy=264+i*115;d.text(label,1427,yy,23);d.right(val,1827,yy-8,34,d.t['text'],d.t['head']);d.meter(1427,yy+47,399,int(val)/10,label)
    d.text('Upgrade available',1427,753,20,d.t['story']);d.line('Skills detail rule',1427,798,1830,798)
    d.text('Carry weight',1427,833,20);d.right('18.2 / 30 kg',1827,833,20)
    d.text('Melee resistance',1427,886,20);d.right('+ 10%',1827,886,20)
    footer(d,'RMB  Item actions     DRAG  Move item                                            ESC  Back')
    if popup:
        d.rect('Popup dim',0,0,1920,1080,'#050807',42)
        d.panel('Item actions popup',720,280,490,497)
        d.text('M16',754,310,42,d.t['text'],d.t['head']);d.text('Item actions',754,366,18,d.t['muted'])
        for i,label in enumerate(['Equip','Load magazine','Split quantity','Drop']):
            yy=431+i*68
            if i==0:d.panel('Focused item action',743,yy-9,443,54,True)
            d.text(label,768,yy,23,c=d.t['muted'] if i==2 else None)
        d.text('Some actions depend on the selected item.',754,735,16,d.t['muted'])


def drawmap(d):
    backdrop(d);header(d,'Map & journal' if d.t['paper'] else 'MAP / JOURNAL')
    d.panel('Journal',56,153,447,787)
    d.text('JOURNAL',84,177,30,d.t['text'],d.t['head'])
    d.role('story',98,259,12,True);d.text('STORY',130,246,18,d.t['story'])
    d.panel('Tracked quest',76,297,406,91,True)
    d.text('CHECK THE ROUTE',97,309,27,d.t['text'],d.t['head']);d.text('Tracked  /  1 of 3 objectives',97,354,17,d.t['muted'])
    d.check(96,433);d.text('Find the route',125,430,20,d.t['muted'])
    d.role('story',103,493,8);d.text('Reach the marked courtyard',125,478,19,d.t['story'])
    d.text('Inspect the service panel',125,537,19)
    d.text('Check the side entrance  ·  optional',96,593,17,d.t['muted'])
    d.line('Journal section',83,658,477,658)
    d.role('side',99,697,12);d.text('SIDE QUESTS',130,684,18,d.t['side'])
    d.text('SUPPLY HAND-OFF',94,742,26,d.t['text'],d.t['head']);d.text('Bring one blue sample',96,784,19,d.t['muted'])
    d.text('ARCHIVE',94,872,19,d.t['muted'])
    x,y,w,h=538,153,1326,787
    mapbg='#C2C7B5' if d.t['paper'] else d.t['panel'];block='#ACB49D' if d.t['paper'] else '#32453D';road='#DADBCB' if d.t['paper'] else '#182820'
    d.rect('Illustrative district / not level calibration',x,y,w,h,mapbg)
    for row in range(4):
        for col in range(7):
            bx=x+65+col*173;by=y+74+row*165
            d.poly('Block',[ (bx,by),(bx+122,by),(bx+122,by+83),(bx+85,by+83),(bx+85,by+108),(bx,by+108)],block)
    for col in range(1,7):d.line('Street',x+32+col*174,y+30,x+32+col*174,y+h-20,road,20)
    for row in range(1,5):d.line('Street',x+22,y+34+row*165,x+w-22,y+34+row*165,road,24)
    d.line('Main avenue',x+25,y+h-50,x+w-55,y+40,road,38)
    d.text('LOCAL AREA',x+28,y+18,19,d.t['text'],d.t['head']);d.right('N',x+w-24,y+20,26,d.t['text'],d.t['head'])
    d.role('story',1450,440,19,True);d.panel('Goal label',1490,417,218,47);d.text('COURTYARD',1507,426,22,d.t['story'],d.t['head'])
    d.role('side',836,690,17);d.role('personal',1180,325,17)
    d.poly('Player / directional chevron',[(1170,744),(1183,778),(1170,770),(1157,778)],d.t['text'])
    for i,kind in enumerate(['story','side','personal']):
        xx=775+i*300;d.role(kind,xx,976,11);d.text({'story':'Story','side':'Side quest','personal':'My marker'}[kind],xx+25,962,18)
    d.text('50 m',1690,852,16,d.t['muted']);d.line('Scale',1660,888,1808,888,width=2)
    footer(d,'SCROLL  Zoom     DRAG  Pan     LMB  Select     RMB  Personal marker                ESC  Back')


def hudpalette(t):
    if not t['paper']:return t
    return dict(t,panel='#282E28',inset='#3C4237',text='#F0E7D1',muted='#C2C6B5',edge='#788371',story='#E0B08B',side='#B8CCD5',personal='#BACDA9')


def hud(d,quiet=False):
    dd=Draw(d.img,d.p,hudpalette(d.t),d.x,d.y,d.s)
    backdrop(dd,True)
    if quiet:dd.rect('Quick selection scene dim',0,0,1920,1080,'#06100C',18)
    dd.rect('Compass soft backing',673,30,578,103,'#07100E',58)
    for i in range(25):
        xx=710+i*21;dd.line('Compass tick',xx,84,xx,98 if i%6==0 else 91,dd.t['muted'],1)
    for label,xx in [('N',710),('NE',962),('E',1214)]:dd.center(label,xx,43,21,dd.t['text'],dd.t['head'])
    dd.poly('Forward bearing',[(956,100),(968,100),(962,107)],dd.t['text'])
    dd.role('story',1070,118,10,True);dd.role('side',768,118,8);dd.role('personal',870,118,9)
    if not quiet:
        dd.rect('Task contrast',48,157,384,139,'#07100E',72 if d.t['rail'] else 58)
        dd.role('story',78,188,11,True);dd.text('CHECK THE ROUTE',105,170,26,dd.t['text'],dd.t['head'])
        dd.text('Reach the marked courtyard',70,223,21);dd.text('48 m',70,261,16,dd.t['muted'])
        dd.role('story',1320,413,16,True);dd.text('48 m',1350,400,19)
        dd.role('personal',1507,590,12);dd.text('112 m',1538,579,16,dd.t['personal'])
    dd.line('Reticle horizontal',956,540,964,540,dd.t['text'],1.5)
    dd.line('Reticle vertical',960,536,960,544,dd.t['text'],1.5)
    dd.rect('Equipment readability',1450,838,402,193,'#07100E',60)
    dd.icon('rifle',1478,856,175,87.5);dd.text('M16',1697,866,28,dd.t['text'],dd.t['head'])
    dd.text('17',1490,937,57,dd.t['text'],dd.t['head']);dd.text('/ 30',1572,962,27,dd.t['muted'],dd.t['head'])
    dd.icon('magazine',1697,953,12,20);dd.text('2 MAGS',1724,951,22,dd.t['text'],dd.t['head']);dd.text('AUTO',1695,915,14,dd.t['muted'])
    heart(dd.img,dd.p,*dd.xy(1475,1007),19*dd.s,dd.t['text']);dd.meter(1507,1013,315,.72)
    if not quiet:
        dd.key('E',73,842);dd.text('Use',112,842,19,dd.t['text'])
        dd.text('Checkpoint saved',73,930,18,dd.t['muted'])
    return dd


def quick(d):
    q=hud(d,True)
    cx,cy=1368,468;w,h=104,72
    offsets={0:(-116,0),1:(-232,0),2:(0,-84),3:(0,-168),4:(116,0),5:(232,0),6:(0,84),7:(0,168)}
    q.center('QUICK SELECT',cx,226,16,q.t['muted'],q.t['head'])
    for n,(dx,dy) in offsets.items():
        x,y=cx+dx-w/2,cy+dy-h/2
        q.rect('Slot %d / stable layout'%n,x,y,w,h,q.t['panel'],88 if d.t['rail'] else 78)
        q.outline('Slot %d / focus frame'%n,x,y,w,h,q.t['text'] if n==1 else q.t['edge'],2 if n==1 else 1)
        q.right(str(n),x+w-7,y+4,15,q.t['text'] if n==1 else q.t['muted'],q.t['head'])
        if n==0:
            q.icon('explore',x+32,y+6,39,39);q.center('EXPLORE',x+52,y+47,15,q.t['text'],q.t['head'])
        elif n==1:
            q.icon('rifle',x+8,y+14,86,43);q.text('M16',x+8,y+4,14,q.t['text'],q.t['head']);q.right('17 / 30',x+97,y+44,16,q.t['text'],q.t['head'])
            q.rect('Equipped / steady lower notch',x+44,y+69,16,3,q.t['text'])
        else:q.line('Unassigned',x+45,y+36,x+59,y+36,q.t['muted'],1.5)
    for name,pts in [('up',[(cx-4,cy-18),(cx+4,cy-18),(cx,cy-24)]),('down',[(cx-4,cy+18),(cx+4,cy+18),(cx,cy+24)]),('left',[(cx-34,cy-4),(cx-34,cy+4),(cx-40,cy)]),('right',[(cx+34,cy-4),(cx+34,cy+4),(cx+40,cy)])]:q.poly('Centre '+name,pts,q.t['text'] if name=='left' else q.t['muted'])
    q.center('M16',cx,706,30,q.t['text'],q.t['head']);q.center('M16 rifle with a detachable magazine.',cx,750,18)
    q.center('Click  Select    MMB  Assign    Tab  Close',cx,792,15,q.t['muted'])


def menu_base(d,title):
    backdrop(d,True);d.rect('Menu contrast',0,0,1920,1080,'#07100D',61)
    # Paper uses a full light surface so dark header/footer text never sits
    # against the scene border. Dark themes retain their translucent frame.
    if d.t['paper']:d.rect('Menu surface',0,0,1920,1080,d.t['bg'])
    else:d.rect('Menu surface',58,56,1804,967,d.t['bg'],94)
    header(d,title)


def titlemenu(d):
    backdrop(d,True);d.rect('Title legibility',0,0,940,1080,'#08100F',83)
    t=hudpalette(d.t);dd=Draw(d.img,d.p,t,d.x,d.y,d.s)
    dd.text('CHALK',118,159,142,t['text'],t['head']);dd.text('MONTRÉAL  /  2024',126,353,20,t['muted'])
    for i,label in enumerate(['Continue','New game','Load game','Settings','Credits','Exit']):
        yy=465+i*66
        if i==0:
            dd.rect('Selected title action',109,yy-5,410,51,t['inset'],88);dd.rect('Focus edge',109,yy-5,3,51,t['text'])
        dd.text(label.upper() if t['rail'] else label,136,yy,27,t['text'] if i==0 else t['muted'],t['head'])
    dd.text('ENTER  Select',126,962,18,t['muted']);dd.right('CHALK / UI STUDY',1810,1000,13,t['muted'])


def pause(d):
    menu_base(d,'PAUSED')
    d.text('Take a moment.',94,192,49,d.t['text'],d.t['head'])
    for i,label in enumerate(['Resume','Inventory','Map & journal','Load checkpoint','Settings','Main menu']):
        yy=319+i*87
        if i==0:d.panel('Focused pause action',80,yy-12,487,65,True)
        d.text(label,112,yy,29,d.t['text'] if i==0 else d.t['muted'],d.t['head'])
    d.panel('Current task summary',1150,270,640,276)
    d.role('story',1195,321,13,True);d.text('CHECK THE ROUTE',1235,301,30,d.t['text'],d.t['head'])
    d.text('Reach the marked courtyard',1193,382,24);d.text('Tracked objective  ·  48 m',1193,441,18,d.t['muted'])
    d.text('Last checkpoint',1156,641,18,d.t['muted']);d.text('Local area  /  18:42',1156,676,31,d.t['text'],d.t['head'])
    footer(d,'ENTER  Select                                                               ESC  Resume')


def settings(d):
    menu_base(d,'SETTINGS')
    for i,label in enumerate(['General','Video','Audio','Controls']):
        y=198+i*76
        if i==1:d.panel('Settings section focus',81,y-8,332,59,True)
        d.text(label,105,y,27,d.t['text'] if i==1 else d.t['muted'],d.t['head'])
    d.text('Display',511,179,35,d.t['text'],d.t['head']);d.text('Changes apply after confirmation.',511,232,19,d.t['muted'])
    rows=[('Display mode','Borderless'),('Resolution','1920 × 1080'),('Brightness','50%'),('Vertical sync','On'),('Quality preset','High'),('Motion blur','Off')]
    for i,(label,value) in enumerate(rows):
        y=324+i*86;d.line('Setting separator',506,y+60,1747,y+60)
        if i==2:d.rect('Focused setting row',506,y-12,1240,70,d.t['inset'])
        d.text(label,529,y,24)
        if i==2:
            d.meter(1125,y+18,385,.5,'Brightness');d.right(value,1707,y,23)
        else:d.right('‹   '+value+'   ›',1707,y,24)
    d.panel('Apply settings',1487,902,260,59,True);d.center('APPLY',1617,916,24,d.t['text'],d.t['head'])
    d.text('Reset to defaults',525,921,20,d.t['muted']);footer(d,'ENTER  Select                                                               ESC  Back')


def save_load(d,confirm=False):
    menu_base(d,'LOAD CHECKPOINT')
    d.text('Return to a saved moment',94,180,43,d.t['text'],d.t['head'])
    d.text('Current local campaign',96,247,20,d.t['muted'])
    for i,(a,b) in enumerate([('Latest checkpoint','LOCAL AREA  /  18:42'),('Previous checkpoint','LOCAL AREA  /  18:31')]):
        y=321+i*187;d.panel(a,90,y,1000,156,i==0)
        d.text(a,120,y+27,29,d.t['text'],d.t['head']);d.text(b,120,y+88,20,d.t['muted'])
        d.right('LOAD',1056,y+55,24,d.t['text'],d.t['head'])
    d.panel('Saved quest snapshot',1180,321,594,341)
    d.role('story',1222,370,14);d.text('CHECK THE ROUTE',1260,352,28,d.t['text'],d.t['head'])
    d.text('Reach the marked courtyard',1214,425,21);d.text('1 / 3 objectives',1214,476,20,d.t['muted'])
    d.text('Inventory and personal marker restored',1214,562,19,d.t['muted'])
    d.text('Manual saving requires an authored save point.',94,833,20,d.t['muted'])
    footer(d,'ENTER  Select                                                               ESC  Back')
    if confirm:
        d.rect('Confirmation dim',0,0,1920,1080,'#040807',55);d.panel('Load confirmation',527,351,866,381)
        d.text('Load this checkpoint?',569,391,38,d.t['text'],d.t['head']);d.text('Progress since this checkpoint will be replaced.',571,461,23)
        d.panel('Cancel / safe default',567,581,319,74,True);d.center('CANCEL',727,602,24,d.t['text'],d.t['head'])
        d.outline('Confirm load',941,581,399,74);d.center('CONFIRM LOAD',1140,602,24,d.t['text'],d.t['head'])


def loading(d):
    backdrop(d,True);d.rect('Loading contrast',0,0,1920,1080,'#07100E',54)
    t=hudpalette(d.t);a=Draw(d.img,d.p,t,d.x,d.y,d.s)
    a.text('CHALK',74,57,53,t['text'],t['head']);a.text('LOCAL AREA',79,760,46,t['text'],t['head'])
    a.text('An ordinary place, changed.',80,824,22,t['muted'])
    a.line('Loading track',79,961,1840,961,t['edge'],4);a.line('Loading value',79,961,1110,961,t['text'],4)
    a.text('LOADING',80,990,18,t['muted']);a.right('62%',1840,986,24,t['text'],t['head'])


PAGES=[('Inventory',inventory),('Map',drawmap),('Quick',quick),('HUD',hud),('Pause',pause),('Settings',settings),('Title',titlemenu),('Save_Load',save_load),('Item_Actions',lambda d:inventory(d,True)),('Confirmation',lambda d:save_load(d,True)),('Loading',loading)]
exec((PROJECT/'Tools/unified_ui_indicator_sheet.py').read_text(encoding='utf-8'),globals())
PAGES.append(('Indicators',draw_indicators))


def overview(theme):
    img=Gimp.Image.new(2048,2248,Gimp.ImageBaseType.RGB);img.undo_disable()
    p=group(img,'Overview / native layout');d=Draw(img,p,theme)
    rect(img,p,'Overview background',0,0,2048,2248,theme['bg'])
    text(img,p,theme['id']+' / '+theme['name'],48,27,44,theme['text'],theme['head'])
    text(img,p,theme['ru'],48,86,25,theme['text'],'Roboto Regular')
    text(img,p,theme['desc'],48,123,21,theme['muted'],'Roboto Regular')
    selected=[PAGES[0],PAGES[1],PAGES[2],PAGES[3],PAGES[4],PAGES[5]]
    labels=['ИНВЕНТАРЬ','КАРТА И ЗАДАНИЯ','QUICK-МЕНЮ · 8 ЯЧЕЕК','HUD И ИНДИКАТОРЫ','ПАУЗА · ПРЕДЛОЖЕНИЕ','НАСТРОЙКИ · ПРЕДЛОЖЕНИЕ']
    for i,(name,fn) in enumerate(selected):
        col,row=i%2,i//2;x=48+col*992;y=217+row*603
        text(img,p,labels[i],x,y-31,19,theme['muted'],'Roboto Regular')
        section=group(img,name+' / native miniature',p);fn(Draw(img,section,theme,x,y,.5))
    y=2054
    text(img,p,'ОДНИ ОБОЗНАЧЕНИЯ НА ВСЕХ ЭКРАНАХ',48,y,21,theme['muted'],'Roboto Regular')
    for i,(kind,label) in enumerate([('story','Сюжет'),('side','Побочное'),('personal','Моя метка')]):
        x=77+i*345;dd=Draw(img,p,theme);dd.role(kind,x,y+70,18);text(img,p,label,x+38,y+51,24,theme['text'],'Roboto Regular')
    text(img,p,'Фокус — светлая рамка   /   Состояние — отдельный знак',1108,y+54,20,theme['muted'],'Roboto Regular')
    text(img,p,'МАКЕТЫ · ПРИМЕРЫ ДАННЫХ · НЕ ИГРОВЫЕ СКРИНШОТЫ',48,2206,15,theme['muted'],'Roboto Regular')
    return img


def build_theme(theme):
    folder=ROOT/(theme['id']+'_'+theme['name'].replace(' ','_'));folder.mkdir(exist_ok=True)
    img=Gimp.Image.new(1920,1080,Gimp.ImageBaseType.RGB);img.undo_disable();groups=[]
    for index,(name,fn) in enumerate(PAGES):
        for old in groups:old.set_visible(False)
        g=group(img,f'{index+1:02d} / {name} — toggle this screen');groups.append(g)
        fn(Draw(img,g,theme));png_export(img,folder/(name+'.png'))
        with (ROOT/('sources/progress-'+theme['id']+'.log')).open('a',encoding='utf-8') as f:f.write(theme['id']+' '+name+' exported\n')
    for g in groups:g.set_visible(False)
    groups[0].set_visible(True);img.undo_enable()
    master=folder/'CHALK_AllMenus_NATIVE.xcf';assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE,img,Gio.File.new_for_path(str(master)))
    original=counts(img);img.delete()
    reopened=Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,Gio.File.new_for_path(str(master)));assert counts(reopened)==original
    assert len(reopened.get_layers())==len(PAGES);reopened.delete()
    board=overview(theme);png_export(board,ROOT/(theme['id']+'_Overview.png'))
    assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE,board,Gio.File.new_for_path(str(ROOT/(theme['id']+'_Overview_NATIVE.xcf'))))
    summary=counts(board);board.delete()
    REPORT.append({'theme':theme,'folder':str(folder),'screens':[n for n,_ in PAGES],'master':str(master),'native_layers':original,'overview_layers':summary,'saved_master_reopened':True})
    (ROOT/('sources/manifest-'+theme['id']+'.json')).write_text(json.dumps(REPORT[-1],indent=2,ensure_ascii=False),encoding='utf-8')


def build_all():
    Gimp.context_push();Gimp.context_set_antialias(True);Gimp.context_set_feather(False)
    for theme in THEMES:
        if not globals().get('ONLY_THEME') or theme['id']==ONLY_THEME:build_theme(theme)
    Gimp.context_pop()


try:
    build_all()
except Exception:
    (ROOT/'sources/build-error.txt').write_text(traceback.format_exc(),encoding='utf-8')
    raise
