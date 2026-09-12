# @Description: Audit, back up, and configure eight total CHALK quick-select cells (0–7).
"""Read-only by default; no Blueprint compilation, saves, tests, or PIE control.

Retains the exact gameplay/presentation data of definitions 0–7 except Position.
Retires only definition/action reference 8 and its owned Eight mapping. The
InputAction asset is retained. Inventory items, bindings, readiness, and mode
policy are never assigned. Root/Mode widget layout and icon changes are a
separate authoring task. Run backup -> assign -> dedicated PC compile/save ->
verify, with PIE stopped for all modes except the template-only audit.
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
OUTPUT = ROOT / 'Saved/QuickSelectCompactSlots'
RECEIPT = OUTPUT / 'backup-receipt.json'
SCHEMA = 'quick_select_compact_slots:v1'
PC = '/Game/AZ/Blueprints/Player/BP_AZ_PlayerController'
PAWN_IMC = '/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IMC_RT_PawnInputs'
SHARED_IMC = '/Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IMC_AlwaysAllowed'
WIDGET = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect/WBP_AZ_QuickSelect'
MODE_WIDGET = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect/WBP_AZ_QuickSelectFists'
RETIRED_ACTION = '/Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IA_QuickSlot_8'
POSITIONS = ('Left', 'Left', 'Up', 'Up', 'Right', 'Right', 'Down', 'Down')
MUTATED = (PC, PAWN_IMC)
BACKED_UP = (PC, PAWN_IMC, WIDGET, MODE_WIDGET, SHARED_IMC, RETIRED_ACTION)
PROTECTED_PC = ('SharedInputMappingContext', 'ChangeFireModeAction', 'InventoryHudWidgetClass',
                'QuickSelectToggleAction', 'OpenInventoryAction', 'InputConfig')

# Import guarded definitions only. Older assignment routines target different
# slot counts and are intentionally never called by this helper.
H = runpy.run_path(str(ROOT / 'Tools/quick_select_assign.py'), run_name='compact_slot_helpers')
E = runpy.run_path(str(ROOT / 'Tools/quick_select_eight_slots.py'), run_name='compact_retired_action_helpers')
T = runpy.run_path(str(ROOT / 'Tools/rifle_p01_activate.py'), run_name='compact_slot_text_helpers')
require, serial, load = H['require'], H['serial'], H['load']
obj_path, dirty_packages = H['obj_path'], H['dirty_packages']
mappings, key_name, set_checked = H['mappings'], H['key_name'], H['set_checked']


def package(obj):
    return obj_path(obj).split('.')[0] if obj else None


def write_report(name, data):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    target = OUTPUT / name
    temporary = target.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(data, indent=2), encoding='utf-8')
    temporary.replace(target)


def package_file(asset):
    require(asset in BACKED_UP and asset.startswith('/Game/'), 'Unexpected package: ' + asset)
    content = (ROOT / 'Content').resolve()
    path = (content / (asset[len('/Game/'):] + '.uasset')).resolve()
    require(path.is_relative_to(content), 'Package escaped Content')
    return path


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def context(allow_pie=False):
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    require(project == ROOT.resolve(), 'Wrong Unreal project')
    pie = bool(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world())
    require(allow_pie or not pie, 'Stop PIE before compact-slot authoring; no live binding migration')
    bp = load(PC, unreal.Blueprint)
    cdo = unreal.get_default_object(bp.generated_class())
    quickbar = cdo.get_editor_property('QuickBar')
    require(quickbar and package(cdo.get_editor_property('SharedInputMappingContext')) == SHARED_IMC,
            'Expected QuickBar/shared input context changed')
    return bp, cdo, quickbar, load(PAWN_IMC, unreal.InputMappingContext), pie


def retained_slot(slot):
    return [[key, value] for key, value in T['fields'](slot.export_text()) if key != 'Position']


def record_mapping(row):
    return {'key': key_name(row), 'action': package(row.get_editor_property('action')),
            'export': row.export_text()}


def is_retired(row):
    return row['action'] == RETIRED_ACTION and row['key'] == 'Eight'


def snapshot(allow_pie=False):
    bp, cdo, quickbar, imc, pie = context(allow_pie)
    slots = list(quickbar.get_editor_property('Slots'))
    actions = list(cdo.get_editor_property('WeaponSlotActions'))
    require(len(slots) in (8, 9) and len(actions) == len(slots),
            'Expected current nine cells or completed eight cells with matching actions')
    root_cdo = unreal.get_default_object(load(WIDGET, unreal.WidgetBlueprint).generated_class())
    return {'controller': obj_path(bp), 'template_only': True, 'pie_active': pie,
            'controller_refs': {n: serial(cdo.get_editor_property(n)) for n in PROTECTED_PC},
            'selector_class': obj_path(cdo.get_editor_property('QuickSelect').get_editor_property('WidgetClass')),
            'root_entry_class': obj_path(root_cdo.get_editor_property('EntryWidgetClass')),
            'root_mode_class': obj_path(root_cdo.get_editor_property('CenterEntryWidgetClass')),
            'slot_exports': [s.export_text() for s in slots],
            'retained_slot_fields_0_7': [retained_slot(s) for s in slots[:8]],
            'weapon_slot_actions': [obj_path(a) for a in actions],
            'default_bindings_0_7': [{'id': serial(quickbar.get_bound_item_id(n)),
                                    'explicit': quickbar.is_binding_explicit(n)} for n in range(8)],
            'default_binding_revision': quickbar.get_binding_revision(),
            'default_ready_item': serial(quickbar.get_ready_item_id()),
            'restore_last_weapon_on_fight': quickbar.get_editor_property('bRestoreLastWeaponOnFight'),
            'pawn_mappings': [record_mapping(r) for r in mappings(imc)],
            'shared_mappings': [record_mapping(r) for r in mappings(load(SHARED_IMC, unreal.InputMappingContext))],
            'dirty_packages': dirty_packages()}


def backup():
    before = snapshot()
    require(not {PC, PAWN_IMC, SHARED_IMC, RETIRED_ACTION}.intersection(before['dirty_packages']),
            'Controller/input assets have unsaved changes; preserve them before backup')
    destination = ROOT / 'Saved/Backups/QuickSelectCompactSlots' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    destination.mkdir(parents=True)
    files = []
    for asset in BACKED_UP:
        source = package_file(asset)
        require(source.is_file(), 'Expected saved backup source is missing: ' + asset)
        target = destination / source.relative_to(ROOT / 'Content')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        require(sha256(source) == sha256(target), 'Backup verification failed: ' + asset)
        files.append({'package': asset, 'source': str(source), 'backup': str(target), 'sha256': sha256(source)})
    result = {'schema': SCHEMA, 'status': 'backed_up', 'backup': str(destination), 'files': files,
              'snapshot': before, 'note': 'Widget file copies preserve saved disk state; current dirty widget packages are listed.'}
    (destination / 'receipt.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    write_report(RECEIPT.name, result)
    return result


def baseline(check_disk=False):
    require(RECEIPT.is_file(), 'Run main("backup") first with PIE stopped')
    data = json.loads(RECEIPT.read_text(encoding='utf-8'))
    require(data.get('schema') == SCHEMA and {r['package'] for r in data['files']} == set(BACKED_UP),
            'Wrong or incomplete compact-slot backup')
    for row in data['files']:
        # The retired action and shared mappings are preserved even after saving
        # the actual targets. Root/Mode widget layout is authored separately.
        if row['package'] in (SHARED_IMC, RETIRED_ACTION) or (check_disk and row['package'] in MUTATED):
            path = package_file(row['package'])
            require(path.is_file() and sha256(path) == row['sha256'],
                    'Protected/baseline package changed: ' + row['package'])
    return data


def assert_preserved(before):
    after = snapshot()
    for name in ('controller_refs', 'selector_class', 'root_entry_class', 'root_mode_class',
                 'retained_slot_fields_0_7', 'default_bindings_0_7', 'default_binding_revision',
                 'default_ready_item', 'restore_last_weapon_on_fight', 'shared_mappings'):
        require(after[name] == before[name], 'Protected data changed: ' + name)
    require(after['weapon_slot_actions'][:8] == before['weapon_slot_actions'][:8],
            'A retained action reference changed')
    require([r['export'] for r in after['pawn_mappings'] if not is_retired(r)] ==
            [r['export'] for r in before['pawn_mappings'] if not is_retired(r)],
            'An unrelated Pawn IMC mapping changed or reordered')
    return after


def preflight():
    require(hasattr(unreal.AZ_QuickBarComponent, 'is_fight_mode'), 'Mode API is not loaded')
    unreal.AZ_QuickSelectEntryView().get_editor_property('PositionOrdinal')
    bp, cdo, quickbar, imc, _ = context()
    slots = list(quickbar.get_editor_property('Slots'))
    actions = list(cdo.get_editor_property('WeaponSlotActions'))
    require(len(slots) in (8, 9) and len(actions) == len(slots), 'Unexpected slot/action count')
    require(not slots[0].get_editor_property('bInventoryBacked')
            and str(slots[0].get_editor_property('WeaponTag').get_editor_property('tag_name')) == 'Weapon.Fist',
            'Mode 0 intrinsic gameplay definition changed')
    require(all(s.get_editor_property('bEnabled') for s in slots[:8]), 'A retained cell is disabled')
    require(all(s.get_editor_property('bInventoryBacked') for s in slots[1:8]) and all(actions[:8]),
            'Retained manual cells/actions are incomplete')
    retired = load(RETIRED_ACTION, unreal.InputAction)
    E['verify_action'](retired, 8)  # Require ownership metadata and known action behavior.
    if len(slots) == 9:
        require(actions[8] == retired and slots[8].get_editor_property('bInventoryBacked'),
                'The ninth cell/action is not the owned shortcut being retired')
        require(serial(quickbar.get_bound_item_id(8)) == serial(unreal.Guid()),
                'Default cell 8 has an item identity; do not silently retire an authored binding')
    own_rows = [r for r in mappings(imc) if package(r.get_editor_property('action')) == RETIRED_ACTION]
    require(len(own_rows) <= 1 and all(key_name(r) == 'Eight' for r in own_rows),
            'Retired action has customized/extra mappings; review before removal')
    if len(slots) == 9:
        require(len(own_rows) == 1, 'Expected owned Eight mapping is missing')
    for row in own_rows:
        require(not row.get_editor_property('triggers') and not row.get_editor_property('modifiers'),
                'Retired Eight mapping behavior was customized')
    replacements = []
    for index, original in enumerate(slots[:8]):
        value = original.copy()
        value.get_editor_property('Position')
        text = T['replace_field'](T['fields'](value.export_text()), 'Position', POSITIONS[index])
        require(value.import_text(T['encode_fields'](text)), 'Position import failed')
        require(retained_slot(value) == retained_slot(original), 'A field other than Position changed')
        replacements.append(value)
    return bp, cdo, quickbar, imc, replacements, actions[:8]


def assign():
    bp, cdo, quickbar, imc, slots, actions = preflight()
    data = imc.get_editor_property('default_key_mappings').copy()
    rows = [r.copy() for r in data.get_editor_property('mappings') if not is_retired(record_mapping(r))]
    data.set_editor_property('mappings', rows)
    for obj in (bp, cdo, quickbar, imc):
        obj.modify()
    set_checked(quickbar, 'Slots', slots, no_notify=True)
    set_checked(cdo, 'WeaponSlotActions', actions, no_notify=True)
    set_checked(imc, 'default_key_mappings', data, no_notify=True)


def verify_assignment():
    _, cdo, quickbar, imc, expected, _ = preflight()
    slots = list(quickbar.get_editor_property('Slots'))
    actions = list(cdo.get_editor_property('WeaponSlotActions'))
    require(len(slots) == len(actions) == 8, 'Expected eight TOTAL cells/actions, 0–7')
    require([s.export_text() for s in slots] == [s.export_text() for s in expected], 'Compact position order differs')
    require(not any(package(r.get_editor_property('action')) == RETIRED_ACTION for r in mappings(imc)),
            'Retired Eight mapping remains')
    require(unreal.EditorAssetLibrary.does_asset_exist(RETIRED_ACTION), 'Retired action asset must remain on disk')
    return {'total_cells': 8, 'indices': list(range(8)), 'position_order': list(POSITIONS),
            'slot_exports': [s.export_text() for s in slots], 'actions': [obj_path(a) for a in actions],
            'retired_action_asset_preserved': RETIRED_ACTION,
            'state': 'No inventory, binding, readiness, or combat policy fields were assigned'}


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
            require(not set(MUTATED).intersection(dirty_packages()), 'Controller/Pawn IMC has unsaved changes')
            assign()
        result = {'schema': SCHEMA, 'status': 'requires_compile_and_save' if mode == 'assign' else 'readback_passed',
                  'assignment': verify_assignment(), 'snapshot': assert_preserved(saved['snapshot']),
                  'backup': saved['backup'], 'no_pie': True, 'compile_after_return': [PC] if mode == 'assign' else [],
                  'explicit_save_packages': list(MUTATED) if mode == 'assign' else [],
                  'widget_layout_authored_separately': [WIDGET, MODE_WIDGET]}
        if mode == 'verify':
            require(not set(MUTATED).intersection(dirty_packages()), 'Save explicit targets before final verification')
        write_report(mode + '-readback.json', result)
        return result
    finally:
        gc.collect()


if __name__ == '__main__':
    print(json.dumps(main(), indent=2))
