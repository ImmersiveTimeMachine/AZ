"""CHALK HUD v03: native GIMP text/vector shapes, plus a compass study.

The existing generated scene is the sole source-image layer. All HUD geometry
is drawn as native GIMP paths/vector layers, including an alpha trace of the
inventory rifle made by GIMP's Selection to Path tool.
"""
from pathlib import Path
import ast

V01=Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v01')
V02=Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v02')
defs=ast.parse((V02/'sources/build_iteration_gimp.py').read_text(encoding='utf-8'))
defs.body=[n for n in defs.body if not isinstance(n,ast.Try)]
exec(compile(defs,str(V02/'sources/build_iteration_gimp.py'),'exec'),globals())
ROOT=Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03')
SRC=V01/'sources'
image_import=imported
paint_layer=layer
serial=0


def log(msg):
    with (ROOT/'sources/build_progress.log').open('a',encoding='utf-8') as stream:
        stream.write(msg+'\n')


def vec(img,parent,name,path,value,opacity=100):
    path.set_name(name+' / editable path')
    path.set_visible(False)
    obj=Gimp.VectorLayer.new(img,path)
    img.insert_layer(obj,parent,0)
    obj.set_name(name)
    obj.set_enable_fill(True)
    obj.set_fill_color(color(value))
    obj.set_stroke_color(color('rgba(0,0,0,0)'))
    obj.set_stroke_width(0)
    obj.set_opacity(opacity)
    obj.refresh()
    return obj


def shape(img,parent,name,pts,value,opacity=100):
    path=Gimp.Path.new(img,name+' / editable path')
    img.insert_path(path,None,0)
    sid=path.bezier_stroke_new_moveto(*pts[0])
    for p in pts[1:]:
        path.bezier_stroke_lineto(sid,*p)
    path.stroke_close(sid)
    return vec(img,parent,name,path,value,opacity)


def layer(img,parent,name,w=None,h=None,x=0,y=0,opacity=100):
    # Legacy icon helpers now return a semantic group. Every primitive drawn
    # into that group becomes a separate native vector layer.
    obj=group(img,name,parent)
    obj.set_opacity(opacity)
    return obj


def polygon(img,parent,pts,value):
    global serial
    serial+=1
    return shape(img,parent,'Shape %03d / fill'%serial,pts,value)


def rect(img,parent,name,x,y,w,h,value,opacity=100):
    return shape(img,parent,name,[(x,y),(x+w,y),(x+w,y+h),(x,y+h)],value,opacity)


def line(img,parent,x1,y1,x2,y2,width,value,name=None,opacity=100):
    global serial
    serial+=1
    dx,dy=x2-x1,y2-y1
    length=max(.001,math.hypot(dx,dy))
    ax,ay=-dy/length*width/2,dx/length*width/2
    return shape(img,parent,name or 'Line %03d'%serial,
                 [(x1+ax,y1+ay),(x2+ax,y2+ay),(x2-ax,y2-ay),(x1-ax,y1-ay)],value,opacity)


def ellipse(img,parent,x,y,w,h,value,name=None):
    global serial
    serial+=1
    path=Gimp.Path.new(img,name or 'Ellipse %03d'%serial)
    img.insert_path(path,None,0)
    path.bezier_stroke_new_ellipse(x+w/2,y+h/2,w/2,h/2,0)
    return vec(img,parent,name or 'Ellipse %03d'%serial,path,value)


def scrim(img,parent,x,y,w,h,opacity=52):
    # A soft GIMP selection fill stays on its own paint layer. This is drawn
    # in GIMP, not generated artwork or a flattened HUD screenshot.
    obj=paint_layer(img,parent,'Contrast / GIMP feathered fill',w,h,x,y,opacity)
    img.select_rectangle(Gimp.ChannelOps.REPLACE,x+25,y+25,w-50,h-50)
    Gimp.Selection.feather(img,32)
    fill(img,obj,DARK)
    return obj


def imported(img,parent,name,filename,x,y,w=None,h=None,tint=None,opacity=100):
    if filename!='Rifle_PrimaryIcon.png':
        return image_import(img,parent,name,filename,x,y,w,h,tint,opacity)
    log('Trace rifle: '+name)
    raster=Gimp.file_load_layer(Gimp.RunMode.NONINTERACTIVE,img,Gio.File.new_for_path(str(SRC/filename)))
    img.insert_layer(raster,None,0)
    raster.set_offsets(0,0)
    iw,ih=raster.get_width(),raster.get_height()
    before={p.get_id() for p in img.get_paths()}
    img.select_item(Gimp.ChannelOps.REPLACE,raster)
    proc=Gimp.get_pdb().lookup_procedure('plug-in-sel2path')
    cfg=proc.create_config()
    cfg.set_property('run-mode',Gimp.RunMode.NONINTERACTIVE)
    cfg.set_property('image',img)
    cfg.set_core_object_array('drawables',[raster])
    result=proc.run(cfg)
    if result.index(0)!=Gimp.PDBStatusType.SUCCESS:
        raise RuntimeError('GIMP Selection to Path failed')
    paths=[p for p in img.get_paths() if p.get_id() not in before]
    if len(paths)!=1:
        raise RuntimeError('Expected one traced path, got '+str(len(paths)))
    trace=paths[0]
    Gimp.Selection.none(img)
    sx=float(w or iw)/iw
    sy=float(h or ih*sx)/ih
    for sid in trace.get_strokes():
        trace.stroke_scale(sid,sx,sy)
        trace.stroke_translate(sid,float(x),float(y))
    img.remove_layer(raster)
    return vec(img,parent,name+' / GIMP native trace',trace,tint or WHITE,opacity)


def bar(img,parent,x,y,width,height,percent,rough=False,value=WHITE):
    g=group(img,'Health / editable vector meter',parent)
    # Quiet asymmetric edge matches A without baking a pack texture into it.
    def contour(w):
        return [(x,y+height*.25),(x+w*.03,y),(x+w*.34,y+height*.08),
                (x+w*.7,y),(x+w,y+height*.13),(x+w,y+height*.87),
                (x+w*.71,y+height),(x+w*.35,y+height*.92),(x,y+height)]
    bg=shape(img,g,'Track / full health',contour(width),'#717875',52)
    fg=shape(img,g,'Fill / 72 percent',contour(width*percent),value)
    return bg,fg


def heart(img,parent,x,y,size=20,value=WHITE):
    path=Gimp.Path.new(img,'Health / heart')
    img.insert_path(path,None,0)
    s=path.bezier_stroke_new_moveto(x+size*.5,y+size*.92)
    path.bezier_stroke_cubicto(s,x+size*.32,y+size*.72,x+size*.01,y+size*.48,x+size*.04,y+size*.28)
    path.bezier_stroke_cubicto(s,x+size*.08,y+size*.02,x+size*.37,y+size*.05,x+size*.5,y+size*.27)
    path.bezier_stroke_cubicto(s,x+size*.63,y+size*.05,x+size*.92,y+size*.02,x+size*.96,y+size*.28)
    path.bezier_stroke_cubicto(s,x+size*.99,y+size*.48,x+size*.68,y+size*.72,x+size*.5,y+size*.92)
    path.stroke_close(s)
    return vec(img,parent,'Health / heart symbol',path,value)


def outline_diamond(img,parent,name,cx,cy,r,value):
    g=group(img,name,parent)
    points=[(cx,cy-r),(cx+r,cy),(cx,cy+r),(cx-r,cy),(cx,cy-r)]
    for i in range(4):
        line(img,g,*points[i],*points[i+1],2,value,'Diamond / edge '+str(i+1))
    return g


def compass(img):
    g=group(img,'30 / COMPASS - toggle whole compass')
    back=scrim(img,g,615,26,690,128,48)
    back.set_name('Contrast / optional compass scrim')
    scale=group(img,'01 / SCALE - ticks and directions',g)
    major=group(img,'Major ticks',scale)
    minor=group(img,'Minor ticks',scale)
    # Illustrative view: center bearing 045 degrees. Camera-forward pointer
    # is fixed; headings/targets would scroll under it in the real widget.
    for bearing in range(-30,121,5):
        x=960+(bearing-45)*4
        opacity=100 if 740<x<1180 else 48
        large=(bearing%45==0)
        line(img,major if large else minor,x,92,x,103 if large else 98,1.4 if large else 1,
             WHITE if large else SOFT,'Tick / %03d deg'%(bearing%360),opacity)
    labels=group(img,'Direction labels / native text',scale)
    center_label(img,labels,'N',780,58,23,SOFT)
    center_label(img,labels,'NE',960,54,27,WHITE)
    center_label(img,labels,'E',1140,58,23,SOFT)
    pointer=group(img,'02 / HEADING POINTER - camera forward',g)
    shape(img,pointer,'Pointer / fixed triangle',[(955,39),(965,39),(960,45)],WHITE)
    active=group(img,'03 / TRACKED TARGET - icon and distance',g)
    outline_diamond(img,active,'Objective / diamond',1052,120,7,PEACH)
    ellipse(img,active,1050.5,118.5,3,3,PEACH,'Objective / center dot')
    center_label(img,active,'126 m',1052,137,19,WHITE,BODY)
    label=center_label(img,active,'TRACKED OBJECTIVE',1052,166,17,SOFT)
    label.set_name('Optional / objective name - hidden')
    label.set_visible(False)
    optional=group(img,'04 / PLAYER WAYPOINT - optional second target',g)
    outline_diamond(img,optional,'Waypoint / small diamond',838,120,5,WHITE)
    center_label(img,optional,'38 m',838,137,18,SOFT,BODY)
    optional.set_visible(False)
    return g


def tidy_names(img):
    # All items get unique hierarchical names; short primitives remain inside
    # semantic groups so users can toggle a whole icon or its individual parts.
    for top in img.get_layers():
        if top.is_group() and top.get_name().startswith('A /'):
            top.set_name('10 / CORE HUD - health and equipped weapon')
        if top.is_group() and top.get_name().startswith('02 / QUICK'):
            top.set_name('20 / QUICK SELECT - toggle open or closed')


def audit(img):
    counts={'text':0,'vector':0,'paint':0,'image':0,'groups':0}
    def recurse(items):
        rows=[]
        for item in items:
            row={'name':item.get_name(),'visible':item.get_visible(),'opacity':item.get_opacity()}
            if item.is_group():
                counts['groups']+=1
                row['type']='group'
                row['children']=recurse(item.get_children())
            elif isinstance(item,Gimp.TextLayer):
                counts['text']+=1
                row.update(type='native_text',text=item.get_text(),font=item.get_font().get_name())
            elif isinstance(item,Gimp.VectorLayer):
                counts['vector']+=1
                p=item.get_path()
                row.update(type='native_vector',path=p.get_name(),strokes=len(p.get_strokes()))
            elif 'GENERATED SCENE' in item.get_name():
                counts['image']+=1
                row['type']='source_image'
            else:
                counts['paint']+=1
                row['type']='gimp_feathered_fill'
            rows.append(row)
        return rows
    rows=recurse(img.get_layers())
    return {'canvas':[img.get_width(),img.get_height()],'counts':counts,'path_count':len(img.get_paths()),'layers':rows}


def build():
    log('BEGIN V03 NATIVE')
    Gimp.context_push()
    Gimp.context_set_antialias(True)
    Gimp.context_set_feather(False)
    img=Gimp.Image.new(1920,1080,Gimp.ImageBaseType.RGB)
    img.undo_disable()
    bg=imported(img,None,'00 / GENERATED SCENE - image only','montreal_concept_plate.png',0,0,1920,1080)
    a=quiet(img)
    # Preserve contextual examples as individually toggleable native layers.
    for child in a.get_children():
        if child.get_name().startswith('Context /'):
            child.set_visible(False)
    q=quick_selector(img)
    q.set_visible(False)
    nav=compass(img)
    tidy_names(img)
    log('Native drawing complete')
    png(img,'01_HUD_Compass.png')
    nav.set_visible(False)
    png(img,'02_HUD_No_Compass.png')
    nav.set_visible(True)
    q.set_visible(True)
    a.set_opacity(60)
    nav.set_opacity(45)
    png(img,'03_Quick_Select_Compass.png')
    q.set_visible(False)
    a.set_opacity(100)
    nav.set_opacity(100)
    img.undo_enable()
    xcf(img,'CHALK_HUD_v03_NATIVE.xcf')
    bg.set_visible(False)
    png(img,'HUD_Compass_Transparent.png')
    bg.set_visible(True)
    q.set_visible(True)
    a.set_opacity(60)
    nav.set_opacity(45)
    xcf(img,'CHALK_Quick_Select_v03_NATIVE.xcf')
    report=audit(img)
    (ROOT/'layer_manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    # Reopen the saved master to inspect actual native layer persistence.
    saved=Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,Gio.File.new_for_path(str(ROOT/'CHALK_HUD_v03_NATIVE.xcf')))
    readback=audit(saved)
    (ROOT/'saved_layer_readback.json').write_text(json.dumps(readback,indent=2),encoding='utf-8')
    log('READBACK '+json.dumps(readback['counts'])+' paths='+str(readback['path_count']))
    saved.delete()
    Gimp.context_pop()
    log('DONE V03')


try:
    build()
except Exception:
    log(traceback.format_exc())
    raise
