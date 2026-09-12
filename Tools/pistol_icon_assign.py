"""Assign existing pistol ammunition artwork while preserving all item data.

Only icon fields on pistol magazine manifests change. The existing pistol texture
is reimported from its editable native artwork. No combat behavior is modified.
"""
import gc
import json
import runpy
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/PistolIcons'
AMMO = '/Game/InventorySystemPro/ExampleContent/Common/Art/Ammo9mm/T_Ammo9mm'
PISTOL_ICON = '/Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Pistol'
ITEM_ROOT = '/Game/AZ/Blueprints/Items/Equippables/Weapons/Pistol/'
NAMES = ['BP_Pickup_Pistol', 'BP_Pickup_PistolMagazine',
         'BP_Pickup_PistolMagazine_Partial', 'BP_Pickup_PistolMagazine_Empty']
ACTORS = ['AZ_Pistol_Pickup', 'AZ_Pistol_Magazine_Full',
          'AZ_Pistol_Magazine_Partial', 'AZ_Pistol_Magazine_Empty']


def patch(H, text):
    top = H['fields'](text)
    if dict(top).get('ItemTypeTag') != '(TagName="Item.Type.Magazine.Pistol")':
        return text
    fragments, count = [], 0
    for entry in H['split_top_level'](H['parenthesized'](dict(top)['Fragments'])):
        kind, fields = H['fragment_parts'](entry)
        if kind == '/Script/AZ.AZ_Inv_CommonUI_ImageFragment' and dict(fields).get('FragmentTag') == '(TagName="Item.Fragment.Icon")':
            fields = H['replace_field'](fields, 'Icon', H['object_reference']('/Script/Engine.Texture2D', AMMO))
            fields = H['replace_field'](fields, 'IconDimensions', '(X=64.000000,Y=64.000000)')
            entry = kind + H['encode_fields'](fields)
            count += 1
        fragments.append(entry)
    assert count == 1, 'Expected exactly one pistol magazine icon fragment'
    return H['encode_fields'](H['replace_field'](top, 'Fragments', '(' + ','.join(fragments) + ')'))


def main(mode='verify'):
    try:
        assert mode in ('author', 'verify')
        assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None, 'Stop Play before editor icon assignment/readback'
        H = runpy.run_path(str(ROOT / 'Tools/rifle_p01_activate.py'), run_name='pistol_icon_helpers')
        blueprints = [unreal.load_asset(ITEM_ROOT + name) for name in NAMES]
        assert all(blueprints)
        entries = [H['item_component_template'](bp) for bp in blueprints]
        for name in ACTORS:
            actor = unreal.load_object(None, '/Game/AZ/Maps/L_001.L_001:PersistentLevel.' + name)
            assert actor
            entries.append(actor.get_component_by_class(unreal.AZ_Inv_CommonUI_ItemComponent))
        assert all(entries) and unreal.load_asset(AMMO)
        before = [{'component': c.get_path_name(), 'manifest': c.get_editor_property('pickup_item_manifest').export_text(),
                   'contained': [m.export_text() for m in c.get_editor_property('initial_contained_item_manifests')]}
                  for c in entries]
        OUT.mkdir(parents=True, exist_ok=True)
        if mode == 'author':
            packages = [ITEM_ROOT + name for name in NAMES] + ['/Game/AZ/Maps/L_001', PISTOL_ICON]
            dirty = {p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
            dirty.update(p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
            assert not dirty.intersection(packages), 'Target packages have unsaved changes'
            backup = ROOT / 'Saved/Backups/PistolIcons' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
            backup.mkdir(parents=True)
            for p in packages:
                file = ROOT / 'Content' / (p.removeprefix('/Game/') + ('.umap' if p.endswith('/L_001') else '.uasset'))
                shutil.copy2(file, backup / file.name)
            (backup / 'manifests-before.json').write_text(json.dumps(before, indent=2))
            for bp in blueprints:
                bp.modify()
            for c, record in zip(entries, before):
                root_text = patch(H, record['manifest'])
                children = [patch(H, text) for text in record['contained']]
                if root_text == record['manifest'] and children == record['contained']:
                    continue
                c.modify()
                manifest = unreal.AZ_Inv_CommonUI_ItemManifest()
                assert manifest.import_text(root_text)
                c.set_editor_property('pickup_item_manifest', manifest, notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
                values = []
                for text in children:
                    child = unreal.AZ_Inv_CommonUI_ItemManifest()
                    assert child.import_text(text)
                    values.append(child)
                c.set_editor_property('initial_contained_item_manifests', values, notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
                assert c.get_editor_property('pickup_item_manifest').export_text() == root_text
                assert [m.export_text() for m in c.get_editor_property('initial_contained_item_manifests')] == children
            task = unreal.AssetImportTask()
            for k, v in {'filename': str(ROOT / 'UI Design/CHALK_HUD_v03/unreal-art/T_HUD_Pistol.png'),
                         'destination_path': '/Game/AZ/Blueprints/Menu/HUD/Art', 'destination_name': 'T_HUD_Pistol',
                         'automated': True, 'replace_existing': True, 'save': False}.items():
                task.set_editor_property(k, v)
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            texture = unreal.load_asset(PISTOL_ICON)
            for k, v in {'compression_settings': unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                         'lod_group': unreal.TextureGroup.TEXTUREGROUP_UI, 'mip_gen_settings': unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                         'srgb': True, 'never_stream': True}.items():
                texture.set_editor_property(k, v)
            assert unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
            report = {'mode': mode, 'backup': str(backup), 'before': before,
                      'compile_after_return': [ITEM_ROOT + n + '.' + n for n in NAMES]}
        else:
            for r in before:
                assert patch(H, r['manifest']) == r['manifest'], 'Magazine icon not updated'
                assert [patch(H, m) for m in r['contained']] == r['contained'], 'Contained magazine icon not updated'
            report = {'mode': mode, 'manifests': before, 'ammo_icon': AMMO, 'pistol_icon': PISTOL_ICON}
        (OUT / (mode + '.json')).write_text(json.dumps(report, indent=2))
        print('PISTOL_ICONS ' + json.dumps({'mode': mode, 'components': len(entries), 'ammo_icon': AMMO}))
        return report
    finally:
        gc.collect()


if __name__ == '__main__':
    main('verify')
