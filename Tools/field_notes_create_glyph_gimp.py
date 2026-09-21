"""Native GIMP Create keycap in the existing S/O controller-set visual language."""
from pathlib import Path
import importlib.util
import json
import math
from gi.repository import Gimp

PROJECT = Path('C:/UnrealEngine/Games/AZ')
spec = importlib.util.spec_from_file_location('fn_input_art', PROJECT/'Tools/field_notes_art_gimp.py')
A = importlib.util.module_from_spec(spec); spec.loader.exec_module(A)
A.ROOT = PROJECT/'UI Design/CHALK_FieldNotes_Input_v01'
A.OUT = A.ROOT/'unreal-art'; A.OUT.mkdir(parents=True, exist_ok=True)


def capsule(radius):
    result = []
    for cx, cy, start in ((32, 16, math.pi), (32, 48, 0)):
        for n in range(33):
            angle = start + n*math.pi/32
            result.append((cx+radius*math.cos(angle), cy+radius*math.sin(angle)))
    return result


Gimp.context_push()
try:
    Gimp.context_set_antialias(True); Gimp.context_set_feather(False)
    image = Gimp.Image.new(64, 64, Gimp.ImageBaseType.RGB)
    group = A.group(image, 'Create / C initial matching existing Share S and Options O keycaps')
    A.contours(image, group, 'White capsule perimeter', (capsule(16), capsule(12)))
    A.label(image, group, 'C', 32, 28, 24)
    result = A.save_native(image, 'T_FN_PS5_Create')
    result.update(channel_readback=A.png_stats(Path(result['png'])), source_sha256=A.sha(Path(result['png'])),
                  asset='/Game/AZ/Blueprints/Menu/Style/FieldNotes/Art/T_FN_PS5_Create',
                  semantics='Create keycap C, using the existing pack S/O initial-letter convention; not an official logo reproduction')
    (A.ROOT/'create-glyph-manifest.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print('Native Create glyph saved and reopened: '+result['png'])
finally:
    Gimp.context_pop()
