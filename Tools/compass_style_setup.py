# @Description: Apply the approved CHALK compass palette and native GIMP artwork.
import gc
import importlib.util
import json
from pathlib import Path
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
spec = importlib.util.spec_from_file_location('az_compass_hud', ROOT / 'Tools/compass_hud_setup.py')
H = importlib.util.module_from_spec(spec)
spec.loader.exec_module(H)
A = H.A
ART = A.DEST + '/Art'
FILES = ROOT / 'UI Design/CHALK_HUD_v03/unreal-art/Compass'


def linear(hex_value):
    v = [int(hex_value[i:i+2], 16) / 255 for i in (1, 3, 5)]
    convert = lambda x: x / 12.92 if x <= .04045 else ((x + .055) / 1.055) ** 2.4
    return {'r': convert(v[0]), 'g': convert(v[1]), 'b': convert(v[2]), 'a': 1.0}


def import_art():
    A.idle()
    result = []
    for name in ('T_CHALK_CompassPointer', 'T_CHALK_CompassTarget', 'T_CHALK_CompassStrip'):
        path = ART + '/' + name
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            task = unreal.AssetImportTask()
            task.filename = str(FILES / (name + '.png'))
            task.destination_path = ART
            task.destination_name = name
            task.automated = True
            task.replace_existing = False
            task.save = False
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = A.load(path)
        texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property('srgb', True)
        texture.set_editor_property('address_x', unreal.TextureAddress.TA_WRAP if name.endswith('Strip') else unreal.TextureAddress.TA_CLAMP)
        texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
        unreal.EditorAssetLibrary.set_metadata_tag(texture, 'AZ.Compass.ArtSource', str(FILES / (name + '.png')))
        A.require(unreal.EditorAssetLibrary.save_loaded_asset(texture), 'Texture save failed')
        result.append(path)
    A.write('style-art-imported', result)
    return result


def overlay_top(row, top):
    H.properties(row['slot'], {'padding': {'left': 0, 'top': top, 'right': 0, 'bottom': 0},
                              'horizontalAlignment': 'HAlign_Center', 'verticalAlignment': 'VAlign_Top'})


def layout():
    A.idle()
    module = A.load(A.MODULE)
    cdo = unreal.get_default_object(module.generated_class())
    obj = H.ref(cdo.get_path_name())
    current = H.properties(obj)
    chalk, peach = linear('#EEEAE0'), linear('#FFBA8C')
    font = current['microFont2'].copy()
    font.update(fontObject=H.ref('/Engine/EngineFonts/Roboto.Roboto'), typefaceFontName='Regular', size=14.25)
    changes = {'compass_Texture': H.ref(H.object_path(ART + '/T_CHALK_CompassStrip')),
               'pointerTexture': H.ref(H.object_path(ART + '/T_CHALK_CompassPointer')),
               'pointerScale': 1, 'pointerY_Translation': 0, 'markerY_Translation': 0,
               'compass_Tint': chalk, 'pointerTint': chalk, 'baseColor2': peach,
               'fontColor1': chalk, 'fontColor2': chalk, 'microFont': font, 'microFont2': font,
               'compass_BackgroundColor': linear('#101515'), 'compass_BackgroundOpacity': .48,
               'compass_BlendMask': H.ref('/Game/ProHUDV2_Horror/Textures/T_BlendMask_Compass_768x64_H.T_BlendMask_Compass_768x64_H')}
    H.properties(obj, changes)
    cp = A.DEST + '/WBP_AZ_Compass'
    rows = H.rows(cp)
    for name, w, h in [('SB_Content', 690, 150), ('SB_Compass', 600, 64), ('SB_Marker', 600, 64), ('SB_Pointer', 16, 16)]:
        H.properties(rows[name]['widget'], {'widthOverride': w, 'heightOverride': h,
                                           'bOverride_WidthOverride': True, 'bOverride_HeightOverride': True})
    overlay_top(rows['SB_Compass'], 28)
    overlay_top(rows['SB_Pointer'], 8)
    overlay_top(rows['SB_Marker'], 86)
    H.properties(rows['Background']['slot'], {'padding': {'left': 0, 'top': 0, 'right': 0, 'bottom': 22},
                                             'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'})
    # The source material pans one UV period per revolution. 1440px gives the
    # approved4px/degree; a600px retainer window covers150 degrees.
    H.properties(rows['Compass_2816x128']['slot'], {'layoutData': {
        'offsets': {'left': -420, 'top': 0, 'right': -420, 'bottom': 0},
        'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 1, 'y': 1}},
        'alignment': {'x': 0, 'y': 0}}})
    module_rows = H.rows(A.MODULE)
    H.properties(module_rows['CompassView']['slot'], {'layoutData': {
        'offsets': {'left': 0, 'top': 26, 'right': 690, 'bottom': 150},
        'anchors': {'minimum': {'x': .5, 'y': 0}, 'maximum': {'x': .5, 'y': 0}},
        'alignment': {'x': .5, 'y': 0}}, 'bAutoSize': False, 'zOrder': 1})
    # Marker geometry must use the same600px window as its container.
    mp = A.DEST + '/WBP_AZ_CompassMarker'
    mr = H.rows(mp)
    H.properties(mr['SB_Base']['widget'], {'widthOverride': 600, 'heightOverride': 64})
    H.properties(mr['SB_Marker']['widget'], {'widthOverride': 16, 'heightOverride': 16})
    overlay_top(mr['Marker'], 0)
    overlay_top(mr['VerticalBox_0'], 0)
    # Keep the supplied animations/refs; reorder existing vertical children.
    image_bp = A.load(mp)
    key = 'AZ.Compass.IconAboveDistance'
    if unreal.EditorAssetLibrary.get_metadata_tag(image_bp, key) != 'v1':
        box = unreal.load_object(None, mr['VerticalBox_0']['widget']['refPath'])
        text = unreal.load_object(None, mr['OV_Text']['widget']['refPath'])
        old_slot = H.properties(mr['OV_Text']['slot'])
        A.require(box.remove_child(text), 'Could not reorder distance row')
        new_slot = box.add_child(text)
        H.properties(H.ref(new_slot.get_path_name()), old_slot)
        unreal.EditorAssetLibrary.set_metadata_tag(image_bp, key, 'v1')
    mr = H.rows(mp)
    H.properties(mr['OV_Text']['slot'], {'padding': {'left': 0, 'top': 9, 'right': 0, 'bottom': 0}})
    result = {'palette_srgb': ['#EEEAE0', '#FFBA8C', '#101515'], 'compass_window': [600, 64],
              'strip_draw_width': 1440, 'angular_window': 150, 'marker_dot_mapping_preserved': True,
              'native_art': str(FILES), 'runtime_visual_check_pending': True}
    A.write('style-layout-authored', result)
    gc.collect()
    return result
