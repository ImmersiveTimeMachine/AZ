"""GIMP-only additive Quick frame export; preserves the eight-art manifest."""
from pathlib import Path
import importlib.util
import json
from gi.repository import Gimp

ROOT = Path('C:/UnrealEngine/Games/AZ')
spec = importlib.util.spec_from_file_location('fn_art', ROOT/'Tools/field_notes_art_gimp.py')
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)


def run():
    original = A.sha(A.ROOT/'production-manifest.json')
    Gimp.context_push()
    try:
        Gimp.context_set_antialias(False)
        Gimp.context_set_feather(False)
        image = Gimp.Image.new(104, 72, Gimp.ImageBaseType.RGB)
        image.undo_disable()
        layer = A.group(image, 'Native assignment frame / white tintable 2px perimeter')
        for name, x, y, w, h in (('Top',0,0,104,2), ('Bottom',0,70,104,2),
                                  ('Left',0,2,2,68), ('Right',102,2,2,68)):
            A.rect(image, layer, name, x,y,w,h, '#FFFFFF')
        result = A.save_native(image, 'T_FN_QuickFrame')
        png = Path(result['png'])
        result['channel_readback'] = A.png_stats(png)
        result['source_sha256'] = A.sha(png)
        result['asset'] = '/Game/AZ/Blueprints/Menu/Style/FieldNotes/Art/T_FN_QuickFrame'
        result['original_production_manifest_unchanged'] = original == A.sha(A.ROOT/'production-manifest.json')
        stats = result['channel_readback']
        assert stats['size'] == [104,72] and stats['alpha_range'] == [0,255]
        assert stats['visible_nonwhite_pixels'] == 0 and stats['visible_pixels'] == 688
        assert result['original_production_manifest_unchanged']
        (A.ROOT/'quick-frame-manifest.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        print('Quick frame exported and native XCF reopened: '+str(png))
    finally:
        Gimp.context_pop()


run()
