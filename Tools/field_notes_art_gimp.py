"""Field Notes production art. Run only in GIMP3; never touches Unreal assets.

Call run('inspect') first, then run('build'). Old XCFs/PNGs are read-only. Icons
and compass are constructed as clean new native text/vector images; no paths
are removed from loaded masters. Map edits are native GIMP tonal operations.
"""
from gi.repository import Gimp, Gio, Gegl
from pathlib import Path
import hashlib
import json
import math
import struct
import zlib

PROJECT = Path('C:/UnrealEngine/Games/AZ')
ROOT = PROJECT / 'UI Design/CHALK_FieldNotes_Production_v01'
OUT = ROOT / 'unreal-art'
SOURCES = ROOT / 'sources'
OLD_COMPASS = PROJECT / 'UI Design/CHALK_HUD_v03/unreal-art/Compass/CHALK_Compass_Strip_NATIVE.xcf'
MAP_SOURCE = PROJECT / 'UI Design/CHALK_QuestMap_v01/sources/map/L001_FinalColor.png'
MAP_RECEIPT = PROJECT / 'UI Design/CHALK_QuestMap_v01/sources/map/map-art-receipt.json'
FONT_FOLDER = PROJECT / 'UI Design/CHALK_HUD_v01/sources/fonts'
ICONS = ('Story', 'Side', 'Personal', 'Active', 'Complete', 'Failed')
WHITE = '#FFFFFF'
PAPER_CURVE = [0.0, .87, .03, .85, .08, .80, .18, .72, .38, .65, .75, .53, 1.0, .48]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png_stats(path):
    """Read-only PNG channel inspection with stdlib; never edits image pixels."""
    from pathlib import Path
    import struct
    import zlib
    raw = Path(path).read_bytes()
    assert raw[:8] == b'\x89PNG\r\n\x1a\n'
    width,height,depth,kind,_,_,interlace = struct.unpack('>IIBBBBB',raw[16:29])
    assert depth == 8 and kind in (2,6) and interlace == 0
    data = bytearray(); offset = 8
    while offset < len(raw):
        length = struct.unpack('>I',raw[offset:offset+4])[0]
        if raw[offset+4:offset+8] == b'IDAT': data.extend(raw[offset+8:offset+8+length])
        offset += length+12
    pixels = zlib.decompress(data); channels = 4 if kind == 6 else 3; stride = width*channels
    previous = bytearray(stride); offset = 0
    min_rgb,max_rgb = [255,255,255],[0,0,0]
    min_a,max_a = 255,0
    box = [width,height,-1,-1]; visible = 0; nonwhite = 0; nongray = 0
    for y in range(height):
        filter_type = pixels[offset]; offset += 1
        row = bytearray(pixels[offset:offset+stride]); offset += stride
        for i in range(stride):
            left = row[i-channels] if i >= channels else 0
            above = previous[i]; corner = previous[i-channels] if i >= channels else 0
            if filter_type == 1: predictor = left
            elif filter_type == 2: predictor = above
            elif filter_type == 3: predictor = (left+above)//2
            elif filter_type == 4:
                p = left+above-corner
                pa,pb,pc = abs(p-left),abs(p-above),abs(p-corner)
                predictor = left if pa <= pb and pa <= pc else above if pb <= pc else corner
            else:
                assert filter_type == 0
                predictor = 0
            row[i] = (row[i]+predictor)&255
        for x in range(width):
            i = x*channels; rgb = row[i:i+3]; alpha = row[i+3] if channels == 4 else 255
            min_a,max_a = min(min_a,alpha),max(max_a,alpha)
            if alpha:
                visible += 1; nonwhite += rgb != bytearray((255,255,255)); nongray += not(rgb[0] == rgb[1] == rgb[2])
                box = [min(box[0],x),min(box[1],y),max(box[2],x),max(box[3],y)]
                for c in range(3): min_rgb[c],max_rgb[c] = min(min_rgb[c],rgb[c]),max(max_rgb[c],rgb[c])
        previous = row
    return {'size':[width,height],'bit_depth':depth,'color_type':kind,'alpha_range':[min_a,max_a],
            'visible_bbox_inclusive':box,'visible_pixels':visible,'visible_rgb_min':min_rgb,'visible_rgb_max':max_rgb,
            'visible_nonwhite_pixels':nonwhite,'visible_nongray_pixels':nongray}


def color(value):
    return Gegl.Color.new(value)


def group(image, name):
    obj = Gimp.GroupLayer.new(image, name)
    image.insert_layer(obj, None, 0)
    return obj


def vector(image, parent, name, path, fill=WHITE):
    path.set_name(name + ' / retained editable path')
    path.set_visible(False)
    layer = Gimp.VectorLayer.new(image, path)
    image.insert_layer(layer, parent, 0)
    layer.set_name(name)
    layer.set_enable_fill(True)
    layer.set_fill_color(color(fill) if isinstance(fill, str) else fill)
    # The installed3.2.4 enable-stroke setter emits a GLib validation error and
    # can leave the default black stroke active. Filled path geometry is the
    # already-proven project recipe; explicitly zero/clear the unused stroke.
    layer.set_stroke_color(color('rgba(0,0,0,0)'))
    layer.set_stroke_width(0)
    layer.refresh()
    return layer


def path_lines(image, parent, name, segments, width, tint=WHITE):
    result = None
    index = 0
    for points, closed in segments:
        points = list(points) + ([points[0]] if closed else [])
        for start, end in zip(points, points[1:]):
            dx,dy = end[0]-start[0],end[1]-start[1]
            length = math.hypot(dx,dy)
            assert length > 0
            nx,ny = -dy/length*width/2,dx/length*width/2
            polygon = [(start[0]+nx,start[1]+ny),(end[0]+nx,end[1]+ny),
                       (end[0]-nx,end[1]-ny),(start[0]-nx,start[1]-ny)]
            # Separate filled vector layers compose as a union. Compound paths
            # use an even/odd fill and otherwise cut holes where segments meet.
            segment_name = name + ' / segment ' + str(index)
            path = Gimp.Path.new(image, segment_name)
            image.insert_path(path, None, 0)
            sid = path.bezier_stroke_new_moveto(*polygon[0])
            for point in polygon[1:]:
                path.bezier_stroke_lineto(sid,*point)
            path.stroke_close(sid)
            result = vector(image, parent, segment_name, path, fill=tint)
            index += 1
    return result


def rect(image, parent, name, x, y, w, h, tint):
    path = Gimp.Path.new(image, name)
    image.insert_path(path, None, 0)
    sid = path.bezier_stroke_new_moveto(x, y)
    for point in ((x+w, y), (x+w, y+h), (x, y+h)):
        path.bezier_stroke_lineto(sid, *point)
    path.stroke_close(sid)
    return vector(image, parent, name, path, fill=tint)


def contours(image, parent, name, polygons):
    path = Gimp.Path.new(image,name)
    image.insert_path(path,None,0)
    for points in polygons:
        sid = path.bezier_stroke_new_moveto(*points[0])
        for point in points[1:]: path.bezier_stroke_lineto(sid,*point)
        path.stroke_close(sid)
    return vector(image,parent,name,path,fill=WHITE)


def label(image, parent, value, cx, y, size, tint=WHITE, font='Roboto Bold'):
    resolved = Gimp.Font.get_by_name(font)
    assert resolved is not None, 'Required font not loaded: ' + font
    layer = Gimp.TextLayer.new(image, value, resolved, float(size), Gimp.Unit.pixel())
    image.insert_layer(layer, parent, 0)
    layer.set_name('Text / ' + value)
    layer.set_color(color(tint))
    layer.set_offsets(round(cx-layer.get_width()/2), round(y))
    return layer


def walk(items):
    for item in items:
        yield item
        if item.is_group():
            yield from walk(item.get_children())


def native_readback(image):
    result = {'dimensions': [image.get_width(), image.get_height()], 'text': [], 'vectors': [], 'raster': [], 'paths': len(image.get_paths())}
    for item in walk(image.get_layers()):
        if item.is_group():
            continue
        base = {'name': item.get_name(), 'visible': item.get_visible(), 'opacity': item.get_opacity(),
                'offset': list(item.get_offsets())[-2:], 'dimensions': [item.get_width(), item.get_height()]}
        if isinstance(item, Gimp.TextLayer):
            size = item.get_font_size()
            base.update(text=item.get_text(), font=item.get_font().get_name(), size=float(size[0] if isinstance(size, tuple) else size))
            result['text'].append(base)
        elif isinstance(item, Gimp.VectorLayer):
            path = item.get_path()
            strokes = []
            for sid in path.get_strokes():
                kind, points, closed = path.stroke_get_points(sid)
                strokes.append({'type': int(kind), 'points': list(points), 'closed': closed})
            base.update(strokes=strokes, rgba=list(item.get_fill_color().get_rgba()))
            result['vectors'].append(base)
        else:
            base['filters'] = len(item.get_filters())
            result['raster'].append(base)
    return result


def export_png(image, path):
    proc = Gimp.get_pdb().lookup_procedure('file-png-export')
    config = proc.create_config()
    config.set_property('run-mode', Gimp.RunMode.NONINTERACTIVE)
    config.set_property('image', image)
    config.set_property('file', Gio.File.new_for_path(str(path)))
    assert proc.run(config).index(0) == Gimp.PDBStatusType.SUCCESS


def save_native(image, stem):
    xcf, png = ROOT / (stem + '_NATIVE.xcf'), OUT / (stem + '.png')
    image.undo_enable()
    assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, image, Gio.File.new_for_path(str(xcf)))
    export_png(image, png)
    reopened = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(str(xcf)))
    readback = native_readback(reopened)
    assert readback['dimensions'] == [image.get_width(), image.get_height()]
    (SOURCES / (stem + '-readback.json')).write_text(json.dumps(readback, indent=2), encoding='utf-8')
    # Close images as a whole; never remove vector paths from a loaded master.
    reopened.delete()
    image.delete()
    return {'stem': stem, 'png': str(png), 'xcf': str(xcf), 'size': readback['dimensions'],
            'native_text': len(readback['text']), 'native_vectors': len(readback['vectors']), 'retained_paths': readback['paths'],
            'native_raster': len(readback['raster']), 'reopened': True}


def inspect_source():
    source = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(str(OLD_COMPASS)))
    data = native_readback(source)
    assert data['dimensions'] == [2880, 128]
    assert set(row['text'] for row in data['text']) == {'N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'}
    assert data['vectors'] and all(row['name'].startswith('Tick ') for row in data['vectors'])
    data['source'] = str(OLD_COMPASS)
    data['source_sha256'] = sha(OLD_COMPASS)
    data['phase'] = -0.00275
    data['north_anchor_x'] = 2880 * (.5 - .00275)
    data['period_degrees'] = 360
    (SOURCES / 'original-compass-master-readback.json').write_text(json.dumps(data, indent=2), encoding='utf-8')
    # Read-only loaded master is left intact until this isolated batch process exits.
    return data


def make_icon(kind):
    image = Gimp.Image.new(128, 128, Gimp.ImageBaseType.RGB)
    image.undo_disable()
    parent = group(image, kind + ' / white RGB, coverage in alpha')
    if kind == 'Story':
        for radius in (52.0, 52.0*.57):
            rings = []
            for r in (radius+7.2/math.sqrt(2),radius-7.2/math.sqrt(2)):
                rings.append([(64,64-r),(64+r,64),(64,64+r),(64-r,64)])
            contours(image,parent,'Story / continuous diamond ring '+str(radius),rings)
    elif kind == 'Side':
        path = Gimp.Path.new(image,'Side / continuous open circle')
        image.insert_path(path,None,0)
        path.bezier_stroke_new_ellipse(64,64,55.6,55.6,0)
        path.bezier_stroke_new_ellipse(64,64,48.4,48.4,0)
        vector(image,parent,'Side / continuous open circle',path,fill=WHITE)
        rect(image, parent, 'Side / centre point', 56, 56, 16, 16, WHITE)
    elif kind == 'Personal':
        for sx, sy in ((-1,-1),(-1,1),(1,-1),(1,1)):
            points = [(64+sx*x,64+sy*y) for x,y in ((28,48.4),(48.4,48.4),(48.4,28),(55.6,28),(55.6,55.6),(28,55.6))]
            contours(image,parent,'Personal / continuous open corner '+str((sx,sy)),[points])
    elif kind == 'Active':
        contours(image,parent,'Active / continuous square badge', [[(8,8),(120,8),(120,120),(8,120)],
                    [(16,16),(112,16),(112,112),(16,112)]])
        path = Gimp.Path.new(image, 'Active / live centre point')
        image.insert_path(path, None, 0)
        path.bezier_stroke_new_ellipse(64,64,12,12,0)
        vector(image, parent, 'Active / live centre point', path, fill=WHITE)
    elif kind == 'Complete':
        path_lines(image, parent, 'Completed / check', [([(16,64),(48,100),(112,24)], False)], 12.0)
    elif kind == 'Failed':
        path_lines(image, parent, 'Failed / cross', [([(16,16),(112,112)], False), ([(16,112),(112,16)], False)], 12.0)
    return save_native(image, 'T_FN_' + kind)


def make_compass():
    source = json.loads((SOURCES / 'original-compass-master-readback.json').read_text(encoding='utf-8'))
    assert source['source_sha256'] == sha(OLD_COMPASS), 'Source changed since inspection.'
    image = Gimp.Image.new(2880, 128, Gimp.ImageBaseType.RGB)
    image.undo_disable()
    ticks = group(image, '01 / ticks — exact original native path geometry')
    for row in source['vectors']:
        path = Gimp.Path.new(image, row['name'])
        image.insert_path(path, None, 0)
        for stroke in row['strokes']:
            path.stroke_new_from_points(Gimp.PathStrokeType(stroke['type']), stroke['points'], stroke['closed'])
        tint = color(WHITE)
        tint.set_rgba(*row['rgba'])
        vector(image, ticks, row['name'], path, fill=tint)
    labels = group(image, '02 / directions — native Roboto Bold text')
    anchors = []
    for degree, word in ((0,'N'),(45,'NE'),(90,'E'),(135,'SE'),(180,'S'),(225,'SW'),(270,'W'),(315,'NW')):
        base_x = (source['north_anchor_x'] + degree * 8) % 2880
        for x in (base_x-2880, base_x, base_x+2880):
            if -50 <= x <= 2930:
                label(image, labels, word, x, 8, 42, WHITE, 'Roboto Bold')
                anchors.append({'degrees': degree, 'label': word, 'x': x, 'y': 8, 'font': 'Roboto Bold', 'size': 42})
    new = native_readback(image)
    before = {row['name']: (row['strokes'], row['rgba']) for row in source['vectors']}
    after = {row['name']: (row['strokes'], row['rgba']) for row in new['vectors']}
    assert before == after, 'Compass tick path data or brightness changed.'
    result = save_native(image, 'T_FN_CompassStrip')
    result.update(period_degrees=360, shader_phase=-0.00275, north_anchor_x=source['north_anchor_x'],
                  draw_size=[1440,64], visible_window=[600,64], anchors=anchors, tick_geometry_equal=True,
                  label_font='Roboto Bold', baked_font_px=42, label_font_at_half_size_px=21)
    return result


def make_map():
    receipt = json.loads(MAP_RECEIPT.read_text(encoding='utf-8'))
    assert sha(MAP_SOURCE) == receipt['source_sha256']
    assert receipt['calibration']['origin'] == [-1435,3620,0] and receipt['calibration']['size'] == [12400,12400]
    image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(str(MAP_SOURCE)))
    assert [image.get_width(),image.get_height()] == [2048,2048]
    image.undo_disable()
    original = image.get_layers()[0]
    original.set_name('00 / original calibrated L_001 capture — unchanged hidden source')
    original.set_visible(False)
    rect(image, None, '01 / opaque paper backing', 0,0,2048,2048,'#C2C7B5')
    parent = group(image, '10 / Field Notes — tonal treatment only')
    working = original.copy()
    image.insert_layer(working,parent,0)
    working.set_name('11 / real geometry — luminance and inverted paper curve')
    working.set_visible(True)
    working.set_offsets(0,0)
    assert working.desaturate(Gimp.DesaturateMode.LUMINANCE)
    assert working.curves_spline(Gimp.HistogramChannel.VALUE,PAPER_CURVE)
    tone = rect(image,parent,'12 / paper sage chroma — separate editable layer',0,0,2048,2048,'#BFC7B0')
    tone.set_mode(Gimp.LayerMode.HSL_COLOR)
    result = save_native(image, 'T_FN_Map_L001')
    result.update(source=str(MAP_SOURCE), source_sha256=receipt['source_sha256'], calibration=receipt['calibration'],
                  world_cm_per_pixel=6.0546875, geometric_edits=[], baked_markers=False,
                  paper_curve=PAPER_CURVE, paper_chroma='#BFC7B0')
    return result


def make_preview():
    image = Gimp.Image.new(1536, 768, Gimp.ImageBaseType.RGB)
    image.undo_disable()
    parent = group(image,'Production review — not imported as game UI')
    rect(image,parent,'Paper context',0,0,1536,324,'#DDD6C4')
    rect(image,parent,'HUD context',0,324,1536,444,'#282E28')
    label(image,parent,'FIELD NOTES / PRODUCTION MASKS',768,22,28,'#282E29')
    label(image,parent,'Same alpha masks; per-context runtime tints',768,66,18,'#596355','Roboto Regular')
    for row, palette in enumerate((['#8E573E','#466477','#526D48','#282E29','#282E29','#913F34'],
                                   ['#E0B08B','#B8CCD5','#BACDA9','#F0E7D1','#F0E7D1','#913F34'])):
        for index, kind in enumerate(ICONS):
            cx, y = 128 + index * 256, 128 + row * 264
            icon = Gimp.file_load_layer(Gimp.RunMode.NONINTERACTIVE,image,Gio.File.new_for_path(str(OUT / ('T_FN_'+kind+'.png'))))
            image.insert_layer(icon,parent,0)
            icon.set_name(kind+' / tint preview only')
            icon.scale(64,64,False)
            icon.set_offsets(cx-32,y)
            icon.set_lock_alpha(True)
            Gimp.context_set_foreground(color(palette[index])); icon.edit_fill(Gimp.FillType.FOREGROUND)
            icon.set_lock_alpha(False)
            label(image,parent,kind.upper(),cx,y+84,21,'#282E29' if row==0 else '#F0E7D1')
    strip = Gimp.file_load_layer(Gimp.RunMode.NONINTERACTIVE,image,Gio.File.new_for_path(str(OUT/'T_FN_CompassStrip.png')))
    image.insert_layer(strip,parent,0)
    strip.set_name('Compass / full360deg period at1440x64 — white source before tint')
    strip.scale(1440,64,False); strip.set_offsets(48,607)
    label(image,parent,'ROBOTO BOLD / 360° / TICKS AND PHASE PRESERVED',768,694,20,'#F0E7D1')
    return save_native(image,'FieldNotes_Art_QA')


def run(mode='inspect'):
    ROOT.mkdir(parents=True,exist_ok=True); OUT.mkdir(exist_ok=True); SOURCES.mkdir(exist_ok=True)
    Gimp.context_push(); Gimp.context_set_antialias(True); Gimp.context_set_feather(False)
    try:
        if mode == 'inspect':
            data = inspect_source()
            print(json.dumps({'inspected':str(OLD_COMPASS),'size':data['dimensions'],'vectors':len(data['vectors']),
                              'labels':data['text'],'source_sha256':data['source_sha256']}))
            return
        if mode == 'icons':
            manifest = json.loads((ROOT/'production-manifest.json').read_text(encoding='utf-8'))
            updates = [make_icon(kind) for kind in ICONS] + [make_preview()]
            by_stem = {row['stem']:row for row in updates}
            manifest['products'] = [by_stem.get(row['stem'],row) for row in manifest['products']]
            (ROOT/'production-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
            print('Field Notes icon masters and preview updated.')
            return
        assert mode == 'build'
        originals = {str(p):sha(p) for p in (OLD_COMPASS,MAP_SOURCE,MAP_RECEIPT)}
        products = [make_icon(kind) for kind in ICONS]
        products.append(make_compass()); products.append(make_map()); products.append(make_preview())
        assert originals == {path:sha(Path(path)) for path in originals}, 'Old art/source changed.'
        result = {'products':products,'originals_unchanged':originals,'ue_assets_changed':False,
                  'font_sources':[str(FONT_FOLDER/'Roboto-Bold.ttf'),str(FONT_FOLDER/'Roboto-Regular.ttf')],
                  'optional_status':'Text modifier only; no Optional icon exported.',
                  'reuse_existing_art':['Fight/Explore','Weapons/ammunition','Heart','Compass pointer'],
                  'channel_contract':'Roles/status: white straight RGB with antialiased coverage in alpha. Compass: white labels and original white/gray ticks over transparent alpha. Map: opaque sRGB colour; no baked pins.'}
        (ROOT/'production-manifest.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        print('Field Notes production complete: '+str(ROOT))
    except Exception as error:
        (SOURCES/'error.json').write_text(json.dumps({'error':str(error)},indent=2),encoding='utf-8')
        raise
    finally:
        Gimp.context_pop(); Gimp.displays_flush()
