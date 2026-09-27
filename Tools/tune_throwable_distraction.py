"""Apply the approved first-pass AI distraction hierarchy; no gameplay or PIE control."""
from pathlib import Path
from datetime import datetime, timezone
import json
import shutil
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
BASE = '/Game/AZ/Blueprints/Throwables/'
# UE hearing multiplies BOTH the listener range and event MaxRange by loudness.
# With the current 700cm listener: stone ~7m, shatter ~14m, explosion ~28m.
# These are AI stimuli, independent of audible sound volume and damage.
VALUES = {
    'Stone': {'impact_loudness': 1.0, 'impact_hearing_range': 700.0},
    'Bottle': {'impact_loudness': 1.0, 'impact_hearing_range': 1000.0, 'shatter_loudness': 2.0},
    'Incendiary': {'impact_loudness': 1.0, 'impact_hearing_range': 1000.0, 'shatter_loudness': 2.0},
    'Grenade': {'impact_loudness': 1.0, 'impact_hearing_range': 700.0,
                'detonation_loudness': 4.0, 'detonation_hearing_range': 1500.0},
}


def apply():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor.get_game_world():
        raise RuntimeError('PIE active; no asset changes made')
    dirty = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    assets = {n: unreal.load_asset(BASE + 'DA_Throwable_' + n) for n in VALUES}
    for n, a in assets.items():
        if not a or a.get_outermost().get_name() in dirty:
            raise RuntimeError('Missing or already dirty target: ' + n)
    before = {n: {k: a.get_editor_property(k) for k in VALUES[n]} for n, a in assets.items()}
    receipt_dir = ROOT / 'Saved/ThrowableCompletion' / ('distraction-' + datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S'))
    receipt_dir.mkdir(parents=True, exist_ok=False)
    for n in VALUES:
        disk = ROOT / 'Content/AZ/Blueprints/Throwables' / ('DA_Throwable_' + n + '.uasset')
        shutil.copy2(disk, receipt_dir / disk.name)
    (receipt_dir / 'before.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
    for n, a in assets.items():
        for k, v in VALUES[n].items():
            a.set_editor_property(k, v)
    if not unreal.EditorLoadingAndSavingUtils.save_packages([a.get_outermost() for a in assets.values()], False):
        raise RuntimeError('Target package save failed; backups: ' + str(receipt_dir))
    after = {n: {k: a.get_editor_property(k) for k in VALUES[n]} for n, a in assets.items()}
    if after != VALUES:
        raise RuntimeError('Readback differs from requested tuning')
    (receipt_dir / 'after.json').write_text(json.dumps(after, indent=2), encoding='utf-8')
    print(json.dumps({'saved': after, 'receipt': str(receipt_dir)}))


if __name__ == '__main__':
    apply()
