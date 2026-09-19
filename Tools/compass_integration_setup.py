# @Description: Prepare and verify isolated CHALK copies of ProHUD compass widgets.
"""Editor authoring only. Never starts/stops PIE or runs gameplay tests.

prepare() creates owned leaf copies and a receipt. Later stages adapt their
references before any active HUD attachment. Compilation is performed through
the dedicated native Blueprint tool after Python returns.
"""
import gc
import hashlib
import json
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/CompassIntegration'
DEST = '/Game/AZ/Blueprints/Menu/HUD/Navigation'
SOURCE = '/Game/ProHUDV2_Horror'
HUD = '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD'
OWNER_KEY = 'AZ.Compass.AuthoringOwner'
OWNER = 'compass_integration_setup:v1'
COPIES = {
    SOURCE + '/Widgets/Content/Compass/WB_Compass_H': DEST + '/WBP_AZ_Compass',
    SOURCE + '/Widgets/Content/Compass/WB_CompassMarker_H': DEST + '/WBP_AZ_CompassMarker',
    SOURCE + '/Widgets/Content/WorldMarker/WB_WorldContainer_H': DEST + '/WBP_AZ_WorldMarkerContainer',
    SOURCE + '/Widgets/Content/WorldMarker/WB_WorldMarker_H': DEST + '/WBP_AZ_WorldMarker',
}


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def call(name, **kwargs):
    group, tool = name.rsplit('.', 1)
    response = unreal.ToolsetRegistry.execute_tool(group, tool, json.dumps(kwargs))
    require(response.is_complete and not response.error, name + ': ' + str(response.error))
    return json.loads(response.value)['returnValue']


def idle():
    require(not call('EditorToolset.EditorAppToolset.IsPIERunning'),
            'PIE is running: wait for the user to stop it. No assets were authored.')


def path(package):
    require(package.startswith('/Game/'), 'Not a project package')
    result = (ROOT / 'Content' / (package[6:] + '.uasset')).resolve()
    require(result.is_relative_to((ROOT / 'Content').resolve()), 'Invalid package path')
    return result


def digest(package):
    return hashlib.sha256(path(package).read_bytes()).hexdigest()


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / (name + '.json')).write_text(json.dumps(value, indent=2), encoding='utf-8')


def baseline():
    idle()
    receipt = OUT / 'baseline.json'
    if receipt.exists():
        return json.loads(receipt.read_text(encoding='utf-8'))
    dirty = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    protected = set(COPIES) | {HUD}
    require(not (dirty & protected), 'Source/HUD has unsaved changes: ' + str(dirty & protected))
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    backup = OUT / ('Backup_' + stamp)
    backup.mkdir(parents=True)
    shutil.copy2(path(HUD), backup / 'WBP_AZ_GameHUD.uasset.bak')
    result = {'timestamp_utc': stamp, 'hud_backup': str(backup / 'WBP_AZ_GameHUD.uasset.bak'),
              'hashes': {p: digest(p) for p in sorted(protected)}, 'copies': COPIES,
              'editor_pid': __import__('os').getpid(), 'state': 'baseline'}
    tree = call('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint={'refPath': HUD + '.' + HUD.rsplit('/', 1)[1]})
    write('hud-tree-before', tree)
    write('baseline', result)
    return result


def prepare():
    idle()
    before = baseline()
    created, existing = [], []
    for source, target in COPIES.items():
        require(digest(source) == before['hashes'][source], 'Source changed since baseline: ' + source)
        if unreal.EditorAssetLibrary.does_asset_exist(target):
            asset = unreal.load_asset(target)
            require(unreal.EditorAssetLibrary.get_metadata_tag(asset, OWNER_KEY) == OWNER,
                    'Unowned asset at destination; do not overwrite: ' + target)
            existing.append(target)
        else:
            asset = unreal.EditorAssetLibrary.duplicate_asset(source, target)
            require(asset is not None, 'Duplicate failed: ' + target)
            unreal.EditorAssetLibrary.set_metadata_tag(asset, OWNER_KEY, OWNER)
            require(unreal.EditorAssetLibrary.save_loaded_asset(asset), 'Save failed: ' + target)
            created.append(target)
    result = {'created': created, 'existing': existing, 'copies': COPIES,
              'active_hud_unchanged': digest(HUD) == before['hashes'][HUD],
              'source_unchanged': all(digest(p) == before['hashes'][p] for p in COPIES),
              'state': 'isolated copies; graph context/remapping and compilation pending'}
    require(result['active_hud_unchanged'] and result['source_unchanged'], 'Protected baseline differs')
    write('prepared', result)
    gc.collect()
    return result


def module_fields():
    """Transfer only typed navigation settings into the empty owned module.

    Does not copy the vendor display tree, event graph, minimap or notifications.
    Compile the module through the native tool, then call module_defaults().
    """
    idle()
    source_bp = unreal.load_asset(SOURCE + '/Widgets/Content/WB_HUD_Container_H')
    target_bp = unreal.load_asset(DEST + '/WBP_AZ_CompassModule')
    require(target_bp is not None, 'Create the empty owned module first')
    library = json.loads((OUT / 'getter-bodies.json').read_text(encoding='utf-8'))
    source_graphs = json.loads((OUT / 'config-extraction.json').read_text(encoding='utf-8'))
    fields = {'MarkerPingIcon1', 'MarkerPingIcon2'}
    excluded = {'GetIsFirstPerson', 'GetDistantMarkerArrowIcon', 'GetVelocitySensitivity',
                'GetSensitivityInterpSpeed'}
    graphs = [lines for name, lines in library.items() if name not in excluded]
    graphs += list(source_graphs['module']['graphs'].values())
    for lines in graphs:
        for line in lines:
            match = re.search(r'VariableGet \| Get (\S+)$', line)
            if match:
                fields.add(match.group(1))
    names = set(unreal.BlueprintEditorLibrary.list_member_variable_names(source_bp, False))
    require(not (fields - names), 'Unknown source fields: ' + str(fields - names))
    current = set(unreal.BlueprintEditorLibrary.list_member_variable_names(target_bp, False))
    added = []
    for name in sorted(fields):
        if name in current:
            continue
        pin_type = unreal.BlueprintEditorLibrary.get_member_variable_type(source_bp, name)
        require(pin_type is not None, 'Missing exact source field type: ' + name)
        require(unreal.BlueprintEditorLibrary.add_member_variable(target_bp, name, pin_type),
                'Could not add field: ' + name)
        unreal.BlueprintEditorLibrary.set_blueprint_variable_category(target_bp, name, 'Navigation|Source Settings')
        added.append(name)
    unreal.EditorAssetLibrary.set_metadata_tag(target_bp, OWNER_KEY, OWNER)
    result = {'module': DEST + '/WBP_AZ_CompassModule', 'fields': sorted(fields), 'added': added,
              'state': 'exact source field types authored; native compile then defaults required'}
    write('module-fields', result)
    gc.collect()
    return result


def module_defaults():
    idle()
    receipt = json.loads((OUT / 'module-fields.json').read_text(encoding='utf-8'))
    source = unreal.get_default_object(unreal.load_asset(SOURCE + '/Widgets/Content/WB_HUD_Container_H').generated_class())
    target_bp = unreal.load_asset(receipt['module'])
    target = unreal.get_default_object(target_bp.generated_class())
    values = {}
    for name in receipt['fields']:
        value = source.get_editor_property(name)
        target.set_editor_property(name, value)
        values[name] = str(value)
    # Python wrappers for user-defined structs compare wrapper identity. Compare
    # the native property JSON instead, including nested font/texture references.
    group = 'editor_toolset.toolsets.object.ObjectTools.'
    wanted = {n.lower() for n in receipt['fields']}
    serialized = []
    for obj in (source, target):
        obj_ref = {'refPath': obj.get_path_name()}
        schema = json.loads(call(group + 'list_properties', instance=obj_ref))
        selected = [n for n in schema if n.lower() in wanted]
        require(len(selected) == len(wanted), 'Not all module settings are reflected')
        serialized.append(json.loads(call(group + 'get_properties', instance=obj_ref, properties=selected)))
    require(serialized[0] == serialized[1], 'Serialized source/module defaults differ')
    require(unreal.EditorAssetLibrary.save_loaded_asset(target_bp), 'Module default save failed')
    write('module-defaults', {'values': serialized[1], 'state': 'source defaults copied and read back; runtime not connected'})
    gc.collect()
    return {'copied_defaults': len(values), 'module': receipt['module']}
