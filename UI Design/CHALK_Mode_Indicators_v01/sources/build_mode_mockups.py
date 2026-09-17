"""Author three CHALK Fight/Explore proposals with native GIMP text and paths.

Run in GIMP's python-fu-eval interpreter. Reuses approved project artwork and
font helpers; does not modify Unreal content or any prior GIMP document.
"""
from pathlib import Path
import ast
import json
import traceback
from gi.repository import Gimp, Gio

PROJECT = Path('C:/UnrealEngine/Games/AZ')
BASE = PROJECT / 'UI Design/CHALK_HUD_v03/sources/build_native_hud.py'
tree = ast.parse(BASE.read_text(encoding='utf-8'))
tree.body = [n for n in tree.body if not isinstance(n, ast.Try)]
exec(compile(tree, str(BASE), 'exec'), globals())
ROOT = PROJECT / 'UI Design/CHALK_Mode_Indicators_v01'
(ROOT / 'sources').mkdir(parents=True, exist_ok=True)
WHITE, SOFT, PEACH = '#EEEAE0', '#A8B0AD', '#FFBA8C'
VARIANTS = [
    ('01', 'Equipment row', 'Large symbol and a clear mode name.',
     'Closest to the existing weapon display.'),
    ('02', 'Compact status', 'Small symbol and name tucked above health.',
     'The quietest footprint during exploration.'),
    ('03', 'Two-symbol strip', 'Both modes remain visible; underline marks the active one.',
     'A steady visual reference when switching with 0.'),
]


def log(message):
    with (ROOT / 'sources/build_progress.log').open('a', encoding='utf-8') as f:
        f.write(message + '\n')


def save(img, name, procedure):
    proc = Gimp.get_pdb().lookup_procedure(procedure)
    cfg = proc.create_config()
    cfg.set_property('run-mode', Gimp.RunMode.NONINTERACTIVE)
    cfg.set_property('image', img)
    cfg.set_property('file', Gio.File.new_for_path(str(ROOT / name)))
    result = proc.run(cfg)
    if result.index(0) != Gimp.PDBStatusType.SUCCESS:
        raise RuntimeError('Save failed: ' + name + ' ' + str(result.index(0)))
    log('Saved ' + name)


def save_xcf_verified(img, name):
    img.undo_enable()
    save(img, name, 'gimp-xcf-save')
    reopened = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,
                             Gio.File.new_for_path(str(ROOT / name)))
    report = audit(reopened)
    if report['counts']['text'] == 0 or report['counts']['vector'] == 0:
        raise RuntimeError('Native layers missing in ' + name)
    if report['path_count'] != report['counts']['vector']:
        raise RuntimeError('Vector/path mismatch in ' + name)
    (ROOT / (Path(name).stem + '_layers.json')).write_text(
        json.dumps(report, indent=2), encoding='utf-8')
    reopened.delete()
    return report['counts']


def label(img, parent, value, x, y, size, tint=WHITE, body=False, name=None):
    return text(img, parent, value, x, y, size, tint,
                BODY if body else FONT, name=name or 'Text / ' + value)


def background(img, parent=None, crop=None, dest=None):
    """Only the reused background is raster; all HUD elements remain native."""
    item = Gimp.file_load_layer(Gimp.RunMode.NONINTERACTIVE, img,
        Gio.File.new_for_path(str(SRC / 'montreal_concept_plate.png')))
    img.insert_layer(item, parent, 0)
    item.set_name('GENERATED SCENE / existing Montreal reference image')
    item.scale(1920, 1080, False)
    if crop:
        x, y, w, h = crop
        item.resize(w, h, -x, -y)
    if dest:
        x, y, w, h = dest
        item.scale(round(w), round(h), False)
        item.set_offsets(round(x), round(y))
    item.set_lock_content(True)
    return item


def glyph(img, parent, mode, x, y, size, tint=WHITE, opacity=100):
    """Reuse the six native vector parts of each approved quick-select glyph."""
    g = group(img, mode + ' / existing editable symbol', parent)
    g.set_opacity(opacity)
    before = {p.get_id() for p in img.get_paths()}
    if mode == 'FIGHT':
        source = ast.parse((PROJECT / 'Tools/quick_select_fist_art.py').read_text(encoding='utf-8'))
        nodes = []
        for node in source.body:
            if isinstance(node, ast.FunctionDef) and node.name == 'contour':
                # The original standalone glyph has no parent group.
                for call in ast.walk(node):
                    if isinstance(call, ast.Call) and isinstance(call.func, ast.Name) and call.func.id == 'vec':
                        call.args[1] = ast.Name(id='glyph_parent', ctx=ast.Load())
                nodes.append(node)
            elif isinstance(node, ast.Expr) and isinstance(node.value, ast.Call) and isinstance(node.value.func, ast.Name) and node.value.func.id == 'contour':
                nodes.append(node)
            elif isinstance(node, ast.For) and isinstance(node.iter, ast.List) and isinstance(node.target, ast.Tuple) and len(node.target.elts) == 3:
                for call in ast.walk(node):
                    if isinstance(call, ast.Call) and isinstance(call.func, ast.Name) and call.func.id == 'line':
                        call.args[1] = ast.Name(id='glyph_parent', ctx=ast.Load())
                nodes.append(node)
        class Tint(ast.NodeTransformer):
            def visit_Constant(self, node):
                return ast.copy_location(ast.Constant(tint), node) if node.value == '#FFFFFF' else node
        module = Tint().visit(ast.Module(body=nodes, type_ignores=[]))
    else:
        source = ast.parse((PROJECT / 'Tools/quick_select_explore_art.py').read_text(encoding='utf-8'))
        nodes = []
        for node in source.body:
            if isinstance(node, ast.Expr) and isinstance(node.value, ast.Call) and isinstance(node.value.func, ast.Name) and node.value.func.id in ('shape', 'ellipse'):
                node.value.args[1] = ast.Name(id='glyph_parent', ctx=ast.Load())
                nodes.append(node)
        module = ast.Module(body=nodes, type_ignores=[])
    scope = dict(globals(), img=img, glyph_parent=g, white=tint)
    exec(compile(ast.fix_missing_locations(module), '<approved native glyph>', 'exec'), scope)
    for path in img.get_paths():
        if path.get_id() not in before:
            for stroke in path.get_strokes():
                path.stroke_scale(stroke, size / 128, size / 128)
                path.stroke_translate(stroke, x, y)
    for child in g.get_children():
        if isinstance(child, Gimp.VectorLayer):
            child.refresh()
    return g


def hud(img, parent, variant, mode, origin=(0, 0), scale=1):
    """Positions are anchored to the approved 1920x1080 HUD baseline."""
    ox, oy = origin
    def p(x, y):
        return ox + x * scale, oy + y * scale
    def icon(state, x, y, size, opacity=100):
        return glyph(img, mode_group, state, *p(x, y), size * scale, opacity=opacity)
    def txt(value, x, y, size, tint=WHITE):
        return label(img, mode_group, value, *p(x, y), size * scale, tint,
                     name='Current mode / editable ' + value)
    def segment(x1, y1, x2, y2, width, tint, name, opacity=100):
        return line(img, mode_group, *p(x1, y1), *p(x2, y2), width * scale, tint, name, opacity)

    g = group(img, mode + ' / CURRENT state / option ' + variant, parent)
    # Same health geometry and value in every state; mode is not a vitals warning.
    vitals = group(img, 'Health / preserved baseline / illustrative 72 percent', g)
    heart(img, vitals, *p(1512, 982), 20 * scale, WHITE)
    bar(img, vitals, *p(1547, 987), 240 * scale, 10 * scale, .72, value=WHITE)
    mode_group = group(img, 'Mode indicator / replaces empty weapon area', g)
    if variant == '01':
        icon(mode, 1506, 900, 70)
        txt(mode, 1592, 920, 27)
    elif variant == '02':
        icon(mode, 1509, 936, 35)
        txt(mode, 1559, 939, 23)
        # Existing inventory peach, restrained to a small non-critical accent.
        segment(1559, 971, 1581, 971, 2, PEACH if mode == 'FIGHT' else SOFT,
                'Status / short accent rule')
    else:
        icon('FIGHT', 1507, 924, 42, 100 if mode == 'FIGHT' else 32)
        icon('EXPLORE', 1563, 924, 42, 100 if mode == 'EXPLORE' else 32)
        segment(1555, 934, 1555, 958, 1, SOFT, 'Mode / separator', 45)
        active_x = 1515 if mode == 'FIGHT' else 1571
        segment(active_x, 971, active_x + 25, 971, 2, PEACH,
                'Current mode / underline / non-color state cue')
        txt(mode, 1625, 934, 23)
    return g


def corner(img, parent, variant, mode, x, y, scale=1.4):
    # This crop avoids the illustrative character/weapon pose in the old plate.
    crop = (1430, 835, 450, 210)
    w, h = crop[2] * scale, crop[3] * scale
    g = group(img, mode + ' / enlarged bottom-right HUD detail', parent)
    background(img, g, crop, (x, y, w, h))
    hud(img, g, variant, mode, (x - crop[0] * scale, y - crop[1] * scale), scale)
    return g


def board_chrome(img, title, subtitle):
    bg = group(img, '00 / REVIEW BOARD / not gameplay UI')
    rect(img, bg, 'Canvas / charcoal', 0, 0, img.get_width(), img.get_height(), '#111817')
    label(img, bg, 'CHALK', 64, 32, 28, SOFT)
    label(img, bg, title, 64, 79, 45)
    label(img, bg, subtitle, 66, 145, 18, SOFT, True)
    return bg


def individual(variant, title, description, note):
    img = Gimp.Image.new(1920, 1080, Gimp.ImageBaseType.RGB)
    img.undo_disable()
    background(img)
    fight = hud(img, None, variant, 'FIGHT')
    fight.set_name('20 / FULL FRAME / FIGHT / show only one state')
    explore = hud(img, None, variant, 'EXPLORE')
    explore.set_name('21 / FULL FRAME / EXPLORE / show only one state')
    explore.set_visible(False)
    save(img, variant + '_Fight_FullFrame.png', 'file-png-export')
    fight.set_visible(False)
    explore.set_visible(True)
    save(img, variant + '_Explore_FullFrame.png', 'file-png-export')
    explore.set_visible(False)

    review = board_chrome(img, variant + ' / ' + title.upper(), description + ' ' + note)
    review.set_name('90 / TWO-STATE REVIEW / hide to see full-frame states')
    label(img, review, 'FIGHT / UNARMED', 120, 238, 25)
    label(img, review, 'EXPLORE / HANDS LOWERED', 1020, 238, 25)
    corner(img, review, variant, 'FIGHT', 100, 289, 1.6)
    corner(img, review, variant, 'EXPLORE', 1000, 289, 1.6)
    label(img, review, 'AT GAME SIZE / 1920 x 1080 reference', 120, 695, 17, SOFT, True)
    label(img, review, 'AT GAME SIZE / 1920 x 1080 reference', 1020, 695, 17, SOFT, True)
    corner(img, review, variant, 'FIGHT', 110, 733, 1)
    corner(img, review, variant, 'EXPLORE', 1010, 733, 1)
    label(img, review, '0 switches mode. The HUD names the current state.', 120, 1002, 18, SOFT, True)
    label(img, review, 'Health stays in place. Weapon/ammo returns when a firearm is equipped.',
          1020, 1002, 16, SOFT, True)
    save(img, variant + '_Review.png', 'file-png-export')
    counts = save_xcf_verified(img, 'CHALK_Mode_' + variant + '_NATIVE.xcf')
    img.delete()
    return counts


def comparison():
    img = Gimp.Image.new(1920, 1260, Gimp.ImageBaseType.RGB)
    img.undo_disable()
    chrome = board_chrome(img, 'FIGHT / EXPLORE',
        'Three HUD proposals / same health baseline / existing CHALK symbols, type and palette')
    label(img, chrome, 'CURRENT: FIGHT', 580, 193, 23)
    label(img, chrome, 'CURRENT: EXPLORE', 1240, 193, 23)
    for index, (variant, title, description, note) in enumerate(VARIANTS):
        y = 244 + index * 310
        row = group(img, variant + ' / ' + title.upper())
        label(img, row, variant, 64, y + 31, 29, PEACH)
        label(img, row, title.upper(), 64, y + 79, 29)
        # Deliberate line breaks retain legibility without long design prose.
        summaries = {
            '01': ('Symbol + name', 'Closest to the weapon row'),
            '02': ('Small symbol + name', 'Least space above health'),
            '03': ('Both symbols + active underline', 'Visible relationship between modes'),
        }
        for j, line_text in enumerate(summaries[variant]):
            label(img, row, line_text, 66, y + 129 + j * 28, 17, SOFT, True)
        corner(img, row, variant, 'FIGHT', 550, y, 1.35)
        corner(img, row, variant, 'EXPLORE', 1210, y, 1.35)
        if index < 2:
            line(img, row, 64, y + 296, 1820, y + 296, 1, '#34413C', 'Review / row separator')
    label(img, chrome, '0 switches mode. HUD = current state. Quick select = destination action.',
          66, 1194, 18, SOFT, True)
    save(img, 'CHALK_Mode_Comparison.png', 'file-png-export')
    counts = save_xcf_verified(img, 'CHALK_Mode_Comparison_NATIVE.xcf')
    img.delete()
    return counts


def main():
    Gimp.context_push()
    Gimp.context_set_antialias(True)
    Gimp.context_set_feather(False)
    summary = {}
    for variant in VARIANTS:
        log('Building ' + variant[0])
        summary[variant[0]] = individual(*variant)
    summary['comparison'] = comparison()
    Gimp.context_pop()
    (ROOT / 'saved-file-receipt.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps({'status': 'saved and reopened', 'files': summary}))
    log('DONE')


try:
    main()
except Exception:
    log(traceback.format_exc())
    raise
