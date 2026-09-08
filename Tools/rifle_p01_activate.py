# @Description: Audit or author the P01 profile, held-aim ability, input wiring and existing rifle manifest.
"""Editor Python authoring; main() is read-only, prepare=True is explicit.

The root owns graph wiring and native compilation of regular Blueprints after
this script. No Blueprint compilation, AnimBP edits, PIE, previews or tests run
here. The existing standing AO is shared with crouch and must be masked above
spine_02 by the root's graph; this does not create a new crouched AO family.

The pickup's exported manifest is patched structurally. Only AnimationProfile
and a single aim grant change; all other fragments, fields and contained
magazine manifests survive. This is authoring pickup defaults, not migration of
runtime inventory instances.
"""

import gc
import hashlib
import json
import re
import runpy
from pathlib import Path

import unreal


PROFILE = '/Game/AZ/Blueprints/Animation/MotionMatching/RifleP01/DA_WeaponAnim_P01'
CHOOSER = '/Game/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations'
AO = '/Game/AZ/Blueprints/Animation/AO_Rifle_Aim'
ABILITY = '/Game/AZ/Blueprints/AbilitySystem/Hero/Abilities/BP_AZ_GA_FirearmAim'
ACTION = '/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IA_RT_Aim'
INPUT_CONFIG = '/Game/AZ/Blueprints/Input/AZ_InputConfig'
MAPPING_CONTEXT = '/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IMC_RT_PawnInputs'
RIFLE = '/Game/AZ/Blueprints/Items/Equippables/Weapons/BPAZ_CommonUI_PickupItem'
WEAPON_FRAGMENT = '/Script/AZ.AZ_Inv_CommonUI_WeaponStateFragment'
GRANT_FRAGMENT = '/Script/AZ.AZ_Inv_CommonUI_AbilityGrantFragment'
OWNER = 'rifle_p01_activate:v1'
OWNER_KEY = 'AZ.RifleP01Activation.Owner'
MANIFEST_KEY = 'AZ.RifleP01Activation.Manifest'
EAL = unreal.EditorAssetLibrary


def package(asset):
    return str(asset.get_path_name()).split('.')[0]


def load(path, cls):
    asset = unreal.load_asset(path)
    if asset is None or not isinstance(asset, cls):
        raise RuntimeError('Missing asset or unexpected type: ' + path)
    return asset


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def save(asset):
    require(EAL.save_loaded_asset(asset, only_if_is_dirty=False), 'Asset save failed: ' + package(asset))


def setup_library():
    # This sibling has a guarded __main__; importing it does not run audit/prepare.
    # Never import rifle_inventory_content_setup.py: that older file auto-executes.
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    return runpy.run_path(str(project / 'Tools/rifle_p01_setup.py'), run_name='rifle_p01_setup_library')


def tag(name):
    result = unreal.GameplayTag()
    require(result.import_text(name), 'Gameplay tag import failed: ' + name)
    require(str(result.get_editor_property('tag_name')) == name, 'Gameplay tag is not registered: ' + name)
    return result


def tags(*names):
    result = unreal.GameplayTagContainer()
    content = '(GameplayTags=(' + ','.join('(TagName="' + name + '")' for name in names) + '))'
    require(result.import_text(content), 'Gameplay tag container import failed')
    return result


def split_top_level(text, separator=',', keep_empty=False):
    """Split Unreal ExportText without splitting quoted strings or nested structs."""
    parts, start, stack, quote, escaped = [], 0, [], None, False
    closing = {')': '(', ']': '[', '}': '{'}
    for i, char in enumerate(text):
        if quote:
            if escaped:
                escaped = False
            elif char == '\\':
                escaped = True
            elif char == quote:
                quote = None
            continue
        if char in ('"', "'"):
            quote = char
        elif char in '([{':
            stack.append(char)
        elif char in ')]}':
            if not stack or stack.pop() != closing[char]:
                raise RuntimeError('Unbalanced Unreal exported text')
        elif char == separator and not stack:
            parts.append(text[start:i].strip())
            start = i + 1
    if quote or stack:
        raise RuntimeError('Unterminated Unreal exported text')
    parts.append(text[start:].strip())
    return parts if keep_empty else [part for part in parts if part]


def parenthesized(text):
    text = text.strip()
    # Unreal exports some empty array/struct fields as "Field=" rather than
    # "Field=()". Both encode an empty payload; retain the original field text.
    if not text:
        return ''
    if not text.startswith('(') or not text.endswith(')'):
        raise RuntimeError('Unsupported exported struct/array representation: ' + text[:100])
    return text[1:-1]


def fields(text):
    result = []
    for part in split_top_level(parenthesized(text)):
        pair = split_top_level(part, '=', keep_empty=True)
        if len(pair) != 2 or not re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', pair[0]):
            raise RuntimeError('Unsupported exported field: ' + part[:100])
        result.append((pair[0], pair[1]))
    if len({key for key, _ in result}) != len(result):
        raise RuntimeError('Duplicated field names in exported text')
    return result


def encode_fields(values):
    return '(' + ','.join(key + '=' + value for key, value in values) + ')'


def replace_field(values, key, value):
    result = [(name, value if name == key else original) for name, original in values]
    if not any(name == key for name, _ in values):
        result.append((key, value))
    return result


def fragment_parts(text):
    if text.strip() == 'None':
        return 'None', []
    index = text.find('(')
    if index < 0:
        raise RuntimeError('Unsupported instanced-fragment export: ' + text[:100])
    return text[:index].strip(), fields(text[index:])


def object_reference(class_path, path, class_object=False):
    name = path.rsplit('/', 1)[1] + ('_C' if class_object else '')
    return '"' + class_path + "'" + path + '.' + name + "'" + '"'


def patched_manifest(original):
    """Pure text transformation: preserve every unrelated field and fragment."""
    values = fields(original)
    container = dict(values)
    if 'Fragments' not in container:
        raise RuntimeError('Existing manifest has no Fragments field')
    fragments = split_top_level(parenthesized(container['Fragments']))
    counts = {WEAPON_FRAGMENT: 0, GRANT_FRAGMENT: 0}
    result = []
    expected_ability = object_reference('/Script/Engine.BlueprintGeneratedClass', ABILITY, True)
    expected_profile = object_reference('/Script/AZ.AZ_WeaponAnimationProfile', PROFILE)
    for fragment in fragments:
        kind, fragment_fields = fragment_parts(fragment)
        if kind == WEAPON_FRAGMENT:
            counts[kind] += 1
            current = dict(fragment_fields)
            if current.get('bUsesDetachableMagazines') != 'True' or 'Weapon.Rifle' not in current.get('WeaponTag', ''):
                raise RuntimeError('Pickup is not the established detachable-magazine rifle')
            fragment = kind + encode_fields(replace_field(fragment_fields, 'AnimationProfile', expected_profile))
        elif kind == GRANT_FRAGMENT:
            counts[kind] += 1
            current = dict(fragment_fields).get('AbilitiesToGrant', '()')
            granted = split_top_level(parenthesized(current))
            if granted and granted != [expected_ability]:
                raise RuntimeError('Existing foreign/legacy ability grants require explicit review; refusing to enable or remove them')
            fragment = kind + encode_fields(replace_field(fragment_fields, 'AbilitiesToGrant', '(' + expected_ability + ')'))
        result.append(fragment)
    if counts[WEAPON_FRAGMENT] != 1 or counts[GRANT_FRAGMENT] > 1:
        raise RuntimeError('Expected one weapon fragment and at most one ability-grant fragment')
    if counts[GRANT_FRAGMENT] == 0:
        result.append(GRANT_FRAGMENT + '(AbilitiesToGrant=(' + expected_ability + '))')
    return encode_fields(replace_field(values, 'Fragments', '(' + ','.join(result) + ')'))


def assert_manifest_preserved(before, after):
    before_fields, after_fields = dict(fields(before)), dict(fields(after))
    old_fragments = split_top_level(parenthesized(before_fields.pop('Fragments')))
    new_fragments = split_top_level(parenthesized(after_fields.pop('Fragments')))
    require(before_fields == after_fields, 'An unrelated top-level manifest field changed')
    def retained(fragment):
        kind, values = fragment_parts(fragment)
        if kind == GRANT_FRAGMENT:
            return None
        if kind == WEAPON_FRAGMENT:
            values = [(key, value) for key, value in values if key != 'AnimationProfile']
            return kind + encode_fields(values)
        return fragment
    require([retained(f) for f in old_fragments if retained(f) is not None]
            == [retained(f) for f in new_fragments if retained(f) is not None],
            'An unrelated fragment or weapon field changed during manifest round-trip')
    grants = [fragment_parts(f)[1] for f in new_fragments if fragment_parts(f)[0] == GRANT_FRAGMENT]
    require(len(grants) == 1, 'Manifest must contain exactly one ability-grant fragment')
    old_grants = [fragment_parts(f)[1] for f in old_fragments if fragment_parts(f)[0] == GRANT_FRAGMENT]
    if old_grants:
        require([(key, value) for key, value in old_grants[0] if key != 'AbilitiesToGrant']
                == [(key, value) for key, value in grants[0] if key != 'AbilitiesToGrant'],
                'An unrelated ability-grant fragment field changed')
    expected = object_reference('/Script/Engine.BlueprintGeneratedClass', ABILITY, True)
    require(split_top_level(parenthesized(dict(grants[0]).get('AbilitiesToGrant', '()'))) == [expected],
            'Manifest ability reference did not resolve to the single new held-aim ability')
    weapon = [dict(fragment_parts(f)[1]) for f in new_fragments if fragment_parts(f)[0] == WEAPON_FRAGMENT]
    require(len(weapon) == 1 and PROFILE in weapon[0].get('AnimationProfile', ''), 'AnimationProfile reference did not resolve')


def item_component_template(bp):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    matches = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
        if isinstance(obj, unreal.AZ_Inv_CommonUI_ItemComponent) and obj not in matches:
            matches.append(obj)
    require(len(matches) == 1, 'Expected exactly one CommonUI item component template')
    return matches[0]


def owned_or_missing(path, cls):
    if not EAL.does_asset_exist(path):
        return None
    asset = load(path, cls)
    require(EAL.get_metadata_tag(asset, OWNER_KEY) == OWNER, 'Unrecognized target asset: ' + path)
    return asset


def claim(asset, manifest):
    EAL.set_metadata_tag(asset, OWNER_KEY, OWNER)
    EAL.set_metadata_tag(asset, MANIFEST_KEY, hashlib.sha256(json.dumps(manifest, sort_keys=True).encode('utf-8')).hexdigest())


def mapping_key(mapping):
    return str(mapping.get_editor_property('key').export_text())


def audit(input_config_path=INPUT_CONFIG, mapping_context_path=MAPPING_CONTEXT):
    setup = setup_library()
    moving = [row['asset'] for row in setup['library_rows']() if row['use_mm']]
    require(len(moving) == 48 and len(set(moving)) == 48, 'Expected exact48 P01 moving-loop allowlist')
    pending = []
    for name in ('AZ_WeaponAnimationProfile', 'AZ_GA_FirearmAim'):
        if not hasattr(unreal, name):
            pending.append('Native reflection is missing unreal.' + name)
    chooser = load(CHOOSER, unreal.ChooserTable)
    aim_offset = load(AO, unreal.AimOffsetBlendSpace)
    for path in moving:
        load(path, unreal.AnimSequence)
    profile = owned_or_missing(PROFILE, unreal.AZ_WeaponAnimationProfile) if hasattr(unreal, 'AZ_WeaponAnimationProfile') else None
    ability = owned_or_missing(ABILITY, unreal.Blueprint)
    if ability and hasattr(unreal, 'AZ_GA_FirearmAim'):
        require(isinstance(unreal.get_default_object(ability.generated_class()), unreal.AZ_GA_FirearmAim), 'Aim Blueprint has a different native parent')
    action = load(ACTION, unreal.InputAction) if EAL.does_asset_exist(ACTION) else None
    config = load(input_config_path, unreal.AZ_InputConfig)
    config_rows = list(config.get_editor_property('ability_input_actions'))
    aim_indexes = [i for i, row in enumerate(config_rows) if str(row.get_editor_property('input_tag').get_editor_property('tag_name')) == 'Input.Action.Aim']
    require(len(aim_indexes) <= 1, 'Duplicate Input.Action.Aim rows need explicit cleanup')
    for i, row in enumerate(config_rows):
        if action and row.get_editor_property('input_action') == action and i not in aim_indexes:
            raise RuntimeError('The dedicated RT Aim action is assigned to another input tag')
    context = load(mapping_context_path, unreal.InputMappingContext)
    mapping_data = context.get_editor_property('default_key_mappings')
    mappings = list(mapping_data.get_editor_property('mappings'))
    aim_rmb = [i for i, mapping in enumerate(mappings) if action and mapping.get_editor_property('action') == action and mapping_key(mapping) == 'RightMouseButton']
    require(len(aim_rmb) <= 1, 'Duplicate Aim/RMB mappings need explicit cleanup')
    for index in aim_rmb:
        require(not mappings[index].get_editor_property('triggers') and not mappings[index].get_editor_property('modifiers'),
                'Existing Aim/RMB row has custom triggers/modifiers; preserve and review it explicitly')
    if action:
        require(not action.get_editor_property('modifiers'), 'Existing Aim action has custom modifiers; explicit review required')
    rifle = load(RIFLE, unreal.Blueprint)
    component = item_component_template(rifle)
    current_manifest = component.get_editor_property('pickup_item_manifest')
    require(hasattr(current_manifest, 'export_text') and hasattr(current_manifest, 'import_text'), 'Manifest ImportText/ExportText unavailable')
    before = current_manifest.export_text()
    proposed = patched_manifest(before)
    # The text transformation itself must preserve every other field before any write.
    assert_manifest_preserved(before, proposed)
    contained = [value.export_text() for value in component.get_editor_property('initial_contained_item_manifests')]
    return dict(mode='audit', status='pending' if pending else 'audit_complete', pending=pending,
                profile=PROFILE, ability=ABILITY, action=ACTION, aim_offset=AO, chooser=CHOOSER,
                input_config=input_config_path, mapping_context=mapping_context_path,
                moving_loop_assets=moving, existing_profile=bool(profile), existing_ability=bool(ability),
                existing_action=bool(action), input_aim_indexes=aim_indexes, mapping_aim_rmb_indexes=aim_rmb,
                manifest_before=before, manifest_proposed=proposed, contained_magazines_before=contained,
                native_compile_after=[ABILITY, RIFLE],
                graph_requirement='Shared standing/crouched AO masked above spine_02; root-owned graph update')


def assign_profile(report):
    profile = owned_or_missing(PROFILE, unreal.AZ_WeaponAnimationProfile)
    if profile is None:
        folder, name = PROFILE.rsplit('/', 1)
        profile = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AZ_WeaponAnimationProfile, None)
        require(profile, 'Could not create native weapon profile DataAsset')
        claim(profile, {'loops': report['moving_loop_assets'], 'aim_offset': AO})
    values = {
        'walk_loco_database': None, 'run_loco_database': None,
        'strafe_walk_database': None, 'strafe_run_database': None, 'strafe_crouch_database': None,
        'relaxed_upper_body_pose': None, 'standing_aim_offset': load(AO, unreal.AimOffsetBlendSpace),
        'crouching_aim_offset': load(AO, unreal.AimOffsetBlendSpace),
        'use_loop_play_rate': True, 'speed_curve_name': 'Speed',
        'play_rate_loop_assets': [load(path, unreal.AnimSequence) for path in report['moving_loop_assets']],
        'phase_locked_transition_to_loop': False, 'use_unarmed_leans': False,
    }
    for name, value in values.items():
        try:
            profile.set_editor_property(name, value)
        except Exception as error:
            raise RuntimeError('Cannot author profile property ' + name + '; explicit native adapter may be required: ' + str(error)) from error
    require([package(asset) for asset in profile.get_editor_property('play_rate_loop_assets')] == report['moving_loop_assets'], 'Profile moving-loop allowlist changed')
    for name in ('walk_loco_database', 'run_loco_database', 'strafe_walk_database', 'strafe_run_database', 'strafe_crouch_database', 'relaxed_upper_body_pose'):
        require(profile.get_editor_property(name) is None, 'Profile must not override raw-clip MM or P01 relaxed poses: ' + name)
    save(profile)
    return profile


def assign_ability():
    bp = owned_or_missing(ABILITY, unreal.Blueprint)
    if bp is None:
        folder, name = ABILITY.rsplit('/', 1)
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.AZ_GA_FirearmAim)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Blueprint, factory)
        require(bp, 'Could not create held-aim Blueprint')
        claim(bp, {'parent': '/Script/AZ.AZ_GA_FirearmAim', 'input': 'Input.Action.Aim', 'required': 'Weapon.Rifle'})
    cdo = unreal.get_default_object(bp.generated_class())
    require(isinstance(cdo, unreal.AZ_GA_FirearmAim), 'New Blueprint generated class is unavailable or wrong type')
    cdo.set_editor_property('input_tag', tag('Input.Action.Aim'))
    cdo.set_editor_property('activation_required_tags', tags('Weapon.Rifle'))
    # Native activation owns these contributions explicitly; BP ownership would double them.
    cdo.set_editor_property('activation_owned_tags', tags())
    cdo.set_editor_property('source_object_must_equal_current_weapon_to_activate', False)
    cdo.set_editor_property('activate_ability_on_granted', False)
    require(str(cdo.get_editor_property('input_tag').get_editor_property('tag_name')) == 'Input.Action.Aim', 'Ability input tag readback failed')
    require('Weapon.Rifle' in cdo.get_editor_property('activation_required_tags').export_text(), 'Required rifle tag readback failed')
    require('TagName=' not in cdo.get_editor_property('activation_owned_tags').export_text(), 'Aim owned tags must remain empty on the BP')
    save(bp)
    return bp


def assign_input(report):
    action = load(ACTION, unreal.InputAction) if EAL.does_asset_exist(ACTION) else None
    if action is None:
        folder, name = ACTION.rsplit('/', 1)
        action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.InputAction, None)
        require(action, 'Could not create the RT Aim InputAction')
        claim(action, {'input': 'Input.Action.Aim', 'key': 'RightMouseButton'})
    action.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
    action.set_editor_property('triggers', [])
    action.set_editor_property('consume_input', False)
    save(action)
    config = load(report['input_config'], unreal.AZ_InputConfig)
    rows = list(config.get_editor_property('ability_input_actions'))
    prior_rows = [row.export_text() for row in rows]
    index = report['input_aim_indexes'][0] if report['input_aim_indexes'] else len(rows)
    if index == len(rows):
        rows.append(unreal.AZ_InputAction())
    rows[index].set_editor_property('input_action', action)
    rows[index].set_editor_property('input_tag', tag('Input.Action.Aim'))
    config.set_editor_property('ability_input_actions', rows)
    for i, old in enumerate(prior_rows):
        if i != index:
            require(rows[i].export_text() == old, 'Unrelated InputConfig row changed')
    save(config)
    context = load(report['mapping_context'], unreal.InputMappingContext)
    data = context.get_editor_property('default_key_mappings')
    mappings = list(data.get_editor_property('mappings'))
    before = [mapping.export_text() for mapping in mappings]
    matches = [mapping for mapping in mappings if mapping.get_editor_property('action') == action and mapping_key(mapping) == 'RightMouseButton']
    if not matches:
        mapping = unreal.EnhancedActionKeyMapping()
        key = unreal.Key()
        require(key.import_text('RightMouseButton'), 'RMB key import failed')
        mapping.set_editor_property('action', action)
        mapping.set_editor_property('key', key)
        mappings.append(mapping)
        data.set_editor_property('mappings', mappings)
        context.set_editor_property('default_key_mappings', data)
    actual = list(context.get_editor_property('default_key_mappings').get_editor_property('mappings'))
    require([mapping.export_text() for mapping in actual[:len(before)]] == before, 'An existing key mapping changed')
    require(sum(mapping.get_editor_property('action') == action and mapping_key(mapping) == 'RightMouseButton' for mapping in actual) == 1, 'Aim/RMB mapping is missing or duplicated')
    save(context)


def assign_pickup(report):
    bp = load(RIFLE, unreal.Blueprint)
    component = item_component_template(bp)
    require(component.get_editor_property('pickup_item_manifest').export_text() == report['manifest_before'], 'Pickup manifest changed after preflight')
    candidate = unreal.AZ_Inv_CommonUI_ItemManifest()
    require(candidate.import_text(report['manifest_proposed']), 'Patched manifest ImportText failed')
    assert_manifest_preserved(report['manifest_before'], candidate.export_text())
    component.set_editor_property('pickup_item_manifest', candidate)
    after = component.get_editor_property('pickup_item_manifest').export_text()
    assert_manifest_preserved(report['manifest_before'], after)
    contained = [value.export_text() for value in component.get_editor_property('initial_contained_item_manifests')]
    require(contained == report['contained_magazines_before'], 'Contained magazine manifests changed')
    save(bp)
    report['manifest_after'] = after


def main(prepare=False, input_config_path=INPUT_CONFIG, mapping_context_path=MAPPING_CONTEXT,
         backup_dir=None, receipt_path=None):
    report = None
    try:
        report = audit(input_config_path, mapping_context_path)
        if not prepare:
            return report
        require(not report['pending'], 'Preparation pending: ' + json.dumps(report['pending']))
        targets = [RIFLE, input_config_path, mapping_context_path]
        targets += [path for path in (PROFILE, ABILITY, ACTION) if EAL.does_asset_exist(path)]
        report['backup'] = setup_library()['backup_existing'](targets, backup_dir)
        report.update(mode='prepare', status='preparing')
        assign_profile(report)
        assign_ability()
        assign_input(report)
        assign_pickup(report)
        report['status'] = 'authored_requires_root_graph_and_native_bp_compile'
        return report
    except Exception as error:
        if report is not None:
            report.update(status='failed', error=str(error))
        raise
    finally:
        if report is not None:
            if receipt_path:
                destination = Path(receipt_path)
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_text(json.dumps(report, indent=2), encoding='utf-8')
            print('[RifleP01Activate] ' + json.dumps({key: report.get(key) for key in ('mode', 'status', 'pending', 'error')}))
        gc.collect()


if __name__ == '__main__':
    main()
