# @Description: Prepare owned PlayStation controller data and a reviewable CommonInput config patch.
"""Bounded Module 7 preparation. Import is inert; root executes editor stages.

prepare_controller_data() copies two data-only BPs, leaving pack sources intact.
Root native-compiles/saves the two returned assets, then verifies controller data.
prepare_config_patch() is file-only: writes a unified diff, NEVER DefaultGame.ini.
No gameplay mappings, runtime widgets, controller class, input mode or save owner
are changed. The runtime prompt/family recipe is saved alongside the patch.
"""
import difflib
import gc
import importlib.util
import json
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
_spec = importlib.util.spec_from_file_location('field_notes_input_style_support', ROOT / 'Tools/field_notes_styles_setup.py')
S = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(S)
OUT = S.OUT / 'Module7'
SOURCE = '/Game/MenuSystemPro/Blueprints/Input/Common/'
DEST = '/Game/AZ/Blueprints/Input/Common/'
COPIES = {family: (SOURCE + 'BP_MenuControllerData_' + family,
                   DEST + 'BP_AZ_InventoryControllerData_' + family) for family in ('PS4', 'PS5')}
OWNER_KEY = 'AZ.FieldNotes.InputDataOwner'
OWNER = 'field_notes_input_setup:v1'
REQUIRED_KEYS = {'Gamepad_FaceButton_Bottom', 'Gamepad_FaceButton_Right', 'Gamepad_FaceButton_Left',
                 'Gamepad_FaceButton_Top', 'Gamepad_DPad_Up', 'Gamepad_DPad_Down',
                 'Gamepad_DPad_Left', 'Gamepad_DPad_Right', 'Gamepad_LeftTrigger', 'Gamepad_RightTrigger',
                 'Gamepad_LeftShoulder', 'Gamepad_RightShoulder', 'Gamepad_LeftThumbstick',
                 'Gamepad_RightThumbstick', 'Gamepad_Left2D', 'Gamepad_Right2D',
                 'Gamepad_Special_Left', 'Gamepad_Special_Right'}


def controller_data(package, family):
    _, cdo = S.defaults(package)
    S.require(isinstance(cdo, S.ue().CommonInputBaseControllerData), 'Wrong controller-data native base: ' + package)
    data = S.read_object(cdo)
    values = data['values']
    S.require(values['inputType'] == 'Gamepad' and values['gamepadName'] == family, 'Controller family differs')
    keys = [row['key'] for row in values['inputBrushDataMap']]
    S.require(len(keys) == len(set(keys)) and REQUIRED_KEYS.issubset(keys), 'Required/unique glyph keys missing')
    hardware = 'DualShock4' if family == 'PS4' else 'DualSense'
    S.require({'inputDeviceName': 'FWinDualShock', 'hardwareDeviceIdentifier': hardware} in values['gamepadHardwareIdMapping'],
              'Verified hardware mapping missing: ' + family)
    data['coverage'] = {'key_count': len(keys), 'keys': keys, 'key_set_count': len(values['inputBrushKeySets'])}
    return data


def prepare_controller_data():
    S.idle()
    # Read every source/collision before the first duplicate.
    snapshots = {}
    for family, (source, target) in COPIES.items():
        snapshots[family] = controller_data(source, family)
        if S.ue().EditorAssetLibrary.does_asset_exist(target):
            asset = S.ue().load_asset(target)
            S.require(S.ue().EditorAssetLibrary.get_metadata_tag(asset, OWNER_KEY) == OWNER,
                      'Existing target is not owned by this recipe: ' + target)
    before_path = OUT / 'controller-source-before.json'
    if before_path.exists():
        old = json.loads(before_path.read_text(encoding='utf-8'))
        S.require(all(old[k]['values'] == snapshots[k]['values'] for k in COPIES), 'Vendor controller data changed since snapshot')
    else:
        S.write(before_path, snapshots)
    created = []
    for family, (source, target) in COPIES.items():
        if not S.ue().EditorAssetLibrary.does_asset_exist(target):
            asset = S.ue().EditorAssetLibrary.duplicate_asset(source, target)
            S.require(asset is not None, 'Controller-data copy failed: ' + target)
            S.ue().EditorAssetLibrary.set_metadata_tag(asset, OWNER_KEY, OWNER)
            created.append(target)
    gc.collect()
    return S.write(OUT / 'controller-data-prepared.json', {
        'created': created, 'native_compile_then_save': [pair[1] for pair in COPIES.values()],
        'vendor_runtime_library_imported': False, 'config_changed': False, 'saved': False,
        'ps5_art_caveat': 'Source PS5 uses PS4 textures for all32 keys; review Create/Options before claiming exact PS5 art.'})


def verify_controller_data():
    saved = json.loads((OUT / 'controller-source-before.json').read_text(encoding='utf-8'))
    result = {}
    for family, (source, target) in COPIES.items():
        asset = S.ue().load_asset(target)
        S.require(asset is not None and S.ue().EditorAssetLibrary.get_metadata_tag(asset, OWNER_KEY) == OWNER, 'Owned copy missing')
        copied = controller_data(target, family)
        original = controller_data(source, family)
        S.require(copied['values'] == saved[family]['values'] == original['values'], 'Data-only copy/source changed')
        result[family] = copied
    gc.collect()
    return S.write(OUT / 'controller-data-readback.json', result)


def prepare_config_patch():
    """File-only artifact. Root applies only after both new classes compile/save."""
    path = ROOT / 'Config/DefaultGame.ini'
    old = path.read_text(encoding='utf-8-sig')
    lines = old.splitlines(keepends=True)
    heading = '[CommonInputPlatformSettings_Windows CommonInputPlatformSettings]'
    starts = [i for i, line in enumerate(lines) if line.strip() == heading]
    S.require(len(starts) == 1, 'Expected exactly one Windows CommonInput section')
    start = starts[0]
    end = next((i for i in range(start + 1, len(lines)) if lines[i].lstrip().startswith('[')), len(lines))
    section = lines[start:end]
    defaults = [i for i, line in enumerate(section) if line.startswith('DefaultGamepadName=')]
    S.require(len(defaults) == 1 and section[defaults[0]].strip() in ('DefaultGamepadName=Xbox', 'DefaultGamepadName=XSX'),
              'Unexpected default gamepad family; inspect current config')
    section[defaults[0]] = 'DefaultGamepadName=XSX\n'  # Actual AZ Xbox CDO GamepadName, not its asset label.
    additions = []
    for _, target in COPIES.values():
        line = '+ControllerData=' + S.object_path(target) + '_C\n'
        if line.strip() not in {s.strip() for s in section}:
            additions.append(line)
    insert_at = max(i for i, line in enumerate(section) if line.startswith('+ControllerData=')) + 1
    section[insert_at:insert_at] = additions
    proposed = ''.join(lines[:start] + section + lines[end:])
    patch = ''.join(difflib.unified_diff(old.splitlines(True), proposed.splitlines(True),
                                       fromfile='a/Config/DefaultGame.ini', tofile='b/Config/DefaultGame.ini'))
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'commoninput-config.patch').write_text(patch, encoding='utf-8')
    return S.write(OUT / 'commoninput-config-proposal.json', {
        'source': str(path), 'source_sha256': S.digest(path), 'patch': str(OUT / 'commoninput-config.patch'),
        'applied': False, 'requires_saved_classes': [pair[1] for pair in COPIES.values()],
        'unchanged': ['InputData', 'AZ KBM/Xbox controller entries', 'GameInstance', 'input mappings', 'campaign save'],
        'rationale': 'Fresh CDO confirms engine family XSX. Add PS4/PS5 class registration without renaming sources.'})


def import_create_glyph():
    """Import only the reviewed native GIMP Create glyph; leave source-pack art intact."""
    S.idle()
    manifest = json.loads((ROOT/'UI Design/CHALK_FieldNotes_Input_v01/create-glyph-manifest.json').read_text(encoding='utf-8'))
    path = manifest['asset']
    S.require(path == S.DEST + '/Art/T_FN_PS5_Create' and manifest['reopened'], 'Unexpected Create artwork')
    S.require(S.digest(manifest['png']) == manifest['source_sha256'], 'Create PNG changed since review')
    S.require(not S.ue().EditorAssetLibrary.does_asset_exist(path), 'Inspect existing Create texture before reimport')
    task = S.ue().AssetImportTask()
    task.filename = manifest['png']; task.destination_path, task.destination_name = path.rsplit('/', 1)
    task.automated = True; task.save = False; task.factory = S.ue().TextureFactory()
    S.ue().AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = S.ue().load_asset(path)
    S.require(isinstance(texture, S.ue().Texture2D), 'Create texture import failed')
    S.ue().EditorAssetLibrary.set_metadata_tag(texture, OWNER_KEY, OWNER)
    S.ue().EditorAssetLibrary.set_metadata_tag(texture, 'AZ.FieldNotes.PNGSHA256', manifest['source_sha256'])
    for key, value in {'compression_settings': S.ue().TextureCompressionSettings.TC_EDITOR_ICON,
                       'lod_group': S.ue().TextureGroup.TEXTUREGROUP_UI, 'srgb': True,
                       'mip_gen_settings': S.ue().TextureMipGenSettings.TMGS_NO_MIPMAPS,
                       'address_x': S.ue().TextureAddress.TA_CLAMP, 'address_y': S.ue().TextureAddress.TA_CLAMP}.items():
        texture.set_editor_property(key, value)
    gc.collect()
    return {'imported': path, 'next': 'Separate call configure_create_glyph() after async texture processing'}


def configure_create_glyph():
    """One PS5 brush delta, with all other controller values compared to source."""
    import copy
    import shutil
    S.idle()
    path = S.DEST + '/Art/T_FN_PS5_Create'; texture = S.ue().load_asset(path)
    S.require(texture and [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()] == [64, 64], 'Create glyph size differs')
    S.require(S.ue().EditorAssetLibrary.get_metadata_tag(texture, OWNER_KEY) == OWNER, 'Unowned Create glyph')
    source, target = COPIES['PS5']; bp, cdo = S.defaults(target)
    before = controller_data(target, 'PS5'); original = controller_data(source, 'PS5')
    expected = copy.deepcopy(original['values'])
    entries = [row for row in expected['inputBrushDataMap'] if row['key'] == 'Gamepad_Special_Left']
    S.require(len(entries) == 1, 'Create key must have exactly one brush')
    entries[0]['keyBrush']['resourceObject'] = S.ref(path)
    S.require(before['values'] == original['values'] or before['values'] == expected, 'Unexpected PS5 controller edits')
    folder = OUT / ('BeforeCreate-' + S.stamp()); S.write(folder/'before.json', before)
    shutil.copy2(S.package_file(target), folder/S.package_file(target).name)
    patch = {'inputBrushDataMap': expected['inputBrushDataMap']}
    S._validate_patch(before['schema'], patch)
    S.require(S.ue().ToolsetLibrary.set_object_properties(cdo, json.dumps(patch)), 'Create glyph assignment failed')
    after = controller_data(target, 'PS5')
    S.require(after['values'] == expected, 'A controller field other than the Create brush changed')
    gc.collect()
    return S.write(OUT/'ps5-create-brush.json', {'asset': target, 'texture': path,
                    'backup': str(folder), 'expected_values': expected,
                    'native_compile_then_save': [target, path], 'physical_device_verified': False})
