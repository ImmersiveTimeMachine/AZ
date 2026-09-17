"""GIMP 3.2 batch: eight native editable cards on the existing Montreal image.

Mockup only. No Unreal source, assets, input or gameplay state is modified.
"""
from gi.repository import Gimp, Gio
from pathlib import Path
import ast
import json
import math
import traceback

PROJECT = Path('C:/UnrealEngine/Games/AZ')
BASE = PROJECT / 'UI Design/CHALK_HUD_v03/sources/build_native_hud.py'
helpers = ast.parse(BASE.read_text(encoding='utf-8'))
helpers.body = [node for node in helpers.body if not isinstance(node, ast.Try)]
exec(compile(helpers, str(BASE), 'exec'), globals())
ROOT = PROJECT / 'UI Design/CHALK_QuickSelect_v05'
ROOT.mkdir(parents=True, exist_ok=True)
(ROOT / 'sources').mkdir(exist_ok=True)

# Explicit pixel geometry at the existing 1920x1080 reference resolution.
CX, CY = 1368, 468
W, H = 104, 72
OFFSETS = {0: (-116, 0), 1: (-232, 0), 2: (0, -84), 3: (0, -168),
           4: (116, 0), 5: (232, 0), 6: (0, 84), 7: (0, 168)}
BOXES = {key: (CX + dx - W / 2, CY + dy - H / 2, W, H)
         for key, (dx, dy) in OFFSETS.items()}
WHITE = '#EEEAE0'
SOFT = '#B9B9B9'
ACCENT = '#FFBA8C'


def outline(image, parent, name, box, tint, thickness=1, opacity=100):
    x, y, w, h = box
    g = group(image, name, parent)
    for i, segment in enumerate(((x,y,x+w,y), (x+w,y,x+w,y+h),
                                  (x+w,y+h,x,y+h), (x,y+h,x,y))):
        line(image, g, *segment, thickness, tint, name + ' / edge ' + str(i), opacity)
    return g


def centered(image, parent, label, cx, y, size, tint=WHITE, font=FONT, name=None):
    item = text(image, parent, label, 0, y, size, tint, font, name=name)
    item.set_offsets(round(cx - item.get_width() / 2), round(y))
    return item


def walking(image, parent, x, y, size):
    """Reuse the already-authored native geometry, without executing its export job."""
    g = group(image, 'Explore action / editable walking symbol', parent)
    before = {p.get_id() for p in image.get_paths()}
    source = ast.parse((PROJECT / 'Tools/quick_select_explore_art.py').read_text(encoding='utf-8'))
    calls = []
    for node in source.body:
        if isinstance(node, ast.Expr) and isinstance(node.value, ast.Call) and isinstance(node.value.func, ast.Name):
            if node.value.func.id in ('ellipse', 'shape'):
                node.value.args[1] = ast.Name(id='glyph_parent', ctx=ast.Load())
                calls.append(node)
    code = ast.fix_missing_locations(ast.Module(body=calls, type_ignores=[]))
    scope = dict(globals(), img=image, glyph_parent=g, white=WHITE)
    exec(compile(code, '<native walking geometry>', 'exec'), scope)
    for path in image.get_paths():
        if path.get_id() not in before:
            for stroke in path.get_strokes():
                path.stroke_scale(stroke, size / 128, size / 128)
                path.stroke_translate(stroke, x, y)
    for child in g.get_children():
        if isinstance(child, Gimp.VectorLayer):
            child.refresh()
    return g


def cell(image, parent, number, selected=False, rifle=False):
    x, y, w, h = BOXES[number]
    g = group(image, 'Slot %d / %s' % (number, 'EXPLORE action' if number == 0 else 'M16 example' if rifle else 'EMPTY'), parent)
    rect(image, g, 'Card / charcoal fill', x, y, w, h, '#14201C', 76)
    outline(image, g, 'Card / selectable edge', (x,y,w,h), ACCENT if selected else '#7F8B85', 2 if selected else 1, 100 if selected else 70)
    if selected:
        rect(image, g, 'Focus / small top marker', x + 9, y, 18, 2, ACCENT)
    key = text(image, g, str(number), 0, y + 5, 15, ACCENT if selected else SOFT, FONT,
               name='Shortcut %d / native editable text' % number)
    key.set_offsets(round(x + w - key.get_width() - 8), round(y + 5))
    if number == 0:
        walking(image, g, x + 31, y + 9, 42)
        centered(image, g, 'EXPLORE', x + w / 2, y + 50, 15, name='Mode / target action, not current state')
    elif rifle:
        imported(image, g, 'M16 / actual inventory silhouette', 'Rifle_PrimaryIcon.png', x + 8, y + 16, 86, 43, WHITE)
        text(image, g, 'M16', x + 8, y + 4, 15, WHITE, name='Item / actual name')
        qty = text(image, g, '17 / 30', 0, y + 47, 16, WHITE, name='Ammo / illustrative current magazine')
        qty.set_offsets(round(x + w - qty.get_width() - 8), round(y + 47))
    else:
        # No repeated EMPTY label: one unobtrusive dash and shortcut are enough.
        line(image, g, x + 45, y + 37, x + 59, y + 37, 1.5, '#A1ABA5', 'Empty / centered dash', 70)
    return g


def arrows(image, parent, selected_direction=None):
    g = group(image, 'Center / arrows only - protected clear space', parent)
    shapes = {
        'up': [(CX-4,CY-18),(CX+4,CY-18),(CX,CY-24)],
        'down': [(CX-4,CY+18),(CX+4,CY+18),(CX,CY+24)],
        'left': [(CX-34,CY-4),(CX-34,CY+4),(CX-40,CY)],
        'right': [(CX+34,CY-4),(CX+34,CY+4),(CX+40,CY)],
    }
    for name, points in shapes.items():
        shape(image, g, 'Direction / ' + name, points, ACCENT if name == selected_direction else '#9BA7A0', 92 if name == selected_direction else 66)
    return g


def state(image, name, populated):
    g = group(image, name)
    for number in range(8):
        cell(image, g, number, selected=(number == (1 if populated else 0)), rifle=populated and number == 1)
    arrows(image, g, 'left')
    centered(image, g, 'QUICK SELECT', CX, 226, 16, SOFT, name='Header / native text')
    details = group(image, 'Focused details / same item name and description', g)
    centered(image, details, 'M16' if populated else 'EXPLORE', CX, 706, 30,
             name='Focused name / native text')
    centered(image, details, 'M16 rifle with a detachable magazine.' if populated else 'Return to exploration.',
             CX, 749, 18, WHITE, BODY, 'Focused description / native text')
    centered(image, g, 'Click  Select     MMB  Assign     Tab  Close', CX, 792, 15,
             SOFT, BODY, 'Controls / current agreed interaction')
    return g


def check_geometry():
    comparisons = []
    for a in range(8):
        ax, ay, aw, ah = BOXES[a]
        for b in range(a+1,8):
            bx, by, bw, bh = BOXES[b]
            ix = min(ax+aw,bx+bw) - max(ax,bx)
            iy = min(ay+ah,by+bh) - max(ay,by)
            assert not (ix > 0 and iy > 0), 'Overlapping slots %d/%d' % (a,b)
            dx = max(bx-(ax+aw), ax-(bx+bw), 0)
            dy = max(by-(ay+ah), ay-(by+bh), 0)
            comparisons.append({'a':a,'b':b,'distance':math.hypot(dx,dy)})
    return {'canvas':[1920,1080], 'center':[CX,CY], 'cell_size':[W,H],
            'boxes':BOXES, 'pair_count':len(comparisons),
            'minimum_gap':min(row['distance'] for row in comparisons),
            'intersections':0, 'arrow_bounds':[CX-40,CY-24,80,48],
            'inner_projected_edge_gap':12, 'arrow_to_card_gap':24,
            'focus_name_y':706,'description_y':749,'controls_y':792}


def build():
    geometry = check_geometry()
    Gimp.context_push()
    Gimp.context_set_antialias(True)
    Gimp.context_set_feather(False)
    image = Gimp.Image.new(1920,1080,Gimp.ImageBaseType.RGB)
    image.undo_disable()
    imported(image, None, '00 / GENERATED SCENE - existing image unchanged', 'montreal_concept_plate.png', 0,0,1920,1080)
    core = quiet(image)
    core.set_name('10 / CORE HUD - existing reference, contextual values')
    for child in core.get_children():
        if child.get_name().startswith('Context /'):
            child.set_visible(False)
    core.set_opacity(62)
    rect(image, None, '15 / MODAL DIM - separate native shape', 0,0,1920,1080,'#07100E',16)
    empty = state(image, '20 / EMPTY BINDINGS - toggle this state', False)
    preview = state(image, '30 / M16 SELECTED - toggle this state', True)
    guides = group(image, '90 / LAYOUT GUIDES - hidden, not game UI')
    outline(image, guides, 'Safe frame', (64,54,1792,972), '#73B7C7',1,65)
    outline(image, guides, 'Central opening / equal projected edge gaps', (CX-64,CY-48,128,96), '#91C89E',1,90)
    for number, box in BOXES.items():
        outline(image, guides, 'Slot %d bounding rectangle' % number, box, '#73B7C7',1,65)
    centered(image, guides, '104 x 72 cells  /  12 px minimum gap  /  no intersections', CX, 858, 17,
             '#91C89E', BODY, 'Geometry / designer annotation')
    guides.set_visible(False)
    empty.set_visible(False)
    png(image, '02_M16_Selected.png')
    preview.set_visible(False)
    empty.set_visible(True)
    png(image, '01_Empty_Bindings.png')
    empty.set_visible(False)
    preview.set_visible(True)
    guides.set_visible(True)
    png(image, '03_Layout_Guides.png')
    guides.set_visible(False)
    image.undo_enable()
    xcf(image, 'CHALK_QuickSelect_v05.xcf')
    report = audit(image)
    (ROOT/'layer_manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    (ROOT/'geometry.json').write_text(json.dumps(geometry,indent=2),encoding='utf-8')
    saved = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,Gio.File.new_for_path(str(ROOT/'CHALK_QuickSelect_v05.xcf')))
    (ROOT/'saved_layer_readback.json').write_text(json.dumps(audit(saved),indent=2),encoding='utf-8')
    saved.delete()
    image.delete()
    Gimp.context_pop()
    print(json.dumps({'status':'saved','root':str(ROOT),'geometry':geometry,'layers':report['counts']}))


try:
    build()
except Exception:
    (ROOT/'sources/error.txt').write_text(traceback.format_exc(),encoding='utf-8')
    raise
