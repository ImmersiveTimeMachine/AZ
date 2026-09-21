# @Description: Migrate exactly eight reviewed L_001 caption overrides as one reversible level edit.
"""Capture both existing disk bytes and current live state; never discard a dirty level.
Only caption fields are changed. Root saves the one returned package separately.
"""
import importlib.util
import json
import shutil
import gc
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
PACKAGE = '/Game/AZ/Maps/L_001'
PREFIX = PACKAGE+'.L_001:PersistentLevel.'
ROWS = (
    ('BPAZ_CommonUI_PickupItem_C_4.BPAZ_Inv_CommonUI_ItemComponent', 'Press E to pick up M16', 'Pick up M16'),
    ('BPAZ_CommonUI_PickupItem_C_2.BPAZ_Inv_CommonUI_ItemComponent', 'Press E to pick up M16', 'Pick up M16'),
    ('AZ_Pistol_Pickup_0.BPAZ_Inv_CommonUI_ItemComponent', 'Press E to pick up pistol', 'Pick up pistol'),
    ('AZ_Pistol_Pickup.BPAZ_Inv_CommonUI_ItemComponent', 'Press E to pick up pistol', 'Pick up pistol'),
    ('BP_AZ_QuestExample_MainUse_C_0', 'Press E — finish TEST panel objective', 'Finish TEST panel objective'),
    ('BP_AZ_QuestExample_SideDelivery_C_0', 'Press E — deliver one TEST sample', 'Deliver one TEST sample'),
    ('BP_AZ_QuestExample_MainOffer_C_0', 'Press E — accept TEST main quest', 'Accept TEST main quest'),
    ('BP_AZ_QuestExample_SideOffer_C_0', 'Press E — accept TEST side quest', 'Accept TEST side quest'),
)


def transforms(u):
    values = {}
    for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
        path = actor.get_path_name()
        if not path.startswith(PREFIX):
            continue
        p, r, scale = actor.get_actor_location(), actor.get_actor_rotation(), actor.get_actor_scale3d()
        values[path] = [p.x, p.y, p.z, r.pitch, r.yaw, r.roll, scale.x, scale.y, scale.z]
    return values


def author():
    spec = importlib.util.spec_from_file_location('fn_world_caption_support',
        ROOT/'Saved/FieldNotesImplementation/WorldInteractionPromptNativeProposal/author_world_prompts.py')
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    s = m.gate(); u = s.ue()
    receipt = m.OUT/'level-caption-batch.json'
    m.require(not receipt.exists(), 'Level caption batch already started; inspect its receipt')
    audit = json.loads((m.OUT/'caption-audit.json').read_text())
    m.require(audit['complete'], 'Finish the declared content audit before migration')
    before = {}
    for name, old, caption in ROWS:
        path = PREFIX+name
        m.require(path in audit['rows'], 'Object missing from the current audit')
        obj = s.resolve(path); row = m._caption_row(s, obj)
        m.require(row['package'] == PACKAGE and row['legacy'] == old and not row['caption'], 'Caption input changed: '+path)
        before[path] = row
    placements = transforms(u)
    m.require(bool(placements), 'The intended editor level is not loaded')
    dirty = {p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_map_packages()}
    folder = m.OUT/'Before'/('LevelCaptions-'+s.stamp()); folder.mkdir(parents=True)
    disk = m.package_file(PACKAGE); shutil.copy2(disk, folder/disk.name)
    state = {'state': 'writing', 'package': PACKAGE, 'backup': str(folder),
             'was_already_dirty': PACKAGE in dirty, 'disk_sha256': s.digest(disk),
             'before': before, 'actor_transforms': placements,
             'preservation': 'Existing live edits retained; no reload, discard or dirty-flag clearing'}
    s.write(folder/'live-before.json', state); s.write(receipt, state)
    after = {}
    for name, old, caption in ROWS:
        path = PREFIX+name; obj = s.resolve(path); row = before[path]
        m._patch(s, obj, {row['field']: caption}, PACKAGE)
        now = m._caption_row(s, s.resolve(path))
        m.require(now['legacy'] == old and now['caption'] == now['resolved_caption'] == caption,
                  'Caption result differs: '+path)
        after[path] = now
    m.require(transforms(u) == placements, 'A level actor or transform changed during caption-only edits')
    state.update(state='authored', after=after, external_save_required=True,
                 actor_transforms_preserved=True, legacy_text_preserved=True)
    s.write(receipt, state); gc.collect()
    return {'package': PACKAGE, 'captions_authored': len(after), 'backup': str(folder),
            'actor_transforms_preserved': True, 'legacy_text_preserved': True, 'external_save_required': True}


def resume_verified():
    """Resume the inspected component-reconstruction stop, accepting only before/desired values."""
    spec = importlib.util.spec_from_file_location('fn_world_caption_support',
        ROOT/'Saved/FieldNotesImplementation/WorldInteractionPromptNativeProposal/author_world_prompts.py')
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    s = m.gate(); u = s.ue(); receipt = m.OUT/'level-caption-batch.json'
    state = json.loads(receipt.read_text())
    m.require(state['state'] == 'writing' and state['package'] == PACKAGE, 'Not the known interrupted level batch')
    comparator = m.module('field_notes_inventory_setup')
    for name, old, caption in ROWS:
        path = PREFIX+name; now = m._caption_row(s, s.resolve(path)); initial = state['before'][path]
        m.require(now['legacy'] == old and now['caption'] in ('', caption), 'Unexpected current caption: '+path)
        expected = comparator.merge(initial['record']['values'], {now['field']: now['caption']})
        m.require(comparator._exact_value(now['record']['values'], expected), 'Unrelated object property changed: '+path)
    m.require(transforms(u) == state['actor_transforms'], 'Level actors changed since caption capture')
    after = {}
    for name, old, caption in ROWS:
        path = PREFIX+name; obj = s.resolve(path); current = m._caption_row(s, obj)
        if current['caption'] != caption:
            m._patch(s, obj, {current['field']: caption}, PACKAGE)
        now = m._caption_row(s, s.resolve(path))
        m.require(now['legacy'] == old and now['caption'] == now['resolved_caption'] == caption, 'Caption readback failed')
        expected = comparator.merge(state['before'][path]['record']['values'], {now['field']: caption})
        m.require(comparator._exact_value(now['record']['values'], expected), 'Unrelated object change after reconstruction')
        after[path] = now
    m.require(transforms(u) == state['actor_transforms'], 'An actor transform changed during caption updates')
    state.update(state='authored', after=after, external_save_required=True,
                 actor_transforms_preserved=True, legacy_text_preserved=True,
                 recovery='Reacquired replacement Blueprint components after PostEditChange')
    s.write(receipt, state); gc.collect()
    return {'package': PACKAGE, 'captions_authored': len(after), 'actor_transforms_preserved': True,
            'legacy_text_preserved': True, 'external_save_required': True}
