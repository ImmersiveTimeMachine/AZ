# @Description: Audit, back up, or narrowly assign CHALK Quick Select presentation and input defaults.
"""Ordinary Unreal Python; main() is read-only and importing never runs authoring.

Sequence: main('backup'); build/restart native Quick Select classes; author entry
and root with quick_select_assets.py and compile them through dedicated tools;
main('assign'); compile ONLY the returned controller/root/entry Blueprints through
dedicated tools; save ONLY the returned packages; main('verify'). Re-read current
CDOs after compilation, never stale component references. No PIE, tests,
Blueprint compilation, package saves, maps, item manifests or vendor edits here.

Only the new action, one appended shared-context mapping, controller
QuickSelectToggleAction, QuickSelect.WidgetClass, root.EntryWidgetClass and
four presentation fields on existing QuickBar slots and new-entry outline
colors are authored. Firing,
ChangeFireModeAction, WeaponSlotActions, inventory and ability grants survive.
"""
import gc
import hashlib
import json
import runpy
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path('C:/UnrealEngine/Games/AZ')
OUTPUT = ROOT / 'Saved/QuickSelect'
RECEIPT = OUTPUT / 'backup-receipt.json'
CONTROLLER = '/Game/AZ/Blueprints/Player/BP_AZ_PlayerController'
SHARED_IMC = '/Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IMC_AlwaysAllowed'
ACTION = '/Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IA_QuickSelect'
FOLDER = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect'
WIDGET = FOLDER + '/WBP_AZ_QuickSelect'
ENTRY = FOLDER + '/WBP_AZ_QuickSelectEntry'
RIFLE_ICON = '/Game/AZ/Assets/Weapons/AK12_Rifle/UI/Textures/Rifle_PrimaryIcon'
OWNED = (CONTROLLER, SHARED_IMC, ACTION, WIDGET, ENTRY)
EXISTING = (CONTROLLER, SHARED_IMC)
SCHEMA = 'quick_select_assign:v1'
OWNER_KEY = 'AZ.QuickSelect.Owner'
PRESENTATION_FIELDS = ('DisplayName', 'Icon', 'Position', 'bEnabled')
PROTECTED_CONTROLLER_FIELDS = ('SharedInputMappingContext', 'WeaponSlotActions',
                               'ChangeFireModeAction', 'InventoryHudWidgetClass')
ENTRY_STYLE = {'IdleColor': ('64706A', .9), 'HoveredColor': ('FFBA8C', 1),
               'EditingColor': ('FFBA8C', 1), 'EquippedColor': ('EEEAE0', .95)}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def obj_path(value):
    return value.get_path_name() if value else None


def serial(value):
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, 'export_text'):
        return value.export_text()
    try:
        return [serial(item) for item in value]
    except TypeError:
        return str(value)


def dirty_packages():
    return sorted({p.get_path_name() for p in
                   unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
                   + unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()})


def write_report(name, data):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / name
    temp = path.with_suffix('.json.tmp')
    temp.write_text(json.dumps(data, indent=2), encoding='utf-8')
    temp.replace(path)


def package_file(package):
    require(package in OWNED, 'Unexpected package: ' + package)
    content = (ROOT / 'Content').resolve()
    target = (content / (package[len('/Game/'):] + '.uasset')).resolve()
    require(target.is_relative_to(content), 'Target escaped project content')
    return target


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path, cls=None):
    asset = unreal.load_asset(path)
    require(asset and (cls is None or isinstance(asset, cls)), 'Missing/wrong asset: ' + path)
    return asset


def context():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    require(project == ROOT.resolve(), 'Wrong Unreal project')
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(not editor.get_game_world(), 'Stop PIE before Quick Select authoring')
    bp = load(CONTROLLER, unreal.Blueprint)
    cdo = unreal.get_default_object(bp.generated_class())
    imc = cdo.get_editor_property('SharedInputMappingContext')
    require(imc and imc.get_path_name().split('.')[0] == SHARED_IMC, 'Shared IMC changed')
    quickbar = cdo.get_editor_property('QuickBar')
    require(quickbar, 'QuickBar default subobject missing')
    return bp, cdo, quickbar, imc


def key_name(mapping):
    return mapping.get_editor_property('key').export_text()


def mappings(imc):
    return list(imc.get_editor_property('default_key_mappings').get_editor_property('mappings'))


def protected_slots(quickbar):
    # Explicit authoritative gameplay fields are never rewritten by this helper.
    fields = ('WeaponTag', 'WeaponAbilities', 'bStrafeOnEquip', 'EffectsOnEquip',
              'bInventoryBacked', 'InventoryItemType')
    return [{key: serial(slot.get_editor_property(key)) for key in fields}
            for slot in quickbar.get_editor_property('Slots')]


def snapshot():
    bp, cdo, quickbar, imc = context()
    result = {'controller': obj_path(bp), 'controller_properties': {
        name: serial(cdo.get_editor_property(name)) for name in PROTECTED_CONTROLLER_FIELDS},
        'quickbar_object': obj_path(quickbar), 'slot_gameplay': protected_slots(quickbar),
        'slot_exports': [slot.export_text() for slot in quickbar.get_editor_property('Slots')],
        'shared_mappings': [row.export_text() for row in mappings(imc)],
        'dirty_packages': dirty_packages(), 'no_pie': True,
        'native_available': hasattr(unreal, 'AZ_QuickSelectComponent') and
                            hasattr(unreal, 'AZ_QuickSelectWidget')}
    # Optional new reflected fields are absent in the pre-build snapshot.
    try:
        component = cdo.get_editor_property('QuickSelect')
        result['quick_select'] = {'component': obj_path(component),
            'widget_class': obj_path(component.get_editor_property('WidgetClass')),
            'toggle_action': obj_path(cdo.get_editor_property('QuickSelectToggleAction'))}
    except Exception:
        result['quick_select'] = None
    return result


def backup():
    before = snapshot()
    require(not set(OWNED).intersection(before['dirty_packages']),
            'Quick Select target package has unsaved changes; preserve them before backup')
    destination = ROOT / 'Saved/Backups/QuickSelect' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    destination.mkdir(parents=True)
    files = []
    for package in OWNED:
        source = package_file(package)
        row = {'package': package, 'existed': source.is_file()}
        if source.is_file():
            target = destination / source.relative_to(ROOT / 'Content')
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            require(sha256(source) == sha256(target), 'Backup verification failed')
            row.update(source=str(source), backup=str(target), sha256=sha256(source))
        files.append(row)
    result = {'schema': SCHEMA, 'status': 'backed_up', 'backup': str(destination),
              'files': files, 'snapshot': before}
    (destination / 'receipt.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    write_report(RECEIPT.name, result)
    return result


def baseline(check_disk=False):
    require(RECEIPT.is_file(), 'Run main("backup") before authoring')
    data = json.loads(RECEIPT.read_text(encoding='utf-8'))
    require(data.get('schema') == SCHEMA, 'Wrong backup receipt')
    require({row['package'] for row in data['files']} == set(OWNED), 'Backup package set differs')
    if check_disk:
        for row in data['files']:
            if row['package'] in EXISTING:
                source = package_file(row['package'])
                require(source.is_file() and sha256(source) == row.get('sha256'),
                        'Controller/shared input changed on disk since backup; refresh baseline')
    return data


def is_own_mapping(mapping):
    action = mapping.get_editor_property('action')
    return action and action.get_path_name().split('.')[0] == ACTION


def assert_preserved(before):
    current = snapshot()
    require(current['controller_properties'] == before['controller_properties'],
            'An existing controller input/HUD reference changed')
    require(current['slot_gameplay'] == before['slot_gameplay'],
            'QuickBar authoritative weapon definitions changed')
    _, _, _, imc = context()
    retained = [row.export_text() for row in mappings(imc) if not is_own_mapping(row)]
    # Baselines after an approved rerun may already contain our appended mapping.
    old = [row for row in before['shared_mappings'] if ACTION + '.' not in row]
    require(retained == old, 'An existing shared mapping changed or reordered')
    return current


def set_checked(obj, name, value, no_notify=False):
    # Discover on the actual object before writing; stale reflection fails first.
    obj.get_editor_property(name)
    if no_notify:
        obj.set_editor_property(name, value, notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    else:
        obj.set_editor_property(name, value)
    require(serial(obj.get_editor_property(name)) == serial(value), 'Readback failed: ' + name)


def action_settings(action):
    return {'value_type': str(action.get_editor_property('value_type')),
            'triggers': [obj_path(x) for x in action.get_editor_property('triggers')],
            'modifiers': [obj_path(x) for x in action.get_editor_property('modifiers')],
            'consume_input': action.get_editor_property('consume_input'),
            'player_mappable': obj_path(action.get_editor_property('player_mappable_key_settings'))}


def assign_input(imc):
    action = unreal.load_asset(ACTION) if unreal.EditorAssetLibrary.does_asset_exist(ACTION) else None
    if action is None:
        folder, name = ACTION.rsplit('/', 1)
        action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.InputAction, None)
        require(action, 'InputAction creation failed')
        unreal.EditorAssetLibrary.set_metadata_tag(action, OWNER_KEY, SCHEMA)
        action.modify()
        set_checked(action, 'value_type', unreal.InputActionValueType.BOOLEAN)
        set_checked(action, 'triggers', [])
        set_checked(action, 'modifiers', [])
        set_checked(action, 'consume_input', True)
        settings = unreal.new_object(unreal.PlayerMappableKeySettings, outer=action,
                                     name='QuickSelectMappableSettings')
        set_checked(settings, 'name', unreal.Name('AZ.QuickSelect'))
        set_checked(settings, 'display_name', unreal.Text('Quick Select'))
        set_checked(settings, 'display_category', unreal.Text('Interface'))
        set_checked(action, 'player_mappable_key_settings', settings)
    require(isinstance(action, unreal.InputAction) and
            unreal.EditorAssetLibrary.get_metadata_tag(action, OWNER_KEY) == SCHEMA,
            'Unexpected/unowned Quick Select InputAction')
    require(action.get_editor_property('value_type') == unreal.InputActionValueType.BOOLEAN
            and not action.get_editor_property('triggers') and not action.get_editor_property('modifiers'),
            'Quick Select action has custom input behavior; inspect rather than replace')
    data = imc.get_editor_property('default_key_mappings').copy()
    rows = [row.copy() for row in data.get_editor_property('mappings')]
    own = [row for row in rows if is_own_mapping(row)]
    require(len(own) <= 1, 'Duplicate Quick Select mappings')
    if not own:
        require(not any(key_name(row) == 'Tab' for row in rows), 'Tab is already used in the shared IMC')
        row = unreal.EnhancedActionKeyMapping()
        key = unreal.Key()
        require(key.import_text('Tab'), 'Could not import Tab key')
        row.set_editor_property('action', action)
        row.set_editor_property('key', key)
        rows.append(row)
        data.set_editor_property('mappings', rows)
        imc.modify()
        set_checked(imc, 'default_key_mappings', data)
    else:
        require(key_name(own[0]) == 'Tab' and not own[0].get_editor_property('triggers')
                and not own[0].get_editor_property('modifiers'), 'Existing Quick Select mapping was customized')
    return action


def linear_color(rgb, alpha):
    channels = [int(rgb[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    channels = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in channels]
    return unreal.LinearColor(*channels, alpha)


def prepare_defaults():
    bp, cdo, quickbar, _ = context()
    component = cdo.get_editor_property('QuickSelect')
    require(isinstance(component, unreal.AZ_QuickSelectComponent), 'QuickSelect native component is unavailable')
    widget_bp, entry_bp = load(WIDGET, unreal.WidgetBlueprint), load(ENTRY, unreal.WidgetBlueprint)
    widget_cdo = unreal.get_default_object(widget_bp.generated_class())
    require(isinstance(widget_cdo, unreal.AZ_QuickSelectWidget), 'Compile native root widget before assignment')
    entry_cdo = unreal.get_default_object(entry_bp.generated_class())
    require(isinstance(entry_cdo, unreal.AZ_QuickSelectEntryWidget),
            'Compile native entry widget before assignment')
    slots = [slot.copy() for slot in quickbar.get_editor_property('Slots')]
    require(len(slots) == 2, 'Expected current Fists/Rifle slots only; review any new slots')
    require(str(slots[0].get_editor_property('WeaponTag').get_editor_property('tag_name')) == 'Weapon.Fist'
            and str(slots[1].get_editor_property('WeaponTag').get_editor_property('tag_name')) == 'Weapon.Rifle',
            'Current slot identity/order changed')
    # EditDefaultsOnly members reject detached Python set_editor_property calls.
    # Import into detached copies, then set the owning CDO array as one value.
    # Reuse only the guarded parser helpers; never run the old foundation setup.
    helpers = runpy.run_path(str(ROOT / 'Tools/rifle_p01_activate.py'), run_name='quick_select_text_helpers')
    for slot, label, position, icon in ((slots[0], 'FISTS', 'Center', None),
            (slots[1], 'LONG GUN', 'Left', load(RIFLE_ICON, unreal.Texture2D))):
        for prop in PRESENTATION_FIELDS:
            slot.get_editor_property(prop)
        original = helpers['fields'](slot.export_text())
        values = original
        changes = {'DisplayName': 'INVTEXT(' + json.dumps(label) + ')',
                   'Icon': helpers['object_reference']('/Script/Engine.Texture2D', RIFLE_ICON) if icon else 'None',
                   'Position': position, 'bEnabled': 'True'}
        for key, value in changes.items():
            values = helpers['replace_field'](values, key, value)
        require(slot.import_text(helpers['encode_fields'](values)), 'Slot presentation import failed')
        retained = lambda rows: [(k, v) for k, v in rows if k not in PRESENTATION_FIELDS]
        require(retained(original) == retained(helpers['fields'](slot.export_text())),
                'A slot gameplay field changed while importing presentation')
    # All reflection, asset classes, and copied slot data validated before writes.
    for obj, prop in ((cdo, 'QuickSelectToggleAction'), (component, 'WidgetClass'),
                      (widget_cdo, 'EntryWidgetClass')):
        obj.get_editor_property(prop)
    for prop in tuple(ENTRY_STYLE) + ('UnavailableOpacity',):
        entry_cdo.get_editor_property(prop)
    return bp, cdo, quickbar, component, widget_bp, widget_cdo, entry_bp, entry_cdo, slots


def assign_defaults(action):
    bp, cdo, quickbar, component, widget_bp, widget_cdo, entry_bp, entry_cdo, slots = prepare_defaults()
    bp.modify()
    widget_bp.modify()
    entry_bp.modify()
    cdo.modify()
    component.modify()
    quickbar.modify()
    widget_cdo.modify()
    entry_cdo.modify()
    set_checked(cdo, 'QuickSelectToggleAction', action, no_notify=True)
    set_checked(component, 'WidgetClass', widget_bp.generated_class(), no_notify=True)
    set_checked(widget_cdo, 'EntryWidgetClass', entry_bp.generated_class(), no_notify=True)
    set_checked(quickbar, 'Slots', slots, no_notify=True)
    for name, (rgb, alpha) in ENTRY_STYLE.items():
        set_checked(entry_cdo, name, linear_color(rgb, alpha), no_notify=True)
    set_checked(entry_cdo, 'UnavailableOpacity', .5, no_notify=True)


def verify_assignment():
    _, cdo, quickbar, imc = context()
    action = load(ACTION, unreal.InputAction)
    require(cdo.get_editor_property('QuickSelectToggleAction') == action, 'Controller toggle action is unassigned')
    component = cdo.get_editor_property('QuickSelect')
    widget_bp = load(WIDGET, unreal.WidgetBlueprint)
    entry_bp = load(ENTRY, unreal.WidgetBlueprint)
    require(component.get_editor_property('WidgetClass') == widget_bp.generated_class(), 'QuickSelect.WidgetClass differs')
    require(unreal.get_default_object(widget_bp.generated_class()).get_editor_property('EntryWidgetClass')
            == entry_bp.generated_class(), 'Root EntryWidgetClass differs')
    own = [row for row in mappings(imc) if is_own_mapping(row)]
    require(len(own) == 1 and key_name(own[0]) == 'Tab', 'Tab mapping missing or duplicated')
    slots = list(quickbar.get_editor_property('Slots'))
    require(len(slots) == 2 and slots[0].get_editor_property('Position') == unreal.AZ_QuickSlotPosition.CENTER
            and slots[1].get_editor_property('Position') == unreal.AZ_QuickSlotPosition.LEFT,
            'Fists/Rifle display positions differ')
    require(all(slot.get_editor_property('bEnabled') for slot in slots), 'An authored slot is disabled')
    require(str(slots[0].get_editor_property('DisplayName')) == 'FISTS'
            and str(slots[1].get_editor_property('DisplayName')) == 'LONG GUN', 'Slot display labels differ')
    require(slots[0].get_editor_property('Icon') is None
            and obj_path(slots[1].get_editor_property('Icon')) == RIFLE_ICON + '.Rifle_PrimaryIcon',
            'Slot display icons differ')
    mappable = action.get_editor_property('player_mappable_key_settings')
    require(mappable and str(mappable.get_editor_property('name')) == 'AZ.QuickSelect'
            and action.get_editor_property('consume_input'), 'Quick Select remapping settings differ')
    entry_cdo = unreal.get_default_object(entry_bp.generated_class())
    for prop, (rgb, alpha) in ENTRY_STYLE.items():
        require(serial(entry_cdo.get_editor_property(prop)) == serial(linear_color(rgb, alpha)),
                'Entry outline color differs: ' + prop)
    return {'toggle_action': obj_path(action), 'action_settings': action_settings(action),
            'widget_class': obj_path(widget_bp.generated_class()), 'entry_class': obj_path(entry_bp.generated_class()),
            'slot_exports': [slot.export_text() for slot in slots]}


def main(mode='audit'):
    try:
        require(mode in ('audit', 'backup', 'assign', 'verify'), 'Unknown mode')
        if mode == 'backup':
            return backup()
        if mode == 'audit':
            result = snapshot()
            write_report('audit-readback.json', result)
            return result
        saved = baseline()
        before = saved['snapshot']
        assert_preserved(before)
        require(hasattr(unreal, 'AZ_QuickSelectWidget') and hasattr(unreal, 'AZ_QuickSlotPosition'),
                'Build/restart the native Quick Select classes before assignment')
        if mode == 'assign':
            # A saved, fully assigned rerun is a verified no-op. Never overwrite
            # changed input/defaults merely to restore this script's preferences.
            try:
                ready = verify_assignment()
            except Exception:
                ready = None
            if ready and not set(OWNED).intersection(dirty_packages()):
                return {'schema': SCHEMA, 'status': 'already_assigned', 'assignment': ready,
                        'snapshot': assert_preserved(before), 'compile_after_return': [],
                        'explicit_save_packages': [], 'no_pie': True}
            baseline(check_disk=True)
            # New widget packages are expected dirty after authoring. Existing
            # controller/shared context must still match the inspected baseline.
            require(not set(EXISTING).intersection(dirty_packages()), 'Controller/shared IMC has unsaved changes')
            prepare_defaults()  # Fail on missing classes/widgets BEFORE input creation.
            _, _, _, imc = context()
            action = assign_input(imc)
            assign_defaults(action)
        result = {'schema': SCHEMA, 'status': 'requires_compile_and_save' if mode == 'assign' else 'readback_passed',
                  'assignment': verify_assignment(), 'snapshot': assert_preserved(before),
                  'backup': saved['backup'], 'no_pie': True,
                  'compile_after_return': [ENTRY, WIDGET, CONTROLLER] if mode == 'assign' else [],
                  'explicit_save_packages': list(OWNED) if mode == 'assign' else []}
        if mode == 'verify':
            require(not set(OWNED).intersection(dirty_packages()), 'Save explicit Quick Select packages before final verification')
        write_report(mode + '-readback.json', result)
        return result
    finally:
        gc.collect()


if __name__ == '__main__':
    print(json.dumps(main(), indent=2))
