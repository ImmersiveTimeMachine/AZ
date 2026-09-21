# @Description: Import the native-GIMP level map and bind its measured calibration.
import json
from pathlib import Path
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
FOLDER = '/Game/AZ/Blueprints/Menu/Map'
TEXTURE = FOLDER + '/Art/T_CHALK_Map_L001'
DEFINITION = FOLDER + '/DA_AZ_Map_L001'
OWNER = 'quest_map_texture_setup:v1'


def configure():
    state = unreal.ToolsetRegistry.execute_tool('EditorToolset.EditorAppToolset', 'IsPIERunning', '{}')
    assert state.is_complete and not state.error and not json.loads(state.value)['returnValue']
    receipt = json.loads((ROOT/'Saved/QuestMapImplementation/map-capture-final.json').read_text())
    assert receipt['completed'] and receipt['temporary_actor_removed']
    assert receipt['world'] == '/Game/AZ/Maps/L_001.L_001'
    for path in (TEXTURE, DEFINITION):
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            assert unreal.EditorAssetLibrary.get_metadata_tag(unreal.load_asset(path), 'AZ.QuestMap.AuthoringOwner') == OWNER
    if not unreal.EditorAssetLibrary.does_asset_exist(TEXTURE):
        task = unreal.AssetImportTask()
        task.filename = str(ROOT/'UI Design/CHALK_QuestMap_v01/sources/map/T_CHALK_Map_L001.png')
        task.destination_path = FOLDER + '/Art'
        task.destination_name = 'T_CHALK_Map_L001'
        task.automated = True
        task.save = False
        task.replace_existing = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(TEXTURE)
        assert isinstance(texture, unreal.Texture2D)
        unreal.EditorAssetLibrary.set_metadata_tag(texture, 'AZ.QuestMap.AuthoringOwner', OWNER)
    texture = unreal.load_asset(TEXTURE)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    if not unreal.EditorAssetLibrary.does_asset_exist(DEFINITION):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.AZ_MapDefinition)
        definition = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_AZ_Map_L001', FOLDER, unreal.DataAsset, factory)
        assert definition
        unreal.EditorAssetLibrary.set_metadata_tag(definition, 'AZ.QuestMap.AuthoringOwner', OWNER)
    definition = unreal.load_asset(DEFINITION)
    for name, value in {
        'MapId': unreal.Name('L_001'), 'LayerId': unreal.Name('Outdoor'),
        'DisplayName': unreal.Text('LOCAL AREA'), 'MapTexture': texture,
        'WorldOrigin': unreal.Vector(*receipt['origin']), 'WorldSizeCm': unreal.Vector2D(*receipt['size']),
        'RotationDegrees': receipt['rotation'], 'bFlipU': receipt['flip_u'], 'bFlipV': receipt['flip_v'],
    }.items():
        definition.set_editor_property(name, value)
    # bool+out uses Optional[Text] in Unreal Python; an empty Text is success.
    result = definition.validate_definition()
    assert result is not None and str(result) == '', repr(result)
    report = {'texture': TEXTURE, 'definition': DEFINITION, 'calibration': receipt,
              'texture_size': [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()],
              'native_validation': 'passed', 'saved': False}
    (ROOT/'Saved/QuestMapImplementation/map-art-configured.json').write_text(json.dumps(report, indent=2))
    return report
