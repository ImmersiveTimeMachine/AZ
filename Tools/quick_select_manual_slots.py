# @Description: Back up, assign, and verify the five manual CHALK quick-slot defaults.
"""Filesystem backups and ordinary Unreal Python authoring, with a read-only default.

Run main('backup') before main('assign'). Build/restart first, and author/compile
the root, standard entry, Fists entry and Fists icon separately. Assignment only
edits PC defaults, root CenterEntryWidgetClass, the exact Pawn IMC 2 mapping,
new 3/4 mappings, and three owned InputActions. It never starts PIE, compiles
Blueprints, saves packages, edits item manifests, or reruns the old two-slot setup.
After dedicated compilation/save, run main('verify') using fresh CDOs.
"""
import gc
import hashlib
import json
import re
import runpy
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUTPUT = ROOT / 'Saved/QuickSelectManualSlots'
RECEIPT = OUTPUT / 'backup-receipt.json'
SCHEMA = 'quick_select_manual_slots:v1'
OWNER_KEY = 'AZ.QuickSelect.ManualSlots.Owner'
PC = '/Game/AZ/Blueprints/Player/BP_AZ_PlayerController'
PAWN_IMC = '/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IMC_RT_PawnInputs'
SHARED_IMC = '/Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IMC_AlwaysAllowed'
LEGACY_TWO = '/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IA_RT_SecondaryWeapon'
FOLDER = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect'
WIDGET = FOLDER + '/WBP_AZ_QuickSelect'
ENTRY = FOLDER + '/WBP_AZ_QuickSelectEntry'
FISTS = FOLDER + '/WBP_AZ_QuickSelectFists'
FISTS_ICON = '/Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Fists'
ACTIONS = {n: '/Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IA_QuickSlot_' + str(n)
           for n in (2, 3, 4)}
KEYS = {2: 'Two', 3: 'Three', 4: 'Four'}
MUTATED = (PC, PAWN_IMC, WIDGET, *ACTIONS.values())
BACKED_UP = (*MUTATED, ENTRY, FISTS, FISTS_ICON, SHARED_IMC)
STRICT_BASELINE = (PC, PAWN_IMC)
PROTECTED_PC = ('SharedInputMappingContext', 'ChangeFireModeAction',
                'InventoryHudWidgetClass', 'QuickSelectToggleAction',
                'OpenInventoryAction', 'InputConfig')
PRESENTATION = ('DisplayName', 'Icon', 'Position', 'bEnabled')

# Both helper modules have guarded entry points. Loading definitions performs no
# asset mutation; never invoke quick_select_assign.main (it expects two slots).
H = runpy.run_path(str(ROOT / 'Tools/quick_select_assign.py'), run_name='manual_slot_helpers')
T = runpy.run_path(str(ROOT / 'Tools/rifle_p01_activate.py'), run_name='manual_slot_text_helpers')
require, serial, load = H['require'], H['serial'], H['load']
obj_path, dirty_packages = H['obj_path'], H['dirty_packages']
mappings, key_name, set_checked = H['mappings'], H['key_name'], H['set_checked']


def package(value):
    return obj_path(value).split('.')[0] if value else None


def write_report(name, data):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    target = OUTPUT / name
    temporary = target.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(data, indent=2), encoding='utf-8')
    temporary.replace(target)


def package_file(path):
    require(path in BACKED_UP and path.startswith('/Game/'), 'Unexpected package: ' + path)
    content = (ROOT / 'Content').resolve()
    target = (content / (path[len('/Game/'):] + '.uasset')).resolve()
    require(target.is_relative_to(content), 'Package escaped Content')
    return target


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def context():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    require(project == ROOT.resolve(), 'Wrong Unreal project')
    require(not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(),
            'Stop PIE before quick-slot authoring')
    bp = load(PC, unreal.Blueprint)
    cdo = unreal.get_default_object(bp.generated_class())
    quickbar = cdo.get_editor_property('QuickBar')
    require(quickbar, 'QuickBar default subobject missing')
    require(package(cdo.get_editor_property('SharedInputMappingContext')) == SHARED_IMC,
            'Controller shared context changed')
    return bp, cdo, quickbar, load(PAWN_IMC, unreal.InputMappingContext)


def retained_slot(slot):
    return [[k, v] for k, v in T['fields'](slot.export_text()) if k not in PRESENTATION]


def mapping_record(row):
    return {'key': key_name(row), 'action': package(row.get_editor_property('action')),
            'export': row.export_text(),
            'non_action': [[k, v] for k, v in T['fields'](row.export_text()) if k != 'Action']}


def snapshot():
    bp, cdo, quickbar, imc = context()
    slots = list(quickbar.get_editor_property('Slots'))
    require(slots, 'Fists slot missing')
    widget = load(WIDGET, unreal.WidgetBlueprint)
    widget_cdo = unreal.get_default_object(widget.generated_class())
    component = cdo.get_editor_property('QuickSelect')
    try:
        center_class = obj_path(widget_cdo.get_editor_property('CenterEntryWidgetClass'))
    except Exception:
        center_class = None
    return {'controller': obj_path(bp), 'controller_refs': {
                name: serial(cdo.get_editor_property(name)) for name in PROTECTED_PC},
            'quick_select_widget': obj_path(component.get_editor_property('WidgetClass')),
            'weapon_slot_actions': [obj_path(a) for a in cdo.get_editor_property('WeaponSlotActions')],
            'slot_exports': [s.export_text() for s in slots], 'slot0_gameplay': retained_slot(slots[0]),
            'root_entry_class': obj_path(widget_cdo.get_editor_property('EntryWidgetClass')),
            'root_center_class': center_class,
            'pawn_mappings': [mapping_record(row) for row in mappings(imc)],
            'shared_mappings': [r.export_text() for r in mappings(load(SHARED_IMC, unreal.InputMappingContext))],
            'native_ready_available': hasattr(unreal.AZ_QuickBarComponent, 'get_ready_item'),
            'dirty_packages': dirty_packages(), 'no_pie': True}


def backup():
    before = snapshot()
    require(not set(STRICT_BASELINE).intersection(before['dirty_packages']),
            'Controller/Pawn IMC has unsaved changes; preserve them before backup')
    destination = ROOT / 'Saved/Backups/QuickSelectManualSlots' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    destination.mkdir(parents=True)
    files = []
    for asset in BACKED_UP:
        source = package_file(asset)
        row = {'package': asset, 'existed': source.is_file()}
        if source.is_file():
            target = destination / source.relative_to(ROOT / 'Content')
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            require(sha256(source) == sha256(target), 'Backup verification failed: ' + asset)
            row.update(source=str(source), backup=str(target), sha256=sha256(source))
        files.append(row)
    result = {'schema': SCHEMA, 'backup': str(destination), 'files': files, 'snapshot': before,
              'status': 'backed_up', 'note': 'File copies preserve saved disk state; dirty dependency packages are listed in snapshot.'}
    (destination / 'receipt.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    write_report(RECEIPT.name, result)
    return result


def baseline(check_disk=False):
    require(RECEIPT.is_file(), 'Run main("backup") first')
    data = json.loads(RECEIPT.read_text(encoding='utf-8'))
    require(data.get('schema') == SCHEMA and {r['package'] for r in data['files']} == set(BACKED_UP),
            'Wrong or incomplete backup receipt')
    if check_disk:
        for row in data['files']:
            if row['package'] in STRICT_BASELINE:
                source = package_file(row['package'])
                require(source.is_file() and sha256(source) == row.get('sha256'),
                        'Controller/Pawn IMC changed since backup; refresh baseline')
    return data


def managed(row):
    return row['action'] in ACTIONS.values() or (row['key'] == 'Two' and row['action'] == LEGACY_TWO)


def assert_preserved(before):
    current = snapshot()
    for key in ('controller_refs', 'quick_select_widget', 'slot0_gameplay', 'root_entry_class', 'shared_mappings'):
        require(current[key] == before[key], 'Protected data changed: ' + key)
    require(current['weapon_slot_actions'][:2] == before['weapon_slot_actions'][:2],
            'Existing slot 0/1 actions changed')
    require([r['export'] for r in current['pawn_mappings'] if not managed(r)] ==
            [r['export'] for r in before['pawn_mappings'] if not managed(r)],
            'An unrelated Pawn IMC mapping changed or reordered')
    old_two = [r for r in before['pawn_mappings'] if r['key'] == 'Two']
    new_two = [r for r in current['pawn_mappings'] if r['key'] == 'Two']
    require(len(old_two) == len(new_two) == 1 and old_two[0]['non_action'] == new_two[0]['non_action'],
            'Two mapping settings changed beyond its action reference')
    return current


def verify_source_contract():
    source = (ROOT / 'Source/AZ/Private/Inventory/AZ_QuickBarComponent.cpp').read_text(encoding='utf-8-sig')
    for name in ('BindSelectedItem', 'OnInventoryChanged'):
        match = re.search(r'void UAZ_QuickBarComponent::' + name + r'\b[\s\S]*?^}', source, re.MULTILINE)
        require(match is not None, 'Missing source seam: ' + name)
        body = match.group(0)
        require('SetBinding(' not in body and 'GetItems()' not in body and '.ItemId =' not in body,
                'Automatic quick-slot assignment reappeared in ' + name)
    require(hasattr(unreal.AZ_QuickBarComponent, 'get_ready_item'),
            'Full build/restart required: GetReadyItem reflection is missing')


def import_fields(instance, changes):
    values = T['fields'](instance.export_text())
    for key, value in changes.items():
        instance.get_editor_property(key)  # Fail on stale reflection before changing the detached copy.
        values = T['replace_field'](values, key, value)
    require(instance.import_text(T['encode_fields'](values)), 'Struct import failed')
    return instance


def prepare():
    verify_source_contract()
    bp, cdo, quickbar, imc = context()
    original = list(quickbar.get_editor_property('Slots'))
    require(len(original) in (2, 5), 'Unexpected slot count; review authored slots before replacing')
    require(str(original[0].get_editor_property('WeaponTag').get_editor_property('tag_name')) == 'Weapon.Fist'
            and not original[0].get_editor_property('bInventoryBacked'), 'Slot 0 is not intrinsic Fists')
    actions = list(cdo.get_editor_property('WeaponSlotActions'))
    require(len(actions) in (2, 5) and actions[0] and actions[1], 'Expected preserved slot 0/1 input actions')
    if len(actions) == 5:
        require(all(package(actions[n]) == ACTIONS[n] for n in ACTIONS), 'Unexpected actions in slots 2–4')
    icon = load(FISTS_ICON, unreal.Texture2D)
    root_bp = load(WIDGET, unreal.WidgetBlueprint)
    root_cdo = unreal.get_default_object(root_bp.generated_class())
    fist_bp = load(FISTS, unreal.WidgetBlueprint)
    require(isinstance(root_cdo, unreal.AZ_QuickSelectWidget), 'Root widget native class is stale')
    require(isinstance(unreal.get_default_object(fist_bp.generated_class()), unreal.AZ_QuickSelectEntryWidget),
            'Compile the Fists entry before assignment')
    root_cdo.get_editor_property('CenterEntryWidgetClass')
    require(package(root_cdo.get_editor_property('EntryWidgetClass')) == ENTRY,
            'Standard entry class changed; preserve/review it')
    slots = [import_fields(original[0].copy(), {'DisplayName': 'INVTEXT("FISTS")',
             'Icon': T['object_reference']('/Script/Engine.Texture2D', FISTS_ICON),
             'Position': 'Center', 'bEnabled': 'True'})]
    require(retained_slot(slots[0]) == retained_slot(original[0]), 'Fists gameplay changed')
    for position in ('Left', 'Right', 'Up', 'Down'):
        slots.append(import_fields(unreal.AZ_QuickSlot(), {
            'WeaponTag': '(TagName="")', 'WeaponAbilities': '()', 'EffectsOnEquip': '()',
            'bStrafeOnEquip': 'False', 'bInventoryBacked': 'True', 'InventoryItemType': '(TagName="")',
            'DisplayName': 'INVTEXT("")', 'Icon': 'None', 'Position': position, 'bEnabled': 'True'}))
    rows = mappings(imc)
    for n, key in KEYS.items():
        matches = [r for r in rows if key_name(r) == key]
        if n == 2:
            require(len(matches) == 1 and package(matches[0].get_editor_property('action')) in (LEGACY_TWO, ACTIONS[2]),
                    'Expected exactly Two -> RT_SecondaryWeapon (or this helper\'s replacement)')
        else:
            require(not matches or (len(matches) == 1 and package(matches[0].get_editor_property('action')) == ACTIONS[n]),
                    key + ' conflicts with another Pawn IMC action')
            if matches:
                require(not matches[0].get_editor_property('triggers') and not matches[0].get_editor_property('modifiers'),
                        key + ' mapping behavior was customized; inspect before authoring')
        require(all(key_name(r) == key for r in rows if package(r.get_editor_property('action')) == ACTIONS[n]),
                'A managed action has unexpected extra mappings')
    return bp, cdo, quickbar, imc, root_bp, root_cdo, fist_bp, slots, actions


def verify_action(action, number):
    require(isinstance(action, unreal.InputAction)
            and unreal.EditorAssetLibrary.get_metadata_tag(action, OWNER_KEY) == SCHEMA,
            'Unexpected/unowned action: ' + ACTIONS[number])
    require(action.get_editor_property('value_type') == unreal.InputActionValueType.BOOLEAN
            and action.get_editor_property('consume_input')
            and not action.get_editor_property('triggers') and not action.get_editor_property('modifiers'),
            'Owned action behavior was customized; inspect instead of replacing')
    settings = action.get_editor_property('player_mappable_key_settings')
    require(settings and str(settings.get_editor_property('name')) == 'AZ.QuickSlot.' + str(number),
            'Player-mappable settings differ')


def create_action(number):
    path = ACTIONS[number]
    action = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if action is None:
        folder, name = path.rsplit('/', 1)
        action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.InputAction, None)
        require(action, 'Failed to create ' + path)
        action.modify()
        unreal.EditorAssetLibrary.set_metadata_tag(action, OWNER_KEY, SCHEMA)
        set_checked(action, 'value_type', unreal.InputActionValueType.BOOLEAN)
        set_checked(action, 'consume_input', True)
        set_checked(action, 'triggers', [])
        set_checked(action, 'modifiers', [])
        settings = unreal.new_object(unreal.PlayerMappableKeySettings, outer=action,
                                     name='QuickSlot' + str(number) + 'KeySettings')
        set_checked(settings, 'name', unreal.Name('AZ.QuickSlot.' + str(number)))
        set_checked(settings, 'display_name', unreal.Text('Quick Slot ' + str(number)))
        set_checked(settings, 'display_category', unreal.Text('Quick Select'))
        set_checked(action, 'player_mappable_key_settings', settings)
    verify_action(action, number)
    return action


def assign():
    bp, cdo, quickbar, imc, root_bp, root_cdo, fist_bp, slots, old_actions = prepare()
    # Preflight any existing actions before creating or modifying anything.
    for n, path in ACTIONS.items():
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            verify_action(load(path, unreal.InputAction), n)
    new_actions = {n: create_action(n) for n in ACTIONS}
    data = imc.get_editor_property('default_key_mappings').copy()
    rows = [r.copy() for r in data.get_editor_property('mappings')]
    for n, key_text in KEYS.items():
        matches = [r for r in rows if key_name(r) == key_text]
        if matches:
            # Two keeps every mapping option, modifier and trigger; only its action changes.
            matches[0].set_editor_property('action', new_actions[n])
        else:
            row = unreal.EnhancedActionKeyMapping()
            key = unreal.Key()
            require(key.import_text(key_text), 'Key import failed: ' + key_text)
            row.set_editor_property('action', new_actions[n])
            row.set_editor_property('key', key)
            rows.append(row)
    data.set_editor_property('mappings', rows)
    for obj in (bp, cdo, quickbar, imc, root_bp, root_cdo):
        obj.modify()
    set_checked(imc, 'default_key_mappings', data, no_notify=True)
    set_checked(cdo, 'WeaponSlotActions', old_actions[:2] + [new_actions[n] for n in (2, 3, 4)], no_notify=True)
    set_checked(quickbar, 'Slots', slots, no_notify=True)
    set_checked(root_cdo, 'CenterEntryWidgetClass', fist_bp.generated_class(), no_notify=True)


def verify_assignment():
    _, cdo, quickbar, imc, _, root_cdo, fist_bp, expected, _ = prepare()
    actual = list(quickbar.get_editor_property('Slots'))
    require(len(actual) == 5 and [serial(s) for s in actual] == [serial(s) for s in expected],
            'Manual quick-slot defaults differ')
    require(root_cdo.get_editor_property('CenterEntryWidgetClass') == fist_bp.generated_class(),
            'Fists center entry is unassigned')
    actions = list(cdo.get_editor_property('WeaponSlotActions'))
    require(len(actions) == 5, 'Expected five input action references')
    for n, path in ACTIONS.items():
        action = load(path, unreal.InputAction)
        verify_action(action, n)
        require(actions[n] == action, 'Controller slot action differs')
        rows = [r for r in mappings(imc) if key_name(r) == KEYS[n]]
        require(len(rows) == 1 and rows[0].get_editor_property('action') == action,
                'Missing or duplicate mapping: ' + KEYS[n])
        if n != 2:
            require(not rows[0].get_editor_property('triggers') and not rows[0].get_editor_property('modifiers'),
                    'New mapping behavior was customized')
    empty_guid = serial(unreal.Guid())
    require(all(serial(quickbar.get_bound_item_id(n)) == empty_guid for n in range(5)),
            'A controller default contains a runtime binding identity')
    require(serial(quickbar.get_ready_item_id()) == empty_guid, 'A controller default contains a ready-item identity')
    return {'slots': [s.export_text() for s in actual], 'weapon_slot_actions': [obj_path(a) for a in actions],
            'center_entry_class': obj_path(fist_bp.generated_class()), 'initial_bindings_empty': True,
            'manual_only_source_checked': True}


def main(mode='audit'):
    try:
        require(mode in ('audit', 'backup', 'assign', 'verify'), 'Unknown mode')
        if mode == 'audit':
            result = snapshot()
            write_report('audit-readback.json', result)
            return result
        if mode == 'backup':
            return backup()
        saved = baseline()
        before = saved['snapshot']
        assert_preserved(before)
        if mode == 'assign':
            try:
                ready = verify_assignment()
            except Exception:
                ready = None
            if ready and not set(MUTATED).intersection(dirty_packages()):
                return {'schema': SCHEMA, 'status': 'already_assigned', 'assignment': ready,
                        'compile_after_return': [], 'explicit_save_packages': [], 'no_pie': True}
            baseline(check_disk=True)
            require(not set(STRICT_BASELINE).intersection(dirty_packages()), 'Controller/Pawn IMC has unsaved changes')
            assign()
        result = {'schema': SCHEMA, 'status': 'requires_compile_and_save' if mode == 'assign' else 'readback_passed',
                  'assignment': verify_assignment(), 'snapshot': assert_preserved(before),
                  'backup': saved['backup'], 'compile_after_return': [WIDGET, PC] if mode == 'assign' else [],
                  'explicit_save_packages': list(MUTATED) if mode == 'assign' else [],
                  'required_existing_dependencies': [ENTRY, FISTS, FISTS_ICON], 'no_pie': True}
        if mode == 'verify':
            require(not set(MUTATED).intersection(dirty_packages()), 'Save explicit target packages before final verification')
        write_report(mode + '-readback.json', result)
        return result
    finally:
        gc.collect()


if __name__ == '__main__':
    print(json.dumps(main(), indent=2))
