# @Description: Audit or prepare the existing P01 reference loops, BranchIn databases and rifle chooser.
"""Run in Unreal Python. main() is read-only; main(prepare=True) authors assets.

This file does not retarget, duplicate animation sequences, author contacts, run
PIE/tests, open animation previews, build an index, or activate a profile/AnimBP.
The controller, aim offset, profile defaults and graph are authored separately.

All 103 source chooser rows survive in the owned duplicate. Generic old idle,
idle-break and locomotion rows are disabled; reaction-specific rows and the
existing shared sprint row survive. Four P01 idle rows and 48 directional loop
rows are appended (155 total, 53 replacement/shared idle-locomotion choices).
Transitions remain explicitly shared source rows, never renamed P01 loops.

Loop metadata and BranchIn links modify the existing P01 clips. Before writing,
the script backs up their on-disk packages and refuses unsaved target packages.
Neither loop flags nor BranchIn links imply verified foot contacts/index quality.
"""

import gc
import hashlib
import json
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal


P01_ROOT = '/Game/AZ/Assets/RTG/Riffle_P01'
OWNED_ROOT = '/Game/AZ/Blueprints/Animation/MotionMatching/RifleP01'
SCHEMA_PATH = '/Game/AZ/Blueprints/Animation/MotionMatching/PSS_v2_SurvivalMan_Loco'
SOURCE_CHOOSER = '/Game/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations'
TARGET_CHOOSER = OWNED_ROOT + '/CHT_P01_CharacterAnimations'
EXPECTED_SOURCE_ROWS = 103
OWNER_KEY = 'AZ.RifleP01.Owner'
MANIFEST_KEY = 'AZ.RifleP01.Manifest'
STATE_KEY = 'AZ.RifleP01.State'
OWNER = 'rifle_p01_setup:v1'

# Exact native EAZ_EightWayDirection names -> the P01 library's direction tokens.
DIRECTIONS = (
    ('F', 'F'), ('FR', 'FR'), ('R', 'R'), ('BR', 'BR_BkPd'),
    ('B', 'B'), ('BL', 'BL_BkPd'), ('L', 'L'), ('FL', 'FL'),
)

# Explicit six families: raw-clip MM searches only the chooser-selected sequence.
# All five native profile DB override properties must remain null for this plan.
FAMILIES = (
    ('WalkExplore', 'Walk/Locomotion/IPC', 'Riffle_P_W2_Walk_', 'Walk', 'Standing', False),
    ('WalkAim', 'Walk/Aim/IPC', 'Riffle_P_W2_Walk_Aim_', 'Walk', 'Standing', True),
    ('JogExplore', 'Jog/Locomotion/IPC', 'Riffle_P_W2_Jog_', 'Run', 'Standing', False),
    ('JogAim', 'Jog/Aim/IPC', 'Riffle_P_W2_Jog_Aim_', 'Run', 'Standing', True),
    ('CrouchExplore', 'CrouchWalk/Locomotion/IPC', 'Riffle_P_W2_CrouchWalk_', None, 'Crouching', False),
    ('CrouchAim', 'CrouchWalk/Locomotion/IPC', 'Riffle_P_W2_CrouchWalk_Aim_', None, 'Crouching', True),
)
IDLES = (
    ('Stand/Idle/IPC/Riffle_P_W2_Stand_Relaxed_Idle_IPC', 'Standing', False),
    ('Stand/Idle/IPC/Riffle_P_W2_Stand_Aim_Idle_IPC', 'Standing', True),
    ('Crouch/Idle/IPC/Riffle_P_W2_Crouch_Idle_IPC', 'Crouching', False),
    ('Crouch/Aim/IPC/Riffle_P_W2_Crouch_Aim_Idle_v2_IPC', 'Crouching', True),
)

EAL = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary
PU = unreal.AZ_PoseSearchUtils
CU = unreal.AZ_ChooserUtils


def package(asset):
    return str(asset.get_path_name()).split('.')[0]


def load(path, cls):
    asset = unreal.load_asset(path)
    if asset is None or not isinstance(asset, cls):
        raise RuntimeError('Missing asset or wrong type: ' + path)
    return asset


def save(asset):
    if not EAL.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Save failed: ' + package(asset))


def require(result, operation):
    if not result:
        raise RuntimeError(operation)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode('utf-8')).hexdigest()


def library_rows():
    """Exactly 52 explicit library selections; the source's sprint row is retained."""
    rows = []
    for suffix, stance, aiming in IDLES:
        rows.append(dict(asset=P01_ROOT + '/' + suffix, state='IdleLoop',
                         gait=None, stance=stance, aiming=aiming, direction=None,
                         database=None, use_mm=False))
    for family, folder, stem, gait, stance, aiming in FAMILIES:
        for direction, token in DIRECTIONS:
            rows.append(dict(asset=P01_ROOT + '/' + folder + '/' + stem + token + '_Loop_IPC',
                             state='LocomotionLoop', gait=gait, stance=stance,
                             aiming=aiming, direction=direction,
                             database=OWNED_ROOT + '/PSD_P01_' + family, use_mm=True))
    if len(rows) != 52 or len({row['asset'] for row in rows}) != 52:
        raise RuntimeError('P01 library table must contain 52 unique sequences')
    return rows


def database_manifests(rows):
    return {OWNED_ROOT + '/PSD_P01_' + family: {
        'schema': SCHEMA_PATH,
        'sequences': [row['asset'] for row in rows if row['database'] == OWNED_ROOT + '/PSD_P01_' + family],
        'membership_owner': 'one_full_length_BranchIn_per_sequence',
        'disable_reselection': True,
    } for family, *_ in FAMILIES}


def branch_notifies(seq):
    result = []
    for event in AL.get_animation_notify_events(seq):
        state = event.get_editor_property('notify_state_class')
        if isinstance(state, unreal.AnimNotifyState_PoseSearchBranchIn):
            db = state.get_editor_property('database')
            result.append(dict(database=package(db) if db else None,
                               start=float(AL.get_anim_notify_event_trigger_time(event)),
                               duration=float(AL.get_anim_notify_event_duration(event))))
    return result


def members(db):
    return [package(db.get_animation_asset(i)) for i in range(db.get_num_animation_assets())]


def chooser_disabled_rows(path):
    values = [bool(value) for value in CU.get_chooser_disabled_rows(path)]
    if len(values) != CU.get_row_count(path):
        raise RuntimeError('Disabled-row helper returned an inconsistent row count: ' + path)
    return values


def existing_owned(path, manifest, cls):
    if not EAL.does_asset_exist(path):
        return None
    asset = load(path, cls)
    if EAL.get_metadata_tag(asset, OWNER_KEY) != OWNER or EAL.get_metadata_tag(asset, MANIFEST_KEY) != digest(manifest):
        raise RuntimeError('Refusing unknown asset or changed authoring manifest: ' + path)
    return asset


def claim(asset, manifest, state):
    EAL.set_metadata_tag(asset, OWNER_KEY, OWNER)
    EAL.set_metadata_tag(asset, MANIFEST_KEY, digest(manifest))
    EAL.set_metadata_tag(asset, STATE_KEY, state)


def chooser_snapshot(path):
    """Parse only the documented diagnostic envelope/cells, never reconstruct it."""
    text = '\n'.join(str(line) for line in CU.dump_chooser_full_tree(path))
    columns = []
    rows = []
    for line in text.splitlines():
        if re.match(r'^\s*\{\s*"index"\s*:', line):
            columns.append(json.loads(line.strip().rstrip(',')))
        match = re.match(r'^\s*\{\s*"i"\s*:\s*(\d+)', line)
        if match:
            row = {'index': int(match.group(1)), 'line': line.strip()}
            # OutputStruct text can contain quoted nested fields. Only simple
            # enum/bool cells are interpreted; all source row data stays in-place.
            row['cells'] = {int(i): value for i, value in re.findall(r'"c(\d+)":\s*"([^"]*)"', line)}
            rows.append(row)
    contexts = re.search(r'"context_count":\s*(\d+)', text)
    nested = re.search(r'"nested":\s*\[\s*//\s*(\d+) nested', text)
    if not contexts or not nested or int(nested.group(1)) != 0:
        raise RuntimeError('Expected a flat source chooser; nested schema requires explicit authoring: ' + path)
    if len(rows) != CU.get_row_count(path):
        raise RuntimeError('Chooser row diagnostic count mismatch: ' + path)
    if [column['index'] for column in columns] != list(range(len(columns))):
        raise RuntimeError('Chooser column diagnostic indexes are not contiguous: ' + path)
    return dict(path=path, context_count=int(contexts.group(1)), columns=columns, rows=rows, text=text)


def column_for_enum(snapshot, enum):
    matches = [column for column in snapshot['columns'] if column['enum'] == enum]
    if len(matches) != 1 or matches[0]['type'] != 'EnumColumn':
        raise RuntimeError('Expected one typed EnumColumn for ' + enum)
    return matches[0]['index']


def enum_cell(row, column):
    value = row['cells'].get(column, '')
    if value.startswith('Any'):
        return 'Any', None
    match = re.fullmatch(r'(=|!=)\s+(.+)', value)
    if not match:
        raise RuntimeError('Unsupported enum cell; preserve and review source row: ' + row['line'])
    # DumpChooserFullTree prefers display names ("Idle Loop") over native
    # enumerator names ("IdleLoop"); normalize only for classification.
    return match.group(1), ''.join(match.group(2).split()).split('::')[-1]


def chooser_plan(source):
    if len(source['rows']) != EXPECTED_SOURCE_ROWS:
        raise RuntimeError('Source chooser changed: expected 103 rows, got ' + str(len(source['rows'])))
    # Current native evaluator passes exactly (AnimInstance, FAZ_ChooserOutputs).
    # Do not silently discard a third source context or guess output bindings.
    if source['context_count'] != 2:
        raise RuntimeError('Pending explicit context adapter: source chooser does not have the native two-context contract')
    columns = {key: column_for_enum(source, enum) for key, enum in (
        ('state', 'EAZ_StateMachineState'), ('gait', 'EAZ_Gait'),
        ('stance', 'EAZ_Stance'), ('reaction', 'EAZ_ObstacleReaction'))}
    outputs = [c['index'] for c in source['columns'] if c['type'] == 'OutputStructColumn']
    randoms = [c['index'] for c in source['columns'] if c['type'] == 'RandomizeColumn']
    if len(outputs) != 1 or len(randoms) != 1:
        raise RuntimeError('Expected one output-struct column and one Randomize column')
    columns['outputs'], columns['randomize'] = outputs[0], randoms[0]
    disabled, preserved_reactions, sprint = [], [], []
    for row in source['rows']:
        comparison, state = enum_cell(row, columns['state'])
        reaction_comparison, reaction = enum_cell(row, columns['reaction'])
        specific_reaction = ((reaction_comparison == '=' and reaction != 'None')
                             or (reaction_comparison == '!=' and reaction == 'None'))
        if specific_reaction:
            preserved_reactions.append(row['index'])
            continue
        if reaction_comparison == '!=':
            raise RuntimeError('Mixed normal/reaction row needs explicit splitting: ' + row['line'])
        if comparison != '=':
            raise RuntimeError('Source SM row is not an exact phase; cannot safely classify: ' + row['line'])
        gait_comparison, gait = enum_cell(row, columns['gait'])
        if state == 'LocomotionLoop' and gait_comparison == '=' and gait == 'Sprint':
            sprint.append(row['index'])
        elif state in ('IdleLoop', 'IdleBreak', 'LocomotionLoop'):
            disabled.append(row['index'])
    if len(sprint) != 1:
        raise RuntimeError('Expected exactly one shared normal Sprint LocomotionLoop row: ' + str(sprint))
    return dict(columns=columns, disable_rows=disabled, preserve_reaction_rows=preserved_reactions,
                shared_sprint_row=sprint[0], expected_total_rows=155)


def package_file(path):
    if not path.startswith('/Game/') or '..' in path:
        raise RuntimeError('Only explicit /Game packages may be backed up: ' + path)
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    result = (project / 'Content' / (path[len('/Game/'):] + '.uasset')).resolve()
    result.relative_to((project / 'Content').resolve())
    return result


def backup_existing(paths, backup_dir=None):
    """Disk copies only, before mutation. Never save another person's dirty state."""
    dirty = {str(p.get_path_name()) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    targets = sorted(set(paths))
    collision = sorted(dirty.intersection(targets))
    if collision:
        raise RuntimeError('Unsaved target packages must be resolved before backup: ' + json.dumps(collision))
    root = Path(backup_dir) if backup_dir else (Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'RifleP01Backups'
                                              / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ'))
    root = root.resolve()
    if root.exists() and any(root.iterdir()):
        raise RuntimeError('Backup directory must be new or empty: ' + str(root))
    copies = []
    # Validate every on-disk original before creating the backup directory.
    for path in targets:
        original = package_file(path)
        if not original.is_file():
            raise RuntimeError('Cannot back up missing on-disk package: ' + str(original))
        for suffix in ('.uasset', '.uexp', '.ubulk', '.uptnl'):
            sidecar = original.with_suffix(suffix)
            if sidecar.is_file():
                destination = root / 'Content' / (path[len('/Game/'):] + suffix)
                copies.append((sidecar, destination))
    manifest = []
    for original, destination in copies:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, destination)
        original_hash = hashlib.sha256(original.read_bytes()).hexdigest()
        if hashlib.sha256(destination.read_bytes()).hexdigest() != original_hash:
            raise RuntimeError('Backup checksum mismatch: ' + str(destination))
        manifest.append(dict(source=str(original), backup=str(destination), sha256=original_hash))
    (root / 'backup-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    return dict(directory=str(root), files=manifest)


def audit(build_chooser=True):
    """Read-only, including all ownership/membership/API checks needed before writes."""
    rows = library_rows()
    manifests = database_manifests(rows)
    schema = load(SCHEMA_PATH, unreal.PoseSearchSchema)
    report = dict(mode='audit', status='audit_complete', sequence_count=52, loop_count=48,
                  idles=4, sequences=[], databases={}, pending=[],
                  profile_database_overrides='all five null; chosen raw clip owns MM vocabulary',
                  contact_curves='not authored', index_status='not built or tested by this script')
    for method in ('add_branch_in_notify', 'set_disable_reselection_on_database'):
        if not hasattr(PU, method):
            report['pending'].append('Missing loaded native function AZ_PoseSearchUtils.' + method)
    dbs = {}
    for path, manifest in manifests.items():
        db = existing_owned(path, manifest, unreal.PoseSearchDatabase)
        current = members(db) if db else []
        if len(current) != len(set(current)) or not set(current).issubset(manifest['sequences']):
            raise RuntimeError('Unexpected or duplicated database entries: ' + path)
        if db and db.get_editor_property('schema') != schema:
            raise RuntimeError('Owned database has a different schema: ' + path)
        dbs[path] = db
        report['databases'][path] = dict(manifest=manifest, existing_members=current)
    for row in rows:
        seq = load(row['asset'], unreal.AnimSequence)
        notifies = branch_notifies(seq)
        length = float(AL.get_sequence_length(seq))
        if notifies:
            if (not row['database'] or len(notifies) != 1 or notifies[0]['database'] != row['database']
                    or abs(notifies[0]['start']) > 0.001 or abs(notifies[0]['duration'] - length) > 0.001):
                raise RuntimeError('Unexpected BranchIn; nothing will strip existing notifies: ' + row['asset'])
        elif row['database'] and row['asset'] in report['databases'][row['database']]['existing_members']:
            raise RuntimeError('Explicit entry would duplicate new BranchIn ownership: ' + row['asset'])
        if seq.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_NONE:
            raise RuntimeError('Library loop/idle unexpectedly additive: ' + row['asset'])
        report['sequences'].append(dict(row, loop=bool(seq.get_editor_property('loop')),
                                        length=length, branch_in=notifies))
    if build_chooser:
        row_helpers_loaded = True
        for method in ('get_chooser_disabled_rows', 'set_chooser_rows_disabled'):
            if not hasattr(CU, method):
                report['pending'].append('Missing loaded native function AZ_ChooserUtils.' + method)
                row_helpers_loaded = False
        if unreal.load_object(None, '/Script/AZ.EAZ_EightWayDirection') is None:
            report['pending'].append('Native EAZ_EightWayDirection is not loaded; complete the closed editor build')
        source_asset = load(SOURCE_CHOOSER, unreal.ChooserTable)
        source = chooser_snapshot(SOURCE_CHOOSER)
        plan = chooser_plan(source)
        # Live readback verified columns_structs is reflected; DisabledRows is
        # absent from Python and uses the project C++ bridge exclusively.
        try:
            source_columns = list(source_asset.get_editor_property('columns_structs'))
            if len(source_columns) != len(source['columns']):
                raise RuntimeError('Reflected column count mismatch')
        except Exception as error:
            report['pending'].append('Chooser columns_structs access unavailable; native reorder helper required: ' + str(error))
        source_disabled = chooser_disabled_rows(SOURCE_CHOOSER) if row_helpers_loaded else None
        chooser_manifest = dict(source=SOURCE_CHOOSER, source_dump_hash=digest(source['text']),
                                rows=rows, source_plan=plan, source_disabled_rows=source_disabled)
        # A missing bridge means the manifest is not yet complete. Report pending
        # instead of comparing an invented disabled-state snapshot to owned data.
        existing = existing_owned(TARGET_CHOOSER, chooser_manifest, unreal.ChooserTable) if row_helpers_loaded else None
        if existing and EAL.get_metadata_tag(existing, STATE_KEY) != 'complete':
            report['pending'].append('Owned chooser is partially prepared; inspect backup and resolve before rerun')
        if existing and CU.get_row_count(TARGET_CHOOSER) != plan['expected_total_rows']:
            raise RuntimeError('Completed owned chooser has an unexpected row count')
        report['chooser'] = dict(path=TARGET_CHOOSER, manifest=chooser_manifest, plan=plan,
                                 existing=bool(existing), source_dump=source['text'])
    if report['pending']:
        report['status'] = 'pending_native_or_asset_review'
    return report


def prepare_loops_and_databases(report):
    """Caller has completed audit and disk backups. No explicit DB entry insertion."""
    schema = load(SCHEMA_PATH, unreal.PoseSearchSchema)
    for path, record in report['databases'].items():
        db = existing_owned(path, record['manifest'], unreal.PoseSearchDatabase)
        if db is None:
            folder, name = path.rsplit('/', 1)
            db = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.PoseSearchDatabase, None)
            require(db, 'Could not create database: ' + path)
            db.set_editor_property('schema', schema)
            claim(db, record['manifest'], 'preparing')
            save(db)
    for row in report['sequences']:
        seq = load(row['asset'], unreal.AnimSequence)
        changed = False
        if not seq.get_editor_property('loop'):
            seq.set_editor_property('loop', True)
            changed = True
        if row['database'] and not branch_notifies(seq):
            require(PU.add_branch_in_notify(seq, load(row['database'], unreal.PoseSearchDatabase), 0.0, 0.0),
                    'Could not author BranchIn: ' + row['asset'])
            changed = True
        if changed:
            save(seq)
        require(seq.get_editor_property('loop'), 'Loop flag did not persist: ' + row['asset'])
    for path, record in report['databases'].items():
        db = load(path, unreal.PoseSearchDatabase)
        # PoseSearchDatabase::PreSaveRoot synchronizes BranchIn-owned entries.
        # Never call AddSequenceToDatabase for these 48 raw-loop memberships.
        save(db)
        current = members(db)
        if sorted(current) != sorted(record['manifest']['sequences']) or len(set(current)) != 8:
            raise RuntimeError('BranchIn synchronization did not produce exactly eight unique members: ' + path)
        updated = PU.set_disable_reselection_on_database(db, True)
        if updated != 8:
            raise RuntimeError('Continuity helper did not visit every database entry: ' + path)
        claim(db, record['manifest'], 'complete')
        save(db)
        record.update(members=current, disable_reselection_count=updated)


def prepare_chooser(report):
    """Duplicate + preserve 103 rows; append 52 and reorder whole column structs."""
    record = report['chooser']
    if record['existing']:
        record['action'] = 'reused_completed_owned_chooser'
        return
    chooser = EAL.duplicate_asset(SOURCE_CHOOSER, TARGET_CHOOSER)
    require(chooser, 'Chooser duplication failed')
    claim(chooser, record['manifest'], 'preparing')
    save(chooser)
    plan = record['plan']
    source_columns = plan['columns']
    # Add complete Any-initialized filter columns, then move them before Randomize.
    direction_col = CU.add_enum_column_to_sub(TARGET_CHOOSER, '', 'MovementDirection8', 'EAZ_EightWayDirection')
    if direction_col < 0:
        raise RuntimeError('Native EAZ_EightWayDirection is not loaded')
    require(CU.set_column_binding_chain(TARGET_CHOOSER, direction_col, ['ChooserContext', 'MovementDirection8'], 0),
            'Cannot bind eight-way context filter')
    aim_col = CU.add_bool_column_to_sub(TARGET_CHOOSER, '', 'ChooserContext.bIsAiming')
    if aim_col < 0:
        raise RuntimeError('Cannot add aim filter')
    require(CU.set_column_binding_chain(TARGET_CHOOSER, aim_col, ['ChooserContext', 'bIsAiming'], 0),
            'Cannot bind aim context filter')
    idle_phase_col = CU.add_multi_enum_column_to_sub(TARGET_CHOOSER, '', 'SMState', 'EAZ_StateMachineState')
    if idle_phase_col < 0:
        raise RuntimeError('Cannot add shared IdleLoop/IdleBreak phase filter')
    require(CU.set_column_binding_chain(TARGET_CHOOSER, idle_phase_col, ['ChooserContext', 'SMState'], 0),
            'Cannot bind idle phase context filter')
    # Source structures retain all103 row values, properties and output data.
    columns = list(chooser.get_editor_property('columns_structs'))
    added_columns = [direction_col, aim_col, idle_phase_col]
    old_indexes = [i for i in range(len(columns)) if i not in added_columns]
    insert_at = old_indexes.index(source_columns['randomize'])
    order = old_indexes[:insert_at] + added_columns + old_indexes[insert_at:]
    chooser.set_editor_property('columns_structs', [columns[i] for i in order])
    remap = {old: new for new, old in enumerate(order)}
    mapped = {key: remap[value] for key, value in source_columns.items()}
    direction_col, aim_col, idle_phase_col = remap[direction_col], remap[aim_col], remap[idle_phase_col]
    if max(direction_col, aim_col, idle_phase_col) >= mapped['randomize']:
        raise RuntimeError('New selection filters must precede Randomize')
    disabled = chooser_disabled_rows(TARGET_CHOOSER)
    if disabled[plan['shared_sprint_row']]:
        raise RuntimeError('Shared source sprint was already disabled; explicit source review required')
    require(CU.set_chooser_rows_disabled(TARGET_CHOOSER, plan['disable_rows'], True),
            'Cannot disable the replaced normal source rows')
    for offset, spec in enumerate(library_rows()):
        index = CU.add_empty_row_to_sub(TARGET_CHOOSER, '', load(spec['asset'], unreal.AnimSequence))
        if index != EXPECTED_SOURCE_ROWS + offset:
            raise RuntimeError('Appended row did not land at expected index')
        state = None if spec['state'] == 'IdleLoop' else spec['state']
        for key, value in (('state', state), ('stance', spec['stance']), ('gait', spec['gait']), ('reaction', 'None')):
            # AddEmptyRow initializes unused enum cells to MatchAny. Explicitly set
            # Any for gait-agnostic idle/crouch rows rather than assuming value zero.
            enum_value = value or ('IdleLoop' if key == 'state' else 'Walk')
            require(CU.set_cell_enum_on_sub(TARGET_CHOOSER, '', index, mapped[key], enum_value, 0 if value else 2),
                    'Cannot author ' + key + ' cell at row ' + str(index))
        require(CU.set_cell_enum_on_sub(TARGET_CHOOSER, '', index, direction_col, spec['direction'] or 'F',
                                        0 if spec['direction'] else 2), 'Cannot author direction cell')
        require(CU.set_cell_bool_on_sub(TARGET_CHOOSER, '', index, aim_col, 1 if spec['aiming'] else 0),
                'Cannot author aim cell')
        if spec['state'] == 'IdleLoop':
            # An actual idle remains the resting pose through cosmetic IdleBreak.
            # Its successful push arms the SM's existing idle-break end timer.
            require(CU.set_cell_multi_enum_on_sub(TARGET_CHOOSER, '', index, idle_phase_col, ['IdleLoop', 'IdleBreak']),
                    'Cannot cover IdleBreak with the corresponding existing P01 idle')
        for name, value in (('bUseMM', 'True' if spec['use_mm'] else 'False'), ('StartTime', '0'),
                            ('BlendTime', '0.2'), ('BlendOut', '0'), ('MMCostLimit', '0')):
            require(CU.set_cell_output_struct_field_on_sub(TARGET_CHOOSER, '', index, mapped['outputs'], name, value),
                    'Cannot author output field ' + name)
        require(CU.set_cell_randomize_on_sub(TARGET_CHOOSER, '', index, mapped['randomize'], 1.0),
                'Cannot set deterministic single-matching-row weight')
    require(CU.get_row_count(TARGET_CHOOSER) == 155, 'Owned chooser must preserve103 + append52 rows')
    require(CU.compile_and_save(TARGET_CHOOSER), 'Chooser compile/save failed')
    actual = chooser_snapshot(TARGET_CHOOSER)
    actual_disabled = chooser_disabled_rows(TARGET_CHOOSER)
    source = chooser_snapshot(SOURCE_CHOOSER)
    for original, preserved in zip(source['rows'], actual['rows'][:103]):
        for old_index, old_value in original['cells'].items():
            if preserved['cells'].get(remap[old_index]) != old_value:
                raise RuntimeError('Source row cell changed during authoring: row ' + str(original['index']))
        # Asset result strings are not rewritten or resolved from short names.
        original_out = re.search(r'"out":\s*"([^"]*)"', original['line'])
        preserved_out = re.search(r'"out":\s*"([^"]*)"', preserved['line'])
        if not original_out or not preserved_out or original_out.group(1) != preserved_out.group(1):
            raise RuntimeError('Source row result changed during authoring: row ' + str(original['index']))
    if any(not actual_disabled[i] for i in plan['disable_rows']):
        raise RuntimeError('Old normal vocabulary is still enabled')
    if any(actual_disabled[103:]):
        raise RuntimeError('A new P01 row was unexpectedly disabled')
    if actual_disabled[plan['shared_sprint_row']]:
        raise RuntimeError('Shared sprint was disabled')
    original_disabled = record['manifest']['source_disabled_rows']
    for index in plan['preserve_reaction_rows']:
        expected_disabled = bool(original_disabled[index]) if index < len(original_disabled) else False
        if actual_disabled[index] != expected_disabled:
            raise RuntimeError('Reaction row enable state changed: ' + str(index))
    claim(chooser, record['manifest'], 'complete')
    require(CU.compile_and_save(TARGET_CHOOSER), 'Chooser completion save failed')
    record.update(action='prepared', final_row_count=len(actual['rows']), final_dump=actual['text'])


def main(prepare=False, build_chooser=True, backup_dir=None, receipt_path=None):
    """Audit by default. Explicit prepare=True backs up before authoring any asset.

    Keep the profile's five database override references null; all six databases
    are reached through the selected sequence's BranchIn. Assign profile chooser,
    standing/shared masked AO and exact48 PlayRateLoopAssets separately.
    """
    report = None
    try:
        report = audit(build_chooser=build_chooser)
        if not prepare:
            return report
        if report['pending']:
            raise RuntimeError('Preparation pending: ' + json.dumps(report['pending']))
        existing_paths = [row['asset'] for row in report['sequences']]
        existing_paths.extend(path for path in report['databases'] if EAL.does_asset_exist(path))
        if build_chooser:
            existing_paths.append(SOURCE_CHOOSER)
            if EAL.does_asset_exist(TARGET_CHOOSER):
                existing_paths.append(TARGET_CHOOSER)
        report['backup'] = backup_existing(existing_paths, backup_dir)
        report['mode'] = 'prepare'
        report['status'] = 'preparing'
        prepare_loops_and_databases(report)
        if build_chooser:
            prepare_chooser(report)
        report['status'] = 'prepared_candidates'
        return report
    except Exception as error:
        if report is not None:
            report['status'] = 'failed'
            report['error'] = str(error)
        raise
    finally:
        # An explicit receipt_path is the only optional audit-time disk write.
        if receipt_path and report is not None:
            path = Path(receipt_path)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(json.dumps(report, indent=2), encoding='utf-8')
        if report is not None:
            print('[RifleP01] ' + json.dumps({key: report.get(key) for key in (
                'mode', 'status', 'sequence_count', 'loop_count', 'pending', 'error')}))
        gc.collect()


if __name__ == '__main__':
    main()
