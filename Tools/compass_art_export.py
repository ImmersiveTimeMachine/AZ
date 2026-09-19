"""Run in GIMP 3: native editable symbols from the approved CHALK HUD design."""
from gi.repository import Gimp, Gio, Gegl
from pathlib import Path
import ast
import json

project = Path('C:/UnrealEngine/Games/AZ')
source = project / 'UI Design/CHALK_HUD_v03/sources/build_native_hud.py'
definitions = ast.parse(source.read_text(encoding='utf-8'))
definitions.body = [n for n in definitions.body if not isinstance(n, ast.Try)]
exec(compile(definitions, str(source), 'exec'), globals())
destination = project / 'UI Design/CHALK_HUD_v03/unreal-art/Compass'
destination.mkdir(parents=True, exist_ok=True)
receipts = []


def export_png(img, filename):
    proc = Gimp.get_pdb().lookup_procedure('file-png-export')
    config = proc.create_config()
    config.set_property('run-mode', Gimp.RunMode.NONINTERACTIVE)
    config.set_property('image', img)
    config.set_property('file', Gio.File.new_for_path(str(filename)))
    result = proc.run(config)
    if result.index(0) != Gimp.PDBStatusType.SUCCESS:
        raise RuntimeError('PNG export failed: ' + str(filename))


Gimp.context_push()
try:
    for kind in ('Pointer', 'Target'):
        img = Gimp.Image.new(128, 128, Gimp.ImageBaseType.RGB)
        if kind == 'Pointer':
            shape(img, None, 'Approved downward pointer / native vector',
                  [(24, 40), (104, 40), (64, 88)], '#FFFFFF')
        else:
            shape(img, None, 'Diamond outer contour / native vector',
                  [(64, 8), (120, 64), (64, 120), (8, 64)], '#FFFFFF')
            hole = shape(img, None, 'Diamond hollow / native vector erase',
                         [(64, 32), (96, 64), (64, 96), (32, 64)], '#FFFFFF')
            hole.set_mode(Gimp.LayerMode.ERASE)
            rect(img, None, 'Centre dot / native vector', 52, 52, 24, 24, '#FFFFFF')
        xcf = destination / ('CHALK_Compass_' + kind + '_NATIVE.xcf')
        if not Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, img, Gio.File.new_for_path(str(xcf))):
            raise RuntimeError('XCF save failed')
        png = destination / ('T_CHALK_Compass' + kind + '.png')
        export_png(img, png)
        receipts.append({'png': str(png), 'xcf': str(xcf), 'size': [128, 128],
                         'source': str(source), 'native_editable_vectors': True})
        img.delete()
    # A new native strip uses the pack's same one-revolution UV contract. The
    # existing material's -0.00275 phase correction is included in glyph centres.
    strip = Gimp.Image.new(2880, 128, Gimp.ImageBaseType.RGB)
    ticks = group(strip, 'Ticks / native vector paths')
    labels = group(strip, 'Directions / editable Oswald text')
    directions = {0: 'N', 45: 'NE', 90: 'E', 135: 'SE', 180: 'S', 225: 'SW', 270: 'W', 315: 'NW'}
    north_x = 2880 * (0.5 - 0.00275)
    for degrees in range(0, 360, 5):
        origin = (north_x + degrees * 8) % 2880
        for x in (origin - 2880, origin, origin + 2880):
            if -50 <= x <= 2930:
                major = degrees % 45 == 0
                line(strip, ticks, x, 76, x, 98 if major else 88,
                     2.8 if major else 2, '#FFFFFF' if major else '#B9B9B9',
                     'Tick %03d at %.2f' % (degrees, x))
                if major:
                    center_label(strip, labels, directions[degrees], x, 8, 46, '#FFFFFF', 'Oswald Light')
    strip_xcf = destination / 'CHALK_Compass_Strip_NATIVE.xcf'
    if not Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, strip, Gio.File.new_for_path(str(strip_xcf))):
        raise RuntimeError('Strip XCF save failed')
    strip_png = destination / 'T_CHALK_CompassStrip.png'
    export_png(strip, strip_png)
    receipts.append({'png': str(strip_png), 'xcf': str(strip_xcf), 'size': [2880, 128],
                     'period_degrees': 360, 'shader_phase': -0.00275, 'render_size': [1440, 64],
                     'visible_window': [600, 64], 'native_editable_vectors_and_text': True})
    strip.delete()
    # Inspection-only composite: makes the source strip's white alpha glyphs
    # readable. It is not imported over or used to modify the source texture.
    sample = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,
                           Gio.File.new_for_path(str(project / 'Saved/CompassIntegration/source-compass-strip.png')))
    background = Gimp.Layer.new(sample, 'Inspection background', sample.get_width(), sample.get_height(),
                                Gimp.ImageType.RGBA_IMAGE, 100, Gimp.LayerMode.NORMAL)
    sample.insert_layer(background, None, len(sample.get_layers()))
    Gimp.context_set_foreground(Gegl.Color.new('#101515'))
    background.fill(Gimp.FillType.FOREGROUND)
    export_png(sample, project / 'Saved/CompassIntegration/source-compass-strip-inspection.png')
    sample.delete()
    (destination / 'export-receipt.json').write_text(json.dumps(receipts, indent=2), encoding='utf-8')
finally:
    Gimp.context_pop()
    Gimp.displays_flush()
