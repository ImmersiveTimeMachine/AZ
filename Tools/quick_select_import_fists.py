"""Ordinary Unreal Python: guarded import of the native GIMP Fists artwork.

Use main('import') after editor recovery, then save only the returned package
through the dedicated asset tool. A specific TextureFactory bypasses the
Interchange path that stalled the first attempt. No global importer setting,
existing asset overwrite, Blueprint compile, PIE, or tests.
"""
import json
from pathlib import Path
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
SOURCE = ROOT / 'UI Design/CHALK_HUD_v03/unreal-art/T_HUD_Fists.png'
ASSET = '/Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Fists'
OWNER_KEY = 'AZ.QuickSelect.FistsOwner'
OWNER = 'quick_select_import_fists:v1'


def main(mode='audit'):
    assert mode in ('audit', 'import', 'verify')
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before import'
    assert SOURCE.is_file(), 'Native GIMP PNG is missing'
    asset = unreal.load_asset(ASSET) if unreal.EditorAssetLibrary.does_asset_exist(ASSET) else None
    if mode == 'import':
        if asset is None:
            task = unreal.AssetImportTask()
            task.set_editor_property('filename', str(SOURCE))
            task.set_editor_property('destination_path', ASSET.rsplit('/', 1)[0])
            task.set_editor_property('destination_name', ASSET.rsplit('/', 1)[1])
            task.set_editor_property('automated', True)
            task.set_editor_property('save', False)
            # AssetTools.cpp: bUseInterchangeFramework requires SpecifiedFactory==nullptr.
            task.set_editor_property('factory', unreal.TextureFactory())
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            asset = unreal.load_asset(ASSET)
            assert isinstance(asset, unreal.Texture2D), 'Texture import did not produce a Texture2D'
            unreal.EditorAssetLibrary.set_metadata_tag(asset, OWNER_KEY, OWNER)
        assert unreal.EditorAssetLibrary.get_metadata_tag(asset, OWNER_KEY) == OWNER, 'Inspect an existing unowned asset before replacing it'
        asset.modify()
        asset.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        asset.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        asset.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        asset.set_editor_property('srgb', True)
        asset.set_editor_property('never_stream', True)
    if mode != 'audit':
        assert isinstance(asset, unreal.Texture2D)
        assert (asset.blueprint_get_size_x(), asset.blueprint_get_size_y()) == (128, 128)
        assert asset.get_editor_property('lod_group') == unreal.TextureGroup.TEXTUREGROUP_UI
    result = {'status': mode, 'source': str(SOURCE), 'asset': asset.get_path_name() if asset else None,
              'save_after_return': [ASSET] if mode == 'import' else [], 'no_pie': True}
    out = ROOT / 'Saved/QuickSelectV2'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'fist-import-readback.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    print(json.dumps(main(), indent=2))
