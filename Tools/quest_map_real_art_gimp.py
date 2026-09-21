"""GIMP-native tonal treatment of the calibrated L_001 capture. No geometric edits.

Execute with GIMP 3's python-fu-eval batch interpreter, after root has completed
the fresh capture. All pixel editing uses GIMP/GEGL; no Pillow or external raster
processing. The unmodified source is retained both on disk and as a hidden layer.
"""
from gi.repository import Gimp, Gio, Gegl
from pathlib import Path
import hashlib
import json
import struct

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'UI Design/CHALK_QuestMap_v01/sources/map'
SOURCE = OUT / 'L001_FinalColor.png'
CAPTURE = ROOT / 'Saved/QuestMapImplementation/map-capture-final.json'
XCF = OUT / 'CHALK_L001_FieldJournal_NATIVE.xcf'
PNG = OUT / 'T_CHALK_Map_L001.png'
RECEIPT = OUT / 'map-art-receipt.json'

# Native GIMP luminance curve: lift shadow geometry, compress bright props so
# runtime peach/white navigation markers retain contrast. No thresholding or
# retouching changes the capture's actual building/obstacle silhouettes.
CURVE = [0.0, 0.075, 0.03, 0.13, 0.08, 0.21, 0.18, 0.33,
         0.38, 0.48, 0.75, 0.62, 1.0, 0.70]
SAGE = '#778879'
GRAIN_OPACITY = 4.0


def hash_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def solid(image, parent, name, value, mode=Gimp.LayerMode.NORMAL, opacity=100.0):
    layer = Gimp.Layer.new(image, name, 2048, 2048, Gimp.ImageType.RGBA_IMAGE, opacity, mode)
    image.insert_layer(layer, parent, 0)
    layer.set_offsets(0, 0)
    Gimp.context_set_foreground(Gegl.Color.new(value))
    assert layer.fill(Gimp.FillType.FOREGROUND)
    return layer


def layers_readback(parent):
    return [{'name': item.get_name(), 'visible': item.get_visible(), 'opacity': item.get_opacity(),
             'width': item.get_width(), 'height': item.get_height(), 'offsets': list(item.get_offsets()),
             'mode': int(item.get_mode()), 'children': layers_readback(item) if item.is_group() else []}
            for item in (parent.get_layers() if isinstance(parent, Gimp.Image) else parent.get_children())]


def run():
    capture = json.loads(CAPTURE.read_text(encoding='utf-8'))
    assert capture['completed'] and capture['temporary_actor_removed'], 'Fresh capture was not completed/cleaned up.'
    assert capture['origin'] == [-1435, 3620, 0] and capture['size'] == [12400, 12400], 'Capture calibration differs.'
    before = hash_file(SOURCE)
    signature = SOURCE.read_bytes()[:24]
    assert signature[:8] == b'\x89PNG\r\n\x1a\n' and struct.unpack('>II', signature[16:24]) == (2048, 2048)
    Gimp.context_push()
    image = None
    try:
        image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(str(SOURCE)))
        assert image and image.get_width() == 2048 and image.get_height() == 2048
        image.undo_disable()
        original = image.get_layers()[0]
        original.set_name('00 / SOURCE — L_001 FinalColor, unmodified')
        original.set_visible(False)
        solid(image, None, '01 / BACKGROUND — opaque dark sage', '#101817')
        presentation = Gimp.GroupLayer.new(image, '10 / FIELD JOURNAL — exact captured geometry')
        image.insert_layer(presentation, None, 0)
        working = original.copy()
        working.set_name('11 / GEOMETRY — luminance and lifted-shadow curve')
        image.insert_layer(working, presentation, 0)
        working.set_visible(True)
        working.set_offsets(0, 0)
        assert working.desaturate(Gimp.DesaturateMode.LUMINANCE)
        assert working.curves_spline(Gimp.HistogramChannel.VALUE, CURVE)
        solid(image, presentation, '12 / SAGE — editable hue, original luminance retained', SAGE, Gimp.LayerMode.HSL_COLOR)
        grain = solid(image, presentation, '13 / CHALK GRAIN — optional, 4 percent', '#808080', Gimp.LayerMode.SOFTLIGHT, GRAIN_OPACITY)
        noise = Gimp.DrawableFilter.new(grain, 'gegl:noise-rgb', 'Subtle monochrome paper grain')
        config = noise.get_config()
        config.set_property('independent', False)
        config.set_property('correlated', False)
        config.set_property('linear', False)
        config.set_property('gaussian', True)
        for channel in ('red', 'green', 'blue'):
            config.set_property(channel, 0.035)
        config.set_property('alpha', 0.0)
        config.set_property('seed', 190926)
        noise.update()
        grain.append_filter(noise)
        image.undo_enable()
        Gimp.displays_flush()
        assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, image, Gio.File.new_for_path(str(XCF)))
        proc = Gimp.get_pdb().lookup_procedure('file-png-export')
        config = proc.create_config()
        config.set_property('run-mode', Gimp.RunMode.NONINTERACTIVE)
        config.set_property('image', image)
        config.set_property('file', Gio.File.new_for_path(str(PNG)))
        assert proc.run(config).index(0) == Gimp.PDBStatusType.SUCCESS
        assert before == hash_file(SOURCE), 'Source capture changed unexpectedly.'
        output_signature = PNG.read_bytes()[:26]
        assert struct.unpack('>II', output_signature[16:24]) == (2048, 2048)
        receipt = {'completed': True, 'source': str(SOURCE), 'source_sha256': before,
                   'source_preserved': True, 'xcf': str(XCF), 'png': str(PNG), 'dimensions': [2048, 2048],
                   'png_color_type': output_signature[25], 'calibration': capture,
                   'world_cm_per_pixel': 12400.0 / 2048, 'curve': CURVE, 'sage': SAGE,
                   'grain_opacity': GRAIN_OPACITY, 'geometric_edits': [], 'baked_markers_or_lore': False,
                   'editing_api': 'GIMP3 native drawable operations + editable GEGL grain filter and tone layers',
                   'layers': layers_readback(image)}
        RECEIPT.write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    except Exception as error:
        (OUT / 'map-art-error.json').write_text(json.dumps({'error': str(error)}, indent=2), encoding='utf-8')
        raise
    finally:
        if image:
            image.delete()
        Gimp.context_pop()
        Gimp.displays_flush()


run()
