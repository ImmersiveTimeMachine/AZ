# @Description: Audit, back up, and append four manual Quick Select shortcuts without replacing existing slots.
"""Ordinary Unreal Python; default main('audit') is read-only, including during PIE.

Backup/assign/verify require no PIE. This helper never starts or stops PIE,
compiles Blueprints, saves packages, migrates live bindings, resets QuickBar
state, or changes item manifests. It appends slots/actions 5-8 while preserving
the exact existing slots/actions 0-4 and every prior mapping. Slot 0 remains
the existing Fists/mode definition. New slots repeat Left/Right/Up/Down; native
PositionOrdinal chooses the second host per direction.

Sequence: main('backup') after PIE stops; full native build/restart; author the
eight-host widget separately; main('assign'); dedicated controller compilation
and explicit saves; main('verify') using fresh CDOs. Never rerun the older
five-slot or two-slot authoring helpers to update this layout.
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
OUTPUT = ROOT / 'Saved/QuickSelectEightSlots'
RECEIPT = OUTPUT / 'backup-receipt.json'
SCHEMA = 'quick_select_eight_slots:v1'
OWNER_KEY = 'AZ.QuickSelect.EightSlots.Owner'
PC = '/Game/AZ/Blueprints/Player/BP_AZ_PlayerController'
PAWN_IMC = '/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IMC_RT_PawnInputs'
SHARED_IMC = '/Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IMC_AlwaysAllowed'
ACTION_FOLDER = '/Game/AZ/Blueprints/Input/AlwaysAllowed'
ACTIONS = {n: ACTION_FOLDER + '/AZ_IA_QuickSlot_' + str(n) for n in range(5, 9)}
KEYS = {5: 'Five', 6: 'Six', 7: 'Seven', 8: 'Eight'}
POSITIONS = {5: 'Left', 6: 'Right', 7: 'Up', 8: 'Down'}
MUTATED = (PC, PAWN_IMC, *ACTIONS.values())
BACKED_UP = (*MUTATED, SHARED_IMC)
PROTECTED_PC = ('SharedInputMappingContext', 'ChangeFireModeAction', 'InventoryHudWidgetClass',
                'QuickSelectToggleAction', 'OpenInventoryAction', 'InputConfig')

# Only guarded helper definitions are imported. Their main/context/assignment
# functions are not called: those helpers target older slot counts.
H = runpy.run_path(str(ROOT / 'Tools/quick_select_assign.py'), run_name='eight_slot_helpers')
T = runpy.run_path(str(ROOT / 'Tools/rifle_p01_activate.py'), run_name='eight_slot_struct_helpers')
require, serial, load = H['require'], H['serial'], H['load']
obj_path, dirty_packages = H['obj_path'], H['dirty_packages']
mappings, key_name, set_checked = H['mappings'], H['key_name'], H['set_checked']


def package(value):
    return obj_path(value).split('.')[0] if value else None


def write_report(name, data):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / name
    tmp = path.with_suffix('.json.tmp')
    tmp.write_text(json.dumps(data, indent=2), encoding='utf-8')
    tmp.replace(path)


def package_file(asset):
    require(asset in BACKED_UP, 'Unexpected asset: ' + asset)
    content = (ROOT / 'Content').resolve()
    target = (content / (asset[len('/Game/'):] + '.uasset')).resolve()
    require(target.is_relative_to(content), 'Backup path escaped Content')
    return target


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def context(allow_pie=False):
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    require(project == ROOT.resolve(), 'Wrong Unreal project')
    pie = bool(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world())
    require(allow_pie or not pie, 'Stop PIE before eight-slot backup or authoring; never migrate live bindings')
    bp = load(PC, unreal.Blueprint)
    cdo = unreal.get_default_object(bp.generated_class())
    quickbar = cdo.get_editor_property('QuickBar')
    require(quickbar and package(cdo.get_editor_property('SharedInputMappingContext')) == SHARED_IMC,
            'Expected controller QuickBar/shared context changed')
    return bp, cdo, quickbar, load(PAWN_IMC, unreal.InputMappingContext), pie


def record_mapping(row):
    return {'key': key_name(row), 'action': package(row.get_editor_property('action')),
            'export': row.export_text()}


def snapshot(allow_pie=False):
    bp, cdo, quickbar, imc, pie = context(allow_pie)
    slots = list(quickbar.get_editor_property('Slots'))
    require(len(slots) in (5, 9), 'Expected five current or nine completed slot definitions')
    return {'controller': obj_path(bp), 'pie_active': pie, 'template_only': True,
            'controller_refs': {name: serial(cdo.get_editor_property(name)) for name in PROTECTED_PC},
            'quick_select_widget': obj_path(cdo.get_editor_property('QuickSelect').get_editor_property('WidgetClass')),
            'slot_exports': [s.export_text() for s in slots],
            'weapon_slot_actions': [obj_path(a) for a in cdo.get_editor_property('WeaponSlotActions')],
            'default_bindings_0_4': [{'id': serial(quickbar.get_bound_item_id(n)),
                                    'explicit': quickbar.is_binding_explicit(n)} for n in range(5)],
            'default_binding_revision': quickbar.get_binding_revision(),
            'default_ready_item_id': serial(quickbar.get_ready_item_id()),
            'pawn_mappings': [record_mapping(row) for row in mappings(imc)],
            'shared_mappings': [record_mapping(row) for row in mappings(load(SHARED_IMC, unreal.InputMappingContext))],
            'dirty_packages': dirty_packages()}


def backup():
    before = snapshot()
    require(not {PC, PAWN_IMC, SHARED_IMC}.intersection(before['dirty_packages']),
            'Controller/input context has unsaved changes; preserve those before backup')
    destination = ROOT / 'Saved/Backups/QuickSelectEightSlots' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    destination.mkdir(parents=True)
    files = []
    for asset in BACKED_UP:
        source = package_file(asset)
        row = {'package': asset, 'existed': source.is_file()}
        if source.is_file():
            saved = destination / source.relative_to(ROOT / 'Content')
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, saved)
            require(sha256(source) == sha256(saved), 'Backup mismatch: ' + asset)
            row.update(source=str(source), backup=str(saved), sha256=sha256(source))
        files.append(row)
    result = {'schema': SCHEMA, 'status': 'backed_up', 'backup': str(destination),
              'files': files, 'snapshot': before}
    (destination / 'receipt.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    write_report(RECEIPT.name, result)
    return result


def baseline(check_disk=False):
    require(RECEIPT.is_file(), 'Run main("backup") after PIE stops')
    result = json.loads(RECEIPT.read_text(encoding='utf-8'))
    require(result.get('schema') == SCHEMA and {r['package'] for r in result['files']} == set(BACKED_UP),
            'Wrong or incomplete eight-slot backup')
    if check_disk:
        for row in result['files']:
            if row['package'] in (PC, PAWN_IMC, SHARED_IMC):
                path = package_file(row['package'])
                require(path.is_file() and sha256(path) == row.get('sha256'), 'Input/controller baseline changed; refresh backup')
    return result


def assert_preserved(before):
    after = snapshot()
    for name in ('controller_refs', 'quick_select_widget', 'default_bindings_0_4',
                 'default_binding_revision', 'default_ready_item_id', 'shared_mappings'):
        require(after[name] == before[name], 'Unrelated data changed: ' + name)
    require(after['slot_exports'][:5] == before['slot_exports'][:5], 'Existing slots 0-4 changed')
    require(after['weapon_slot_actions'][:5] == before['weapon_slot_actions'][:5], 'Existing shortcut actions 0-4 changed')
    retained = lambda rows: [r['export'] for r in rows if r['action'] not in ACTIONS.values()]
    require(retained(after['pawn_mappings']) == retained(before['pawn_mappings']), 'Existing Pawn IMC mappings changed/reordered')
    return after


def native_guard():
    # The old build can store larger arrays but renders only one card per
    # direction. Require the new loaded view schema before assigning any assets.
    unreal.AZ_QuickSelectEntryView().get_editor_property('PositionOrdinal')
    unreal.AZ_QuickSelectView().get_editor_property('ModeText')


def new_slot(position):
    value = unreal.AZ_QuickSlot()
    fields = {'WeaponTag': '(TagName="")', 'WeaponAbilities': '()', 'EffectsOnEquip': '()',
              'bStrafeOnEquip': 'False', 'bInventoryBacked': 'True', 'InventoryItemType': '(TagName="")',
              'DisplayName': 'INVTEXT("")', 'Icon': 'None', 'Position': position, 'bEnabled': 'True'}
    text = T['fields'](value.export_text())
    for key, content in fields.items():
        value.get_editor_property(key)
        text = T['replace_field'](text, key, content)
    require(value.import_text(T['encode_fields'](text)), 'New slot struct import failed')
    return value


def verify_action(action, number):
    require(isinstance(action, unreal.InputAction) and
            unreal.EditorAssetLibrary.get_metadata_tag(action, OWNER_KEY) == SCHEMA, 'Unowned/wrong new shortcut action')
    require(action.get_editor_property('value_type') == unreal.InputActionValueType.BOOLEAN
            and action.get_editor_property('consume_input')
            and not action.get_editor_property('triggers') and not action.get_editor_property('modifiers'),
            'New action behavior changed; inspect instead of replacing it')
    settings = action.get_editor_property('player_mappable_key_settings')
    require(settings and str(settings.get_editor_property('name')) == 'AZ.QuickSlot.' + str(number),
            'Player-mappable action identifier differs')


def preflight():
    native_guard()
    bp, cdo, quickbar, imc, _ = context()
    old_slots = list(quickbar.get_editor_property('Slots'))
    old_actions = list(cdo.get_editor_property('WeaponSlotActions'))
    require(len(old_slots) in (5, 9) and len(old_actions) == len(old_slots), 'Slot/action counts differ')
    require(not old_slots[0].get_editor_property('bInventoryBacked')
            and str(old_slots[0].get_editor_property('WeaponTag').get_editor_property('tag_name')) == 'Weapon.Fist',
            'Slot 0 is no longer the audited intrinsic definition')
    require(all(old_slots[n].get_editor_property('bInventoryBacked') and old_actions[n] for n in range(1, 5)),
            'Existing manual slots/actions are incomplete')
    slots = [slot.copy() for slot in old_slots[:5]] + [new_slot(POSITIONS[n]) for n in range(5, 9)]
    if len(old_slots) == 9:
        require([s.export_text() for s in old_slots[5:]] == [s.export_text() for s in slots[5:]],
                'Existing slots 5-8 were customized; do not overwrite them')
        require(all(package(old_actions[n]) == ACTIONS[n] for n in ACTIONS), 'Existing actions 5-8 differ')
    rows = mappings(imc)
    shared_rows = mappings(load(SHARED_IMC, unreal.InputMappingContext))
    for n, key in KEYS.items():
        matches = [r for r in rows if key_name(r) == key]
        require(not matches or (len(matches) == 1 and package(matches[0].get_editor_property('action')) == ACTIONS[n]
                and not matches[0].get_editor_property('triggers') and not matches[0].get_editor_property('modifiers')),
                'Pawn mapping conflicts with ' + key)
        require(not any(key_name(r) == key for r in shared_rows), 'Shared mapping conflicts with ' + key)
        require(all(key_name(r) == key for r in rows if package(r.get_editor_property('action')) == ACTIONS[n]),
                'New shortcut action has unexpected extra mappings')
        if unreal.EditorAssetLibrary.does_asset_exist(ACTIONS[n]):
            verify_action(load(ACTIONS[n], unreal.InputAction), n)
    return bp, cdo, quickbar, imc, slots, old_actions


def create_action(number):
    path = ACTIONS[number]
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        result = load(path, unreal.InputAction)
        verify_action(result, number)
        return result
    folder, name = path.rsplit('/', 1)
    result = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.InputAction, None)
    require(result, 'Could not create ' + path)
    result.modify()
    unreal.EditorAssetLibrary.set_metadata_tag(result, OWNER_KEY, SCHEMA)
    for prop, value in (('value_type', unreal.InputActionValueType.BOOLEAN), ('consume_input', True), ('triggers', []), ('modifiers', [])):
        set_checked(result, prop, value)
    settings = unreal.new_object(unreal.PlayerMappableKeySettings, outer=result, name='QuickSlot' + str(number) + 'KeySettings')
    set_checked(settings, 'name', unreal.Name('AZ.QuickSlot.' + str(number)))
    set_checked(settings, 'display_name', unreal.Text('Quick Slot ' + str(number)))
    set_checked(settings, 'display_category', unreal.Text('Quick Select'))
    set_checked(result, 'player_mappable_key_settings', settings)
    verify_action(result, number)
    return result


def assign():
    bp, cdo, quickbar, imc, slots, previous_actions = preflight()
    actions = {n: create_action(n) for n in ACTIONS}
    data = imc.get_editor_property('default_key_mappings').copy()
    rows = [row.copy() for row in data.get_editor_property('mappings')]
    for n, key_name_text in KEYS.items():
        if any(package(r.get_editor_property('action')) == ACTIONS[n] for r in rows):
            continue
        row = unreal.EnhancedActionKeyMapping()
        key = unreal.Key()
        require(key.import_text(key_name_text), 'Cannot import key ' + key_name_text)
        row.set_editor_property('key', key)
        row.set_editor_property('action', actions[n])
        rows.append(row)
    data.set_editor_property('mappings', rows)
    for obj in (bp, cdo, quickbar, imc):
        obj.modify()
    set_checked(imc, 'default_key_mappings', data, no_notify=True)
    set_checked(cdo, 'WeaponSlotActions', previous_actions[:5] + [actions[n] for n in range(5, 9)], no_notify=True)
    set_checked(quickbar, 'Slots', slots, no_notify=True)


def verify_assignment():
    _, cdo, quickbar, imc, expected, _ = preflight()
    actual = list(quickbar.get_editor_property('Slots'))
    actions = list(cdo.get_editor_property('WeaponSlotActions'))
    require(len(actual) == len(actions) == 9, 'Expected mode 0 plus eight manual shortcuts')
    require([s.export_text() for s in actual] == [s.export_text() for s in expected], 'Slot definitions differ')
    for n in range(5, 9):
        require(package(actions[n]) == ACTIONS[n], 'Slot action differs: ' + str(n))
        rows = [r for r in mappings(imc) if key_name(r) == KEYS[n]]
        require(len(rows) == 1 and rows[0].get_editor_property('action') == actions[n], 'Missing/duplicate new shortcut')
    return {'slots': [s.export_text() for s in actual], 'actions': [obj_path(a) for a in actions],
            'position_order': ['Center', 'Left', 'Right', 'Up', 'Down', 'Left', 'Right', 'Up', 'Down'],
            'bindings': 'No runtime or default binding/readiness fields were assigned'}


def main(mode='audit'):
    try:
        require(mode in ('audit', 'backup', 'assign', 'verify'), 'Unknown mode')
        if mode == 'audit':
            result = snapshot(allow_pie=True)
            write_report('audit-readback.json', result)
            return result
        if mode == 'backup':
            return backup()
        saved = baseline()
        assert_preserved(saved['snapshot'])
        if mode == 'assign':
            try:
                ready = verify_assignment()
            except Exception:
                ready = None
            if ready and not set(MUTATED).intersection(dirty_packages()):
                return {'schema': SCHEMA, 'status': 'already_assigned', 'assignment': ready,
                        'compile_after_return': [], 'explicit_save_packages': []}
            baseline(check_disk=True)
            require(not {PC, PAWN_IMC}.intersection(dirty_packages()), 'Controller/Pawn IMC has unsaved edits')
            assign()
        result = {'schema': SCHEMA, 'status': 'requires_compile_and_save' if mode == 'assign' else 'readback_passed',
                  'assignment': verify_assignment(), 'snapshot': assert_preserved(saved['snapshot']),
                  'backup': saved['backup'], 'no_pie': True,
                  'compile_after_return': [PC] if mode == 'assign' else [],
                  'explicit_save_packages': list(MUTATED) if mode == 'assign' else []}
        if mode == 'verify':
            require(not set(MUTATED).intersection(dirty_packages()), 'Save explicit targets before verification')
        write_report(mode + '-readback.json', result)
        return result
    finally:
        gc.collect()


if __name__ == '__main__':
    print(json.dumps(main(), indent=2))
