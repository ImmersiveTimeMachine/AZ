# @Description: Audit, author and verify the pistol's canonical inventory definitions.
"""Guarded editor authoring: importing this module never changes Unreal assets.

main('audit') is read-only apart from a receipt in Saved/PistolInventory.
main('author') creates eight owned Blueprints and one owned notify-free animation. It never
compiles Blueprints or edits rifle assets, input mappings, quick slots or maps.
Compile the returned full object paths through the native Blueprint tool, then
call main('save') and main('verify'). Profile, icon and new native fields must
already exist before authoring. The user owns PIE/gameplay testing.

The 15-round capacity, 7-round partial pickup and combat values below are gameplay
defaults, not a claim about the real-world model represented by Pistols_B.
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
OUTPUT = ROOT / 'Saved/PistolInventory'
OWNER_KEY = 'AZ.PistolInventory.Owner'
OWNER = 'pistol_inventory_setup:v1'
WEAPON = '/Game/AZ/Blueprints/Weapon/AZ_BP_Pistol'
ITEMS = '/Game/AZ/Blueprints/Items/Equippables/Weapons/Pistol'
PICKUP = ITEMS + '/BP_Pickup_Pistol'
MAGAZINE = ITEMS + '/BP_Pickup_PistolMagazine'
MAGAZINES = [(MAGAZINE, 15), (MAGAZINE + '_Partial', 7), (MAGAZINE + '_Empty', 0)]
ABILITIES = [
    ('Aim', 'AZ_GA_FirearmAim', 'Input.Action.Aim'),
    ('Fire', 'AZ_GA_FirearmFire', 'Input.Action.PrimaryAttack'),
    ('Reload', 'AZ_GA_FirearmReload', 'Input.Action.Reload'),
]
ABILITY_ROOT = '/Game/AZ/Blueprints/AbilitySystem/Hero/Abilities/Pistol'
PROFILE = '/Game/AZ/Blueprints/Animation/MotionMatching/Pistol/DA_WeaponAnim_Pistol'
ICON = '/Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Pistol'
MAG_ICON = '/Game/InventorySystemPro/ExampleContent/Common/Art/Ammo9mm/T_Ammo9mm'
RETICLE = '/Game/AZ/Blueprints/Menu/HUD/Reticles/DA_HUDReticle_Rifle'
PICKUP_TEMPLATE = '/Game/AZ/Blueprints/Items/Equippables/Weapons/BPAZ_CommonUI_PickupItem'
MESH = '/Game/MilitaryWeapDark/Weapons/Pistols_B'
PICKUP_MESH = ITEMS + '/SM_Pistol_Pickup'
MAGAZINE_MESH = '/Game/AZ/Blueprints/Items/Equippables/Weapons/Pistol/SM_Pistol_Magazine'
SOUND = '/Game/MilitaryWeapDark/Sound/Pistol/PistolB_Fire_Cue'
FLASH = '/Game/MilitaryWeapDark/FX/P_Pistol_MuzzleFlash_01'
IMPACT = '/Game/MilitaryWeapDark/FX/P_Impact_Stone_Small_01'
MESH_FIRE_SOURCE = '/Game/MilitaryWeapDark/Weapons/Anims/Fire_Pistol_W'
MESH_FIRE = '/Game/AZ/Blueprints/Animation/MotionMatching/Pistol/AS_Pistol_WeaponFire'
FAMILY = 'PistolB.Standard'
CAPACITY = 15
SOCKETS = {
    'relaxed_socket_name': 'RightHandPistolSocketRelaxed',
    'aim_socket_name': 'RightHandPistolSocketAim',
    'carry_socket_name': 'PistolHolsterSocket',
}
EAL = unreal.EditorAssetLibrary


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def helpers():
    # Guarded sibling: only pure tag/manifest helpers are used. Never import the
    # old rifle_inventory_content_setup.py, whose module body runs main().
    return runpy.run_path(str(ROOT / 'Tools/rifle_p01_activate.py'), run_name='pistol_manifest_helpers')


def ability_path(name):
    return ABILITY_ROOT + '/BP_AZ_GA_Pistol' + name


def packages():
    return [WEAPON] + [ability_path(row[0]) for row in ABILITIES] + [PICKUP] + [row[0] for row in MAGAZINES]


def owned_packages():
    return [MESH_FIRE] + packages()


def object_path(path):
    return path + '.' + path.rsplit('/', 1)[1]


def source_file(path):
    require(path.startswith('/Game/'), 'Only project packages are supported')
    return ROOT / 'Content' / (path.removeprefix('/Game/') + '.uasset')


def load(path, cls):
    obj = unreal.load_asset(path)
    require(isinstance(obj, cls), 'Missing asset or unexpected type: ' + path)
    return obj


def owned_or_missing(path, cls=None):
    if not EAL.does_asset_exist(path):
        return None
    bp = load(path, cls or unreal.Blueprint)
    require(EAL.get_metadata_tag(bp, OWNER_KEY) == OWNER, 'Foreign target asset; inspect before changing: ' + path)
    return bp


def claim(bp):
    EAL.set_metadata_tag(bp, OWNER_KEY, OWNER)


def create_blueprint(path, parent=None, duplicate=None):
    bp = owned_or_missing(path)
    if bp:
        return bp
    if duplicate:
        bp = EAL.duplicate_asset(duplicate, path)
    else:
        folder, name = path.rsplit('/', 1)
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Blueprint, factory)
    require(bp is not None, 'Could not create Blueprint: ' + path)
    claim(bp)
    return bp


def native_component(cdo, name, cls):
    matches = [c for c in cdo.get_components_by_class(cls) if c.get_name() == name]
    require(len(matches) == 1, 'Missing or ambiguous native component: ' + name)
    return matches[0]


def set_property(obj, name, value):
    obj.modify()
    if isinstance(obj, unreal.SkeletalMeshComponent) and name == 'skeletal_mesh_asset':
        # UE5.8's editor property is a transient alias; NEVER notification does
        # not update the rendered SkinnedAsset. Use the native mesh setter.
        obj.set_skeletal_mesh_asset(value)
    elif isinstance(obj, unreal.StaticMeshComponent) and name == 'static_mesh':
        obj.set_static_mesh(value)
    else:
        obj.set_editor_property(name, value, notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)


def roundtrip_manifest(text):
    # Detached struct: never import text into the wrapper obtained from a live
    # property. This avoids corrupting live instanced-fragment storage.
    result = unreal.AZ_Inv_CommonUI_ItemManifest()
    require(result.import_text(text), 'Manifest ImportText failed')
    return result


def manifest_text(H, path, rounds=None):
    is_magazine = rounds is not None
    ref, encode = H['object_reference'], H['encode_fields']
    def fragment(name, fields):
        return '/Script/AZ.AZ_Inv_CommonUI_' + name + encode(list(fields.items()))
    def tag(name):
        return '(TagName="' + name + '")'
    def text(name, value):
        return fragment('Text_Fragment', {'FragmentTag': tag(name), 'FragmentText': json.dumps(value)})
    title = 'Pistol magazine' if is_magazine else 'Pistol'
    description = 'Detachable pistol magazine. Each magazine keeps its remaining rounds.' if is_magazine else 'Semi-automatic pistol with a detachable magazine.'
    fragments = [
        fragment('GridFragment', {'GridSize': '(X=1,Y=1)' if is_magazine else '(X=2,Y=2)', 'FragmentTag': tag('Item.Fragment.Grid')}),
        fragment('ImageFragment', {'Icon': ref('/Script/Engine.Texture2D', MAG_ICON if is_magazine else ICON),
                                  'IconDimensions': '(X=64,Y=64)' if is_magazine else '(X=128,Y=96)', 'FragmentTag': tag('Item.Fragment.Icon')}),
        text('Item.Fragment.Name.StaticText', title), text('Item.Fragment.Ammo.Primary.Name', title),
        text('Item.Fragment.Text', description), text('Item.Fragment.Description', description),
        text('Item.Fragment.Ammo.Text', 'Magazine:'), text('Item.Fragment.Ammo.Value', ''),
        text('Item.Fragment.Ammo.Separator', '/'), text('Item.Fragment.Ammo.MaxValue', ''),
        text('Item.Fragment.Ammo.StaticText', 'rounds'),
    ]
    if is_magazine:
        fragments.append(fragment('MagazineFragment', {'MagazineFamily': json.dumps(FAMILY), 'Capacity': str(CAPACITY), 'InitialRounds': str(rounds)}))
    else:
        fragments += [
            fragment('EquipmentFragment', {'EquipmentType': tag('Item.Type.Weapon.Pistol'), 'FragmentTag': tag('Item.Fragment.Equipment')}),
            fragment('WeaponStateFragment', {
                'bUsesDetachableMagazines': 'True', 'MagazineFamily': json.dumps(FAMILY),
                'WeaponActorClass': ref('/Script/Engine.BlueprintGeneratedClass', WEAPON, True),
                'AnimationProfile': ref('/Script/AZ.AZ_WeaponAnimationProfile', PROFILE),
                'ReticleDefinition': ref('/Script/AZ.AZ_HUDReticleDefinition', RETICLE),
                'WeaponTag': tag('Weapon.Pistol'), 'DefaultFireMode': 'Single', 'SupportedFireModes': '(Single)',
                'CurrentClipAmmo': '0', 'MaxClipAmmo': '0', 'CurrentReserveAmmo': '0', 'MaxReserveAmmo': '0',
                'FireRate': '4.0', 'BaseDamage': '20.0', 'MaxRange': '6000.0', 'SpreadBase': '1.0', 'SpreadAim': '0.65',
                'MuzzleSocketName': '"MuzzleFlash"', 'bRequiresAimToFire': 'True', 'bInfiniteAmmo': 'False',
                'FireSound': ref('/Script/Engine.SoundCue', SOUND), 'MuzzleFlash': 'None',
                'CascadeMuzzleFlash': ref('/Script/Engine.ParticleSystem', FLASH),
                'WorldImpactEffect': ref('/Script/Engine.ParticleSystem', IMPACT), 'WorldImpactScale': '1.0',
                'ShotNoiseLoudness': '1.0', 'ShotNoiseMaxRange': '2200.0',
                'Recoil': '(bEnabled=True,SpreadRadiusPerShotDegrees=0.16,MaxAdditionalSpreadRadiusDegrees=1.2,SpreadRecoveryDelaySeconds=0.18,SpreadRecoverySpeedDegreesPerSecond=1.5,bCameraKickEnabled=True,CameraPitchKickDegrees=0.8,CameraYawKickRadiusDegrees=0.2,MaxCameraPitchDegrees=4.0,MaxCameraYawDegrees=1.2,CameraRecoveryDelaySeconds=0.16,CameraRecoverySpeedDegreesPerSecond=8.0)',
            }),
            fragment('AbilityGrantFragment', {'AbilitiesToGrant': '(' + ','.join(ref('/Script/Engine.BlueprintGeneratedClass', ability_path(row[0]), True) for row in ABILITIES) + ')'}),
        ]
    return encode([('Fragments', '(' + ','.join(fragments) + ')'), ('ItemCategory', 'Equippable'),
                   ('ItemTypeTag', tag('Item.Type.Magazine.Pistol' if is_magazine else 'Item.Type.Weapon.Pistol')),
                   ('PickupActorClass', ref('/Script/Engine.BlueprintGeneratedClass', path, True))])


def author_ability(H, name, native_name, input_tag):
    native = getattr(unreal, native_name)
    bp = create_blueprint(ability_path(name), native)
    cdo = unreal.get_default_object(bp.generated_class())
    require(isinstance(cdo, native), 'Wrong native ability parent: ' + name)
    bp.modify()
    for key, value in {
        'input_tag': H['tag'](input_tag), 'activation_required_tags': H['tags']('Weapon.Pistol'),
        'activation_owned_tags': H['tags'](), 'source_object_must_equal_current_weapon_to_activate': False,
        'activate_ability_on_granted': False,
    }.items():
        set_property(cdo, key, value)


def animation_record(sequence):
    return {'path': sequence.get_path_name(), 'length': float(unreal.AnimationLibrary.get_sequence_length(sequence)),
            'rate_scale': float(sequence.get_editor_property('rate_scale')),
            'skeleton': sequence.get_editor_property('skeleton').get_path_name(),
            'notifies': [event.export_text() for event in unreal.AnimationLibrary.get_animation_notify_events(sequence)]}


def author_weapon_fire():
    source = load(MESH_FIRE_SOURCE, unreal.AnimSequence)
    before = animation_record(source)
    source_disk = source_file(MESH_FIRE_SOURCE)
    source_hash = hashlib.sha256(source_disk.read_bytes()).hexdigest()
    sequence = owned_or_missing(MESH_FIRE, unreal.AnimSequence)
    if sequence is None:
        sequence = EAL.duplicate_asset(MESH_FIRE_SOURCE, MESH_FIRE)
        require(isinstance(sequence, unreal.AnimSequence), 'Could not duplicate pistol weapon firing clip')
        claim(sequence)
    sequence.modify()
    # The source has Cascade + sound notifies. Accepted-shot presentation owns
    # both effects; this duplicate supplies only slide/trigger animation.
    for track in unreal.AnimationLibrary.get_animation_notify_track_names(sequence):
        unreal.AnimationLibrary.remove_animation_notify_events_by_track(sequence, track)
    require(not unreal.AnimationLibrary.get_animation_notify_events(sequence), 'Weapon firing duplicate still contains notifies')
    require(animation_record(source) == before and hashlib.sha256(source_disk.read_bytes()).hexdigest() == source_hash,
            'Source pack animation changed while creating the clean duplicate')
    EAL.set_metadata_tag(sequence, 'AZ.PistolInventory.FireSourceSHA256', source_hash)
    (OUTPUT / 'weapon-fire-source.json').write_text(json.dumps({'source': before, 'sha256': source_hash}, indent=2), encoding='utf-8')
    return sequence


def validate_weapon_fire():
    sequence = owned_or_missing(MESH_FIRE, unreal.AnimSequence)
    require(sequence is not None, 'Pistol weapon firing duplicate is missing')
    source = load(MESH_FIRE_SOURCE, unreal.AnimSequence)
    actual, original = animation_record(sequence), animation_record(source)
    require(not actual['notifies'], 'Duplicate weapon firing clip has cosmetic notifies')
    require(all(actual[key] == original[key] for key in ('length', 'rate_scale', 'skeleton')), 'Weapon firing clip timing/skeleton differs from source')
    baseline = json.loads((OUTPUT / 'weapon-fire-source.json').read_text(encoding='utf-8'))
    require(original == baseline['source'] and hashlib.sha256(source_file(MESH_FIRE_SOURCE).read_bytes()).hexdigest() == baseline['sha256'],
            'Pack source animation changed after pistol authoring')
    return actual


def author_weapon(H):
    bp = create_blueprint(WEAPON, unreal.AZ_Weapon)
    cdo = unreal.get_default_object(bp.generated_class())
    require(isinstance(cdo, unreal.AZ_Weapon), 'Wrong weapon actor parent')
    bp.modify()
    for key, value in dict(SOCKETS, weapon_tag=H['tag']('Weapon.Pistol'), spawn_with_collision=False,
                          weapon_mesh_fire_animation=load(MESH_FIRE, unreal.AnimSequence)).items():
        set_property(cdo, key, value)
    # Native inherited templates are the class's persistent component defaults.
    for name in ('SkeletalMesh', 'WeaponMesh1P', 'WeaponMesh3P'):
        comp = native_component(cdo, name, unreal.SkeletalMeshComponent)
        set_property(comp, 'skeletal_mesh_asset', load(MESH, unreal.SkeletalMesh) if name == 'WeaponMesh3P' else None)
        set_property(comp, 'relative_location', unreal.Vector(0, 0, 0))
        set_property(comp, 'relative_rotation', unreal.Rotator(0, 0, 0))
        set_property(comp, 'relative_scale3d', unreal.Vector(1, 1, 1))
        set_property(comp, 'visible', name == 'WeaponMesh3P')
        set_property(comp, 'hidden_in_game', name != 'WeaponMesh3P')
    set_property(native_component(cdo, 'Mesh', unreal.StaticMeshComponent), 'static_mesh', None)


def author_pickup(H, path, rounds=None):
    bp = create_blueprint(path, duplicate=PICKUP_TEMPLATE)
    cdo = unreal.get_default_object(bp.generated_class())
    require(isinstance(cdo, unreal.AZ_PickupItem), 'Wrong CommonUI pickup parent: ' + path)
    item = H['item_component_template'](bp)
    bp.modify()
    set_property(item, 'pickup_item_manifest', roundtrip_manifest(manifest_text(H, path, rounds)))
    set_property(item, 'initial_contained_item_manifests', [] if rounds is not None else [roundtrip_manifest(manifest_text(H, MAGAZINE, CAPACITY))])
    # Live magazine text derives actual rounds from the transferred payload.
    set_property(item, 'pickup_message', 'Press E to pick up pistol magazine' if rounds is not None else 'Press E to pick up pistol')
    set_property(native_component(cdo, 'SkeletalMesh', unreal.SkeletalMeshComponent), 'skeletal_mesh_asset', None)
    mesh = native_component(cdo, 'Mesh', unreal.StaticMeshComponent)
    set_property(mesh, 'static_mesh', load(MAGAZINE_MESH if rounds is not None else PICKUP_MESH, unreal.StaticMesh))
    set_property(mesh, 'relative_location', unreal.Vector(0, 0, 0))
    set_property(mesh, 'relative_rotation', unreal.Rotator(0, 0, 0))
    set_property(mesh, 'relative_scale3d', unreal.Vector(1, 1, 1))


def capture(H):
    rows = []
    for path in packages():
        bp = owned_or_missing(path)
        row = {'path': path, 'exists': bool(bp)}
        if bp:
            cdo = unreal.get_default_object(bp.generated_class())
            row['status'] = str(bp.get_editor_property('status'))
            if path == WEAPON:
                row['sockets'] = {key: str(cdo.get_editor_property(key)) for key in SOCKETS}
                row['weapon_tag'] = str(cdo.get_editor_property('weapon_tag').get_editor_property('tag_name'))
                actual_mesh = native_component(cdo, 'WeaponMesh3P', unreal.SkeletalMeshComponent).get_skeletal_mesh_asset()
                row['mesh'] = actual_mesh.get_path_name() if actual_mesh else None
                row['unused_skeletal_meshes'] = {
                    name: bool(native_component(cdo, name, unreal.SkeletalMeshComponent).get_skeletal_mesh_asset())
                    for name in ('SkeletalMesh', 'WeaponMesh1P')}
                fire = cdo.get_editor_property('weapon_mesh_fire_animation')
                row['mesh_fire'] = fire.get_path_name() if fire else None
            elif path.startswith(ABILITY_ROOT + '/'):
                row['input'] = str(cdo.get_editor_property('input_tag').get_editor_property('tag_name'))
                row['required'] = cdo.get_editor_property('activation_required_tags').export_text()
            else:
                item = H['item_component_template'](bp)
                row['manifest'] = item.get_editor_property('pickup_item_manifest').export_text()
                row['contained'] = [value.export_text() for value in item.get_editor_property('initial_contained_item_manifests')]
                row['prompt'] = item.get_editor_property('pickup_message')
                mesh = native_component(cdo, 'Mesh', unreal.StaticMeshComponent).get_editor_property('static_mesh')
                row['mesh'] = mesh.get_path_name() if mesh else None
                actual_skeletal = native_component(cdo, 'SkeletalMesh', unreal.SkeletalMeshComponent).get_skeletal_mesh_asset()
                row['skeletal_mesh'] = actual_skeletal.get_path_name() if actual_skeletal else None
        rows.append(row)
    return rows


def manifest_comparison(text):
    # Saving assigns stable localization identities to FText fields. Compare
    # their authored source strings while retaining every gameplay field exactly.
    quoted = r'"(?:\\.|[^"\\])*"'
    pattern = r'FragmentText=NSLOCTEXT\(\s*' + quoted + r'\s*,\s*' + quoted + r'\s*,\s*(' + quoted + r')\s*\)'
    return re.sub(pattern, lambda match: 'FragmentText=' + match.group(1), text)


def validate(H, rows, require_compiled=False):
    records = {row['path']: row for row in rows}
    require(all(row['exists'] for row in rows), 'Pistol assets are incomplete')
    actor = records[WEAPON]
    require(actor['sockets'] == SOCKETS and actor['weapon_tag'] == 'Weapon.Pistol', 'Pistol actor sockets/profile differ')
    require(actor['mesh'] == object_path(MESH) and actor['mesh_fire'] == object_path(MESH_FIRE), 'Pistol mesh/fire animation differs')
    require(not any(actor['unused_skeletal_meshes'].values()), 'An unused pistol skeletal component still has a rendered mesh')
    for name, _, input_tag in ABILITIES:
        row = records[ability_path(name)]
        require(row['input'] == input_tag and row['required'] == H['tags']('Weapon.Pistol').export_text(), 'Pistol ability input/profile differs: ' + name)
    for path, rounds in [(PICKUP, None)] + MAGAZINES:
        row = records[path]
        expected = roundtrip_manifest(manifest_text(H, path, rounds)).export_text()
        require(manifest_comparison(row['manifest']) == manifest_comparison(expected), 'Pistol manifest readback differs: ' + path)
        expected_children = [] if rounds is not None else [roundtrip_manifest(manifest_text(H, MAGAZINE, CAPACITY)).export_text()]
        require([manifest_comparison(value) for value in row['contained']]
                == [manifest_comparison(value) for value in expected_children], 'Initial contained magazines differ: ' + path)
        require(row['mesh'] == object_path(MAGAZINE_MESH if rounds is not None else PICKUP_MESH), 'Pickup mesh differs: ' + path)
        require(row['skeletal_mesh'] is None, 'Pickup still renders an inherited skeletal mesh: ' + path)
    if require_compiled:
        for path in packages():
            require(load(path, unreal.Blueprint).get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE,
                    'Compile through the native Blueprint tool before saving: ' + path)


def prerequisites(H):
    expected = [(PICKUP_TEMPLATE, unreal.Blueprint), (MESH, unreal.SkeletalMesh),
                (PICKUP_MESH, unreal.StaticMesh), (MAGAZINE_MESH, unreal.StaticMesh),
                (ICON, unreal.Texture2D), (MAG_ICON, unreal.Texture2D), (PROFILE, unreal.AZ_WeaponAnimationProfile),
                (RETICLE, unreal.AZ_HUDReticleDefinition), (SOUND, unreal.SoundBase),
                (FLASH, unreal.ParticleSystem), (IMPACT, unreal.ParticleSystem), (MESH_FIRE_SOURCE, unreal.AnimSequence)]
    report = [{'path': path, 'ready': isinstance(unreal.load_asset(path), cls)} for path, cls in expected]
    mesh = load(MESH, unreal.SkeletalMesh)
    report.append({'requirement': 'Pistol mesh MuzzleFlash socket', 'ready': mesh.find_socket('MuzzleFlash') is not None})
    # Unknown reflected names are fatal for authoring, never silently ignored.
    for name, obj, field in [
        ('Cascade muzzle support', unreal.AZ_Inv_CommonUI_WeaponStateFragment(), 'cascade_muzzle_flash'),
        ('Weapon mesh firing animation', unreal.get_default_object(unreal.AZ_Weapon), 'weapon_mesh_fire_animation'),
    ]:
        try:
            obj.get_editor_property(field)
            ready = True
        except Exception:
            ready = False
        report.append({'requirement': name, 'ready': ready})
    try:
        H['tag']('Item.Type.Magazine.Pistol')
        tag_ready = True
    except Exception:
        tag_ready = False
    report.append({'requirement': 'Item.Type.Magazine.Pistol native tag', 'ready': tag_ready})
    return report


def placement_proposal():
    """Receipt only: root chooses whether/where to place after compiling assets."""
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    if not world:
        return {'status': 'editor_world_unavailable'}
    template = load(PICKUP_TEMPLATE, unreal.Blueprint)
    anchors = [actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
               if actor.get_class() == template.generated_class()]
    anchors.sort(key=lambda actor: actor.get_actor_label())
    if not anchors:
        return {'status': 'no_rifle_anchor', 'map': world.get_path_name()}
    anchor = anchors[0]
    p = anchor.get_actor_location()
    targets = [('AZ_Pistol_Pickup', PICKUP)] + [('AZ_Pistol_Magazine_' + label, path)
               for label, (path, _) in zip(('Full', 'Partial', 'Empty'), MAGAZINES)]
    return {'status': 'proposal_only_no_actors_spawned', 'map': world.get_path_name(),
            'anchor': anchor.get_actor_label(), 'anchor_location': [p.x, p.y, p.z],
            'proposed': [{'label': label, 'asset': path, 'location': [p.x + 180, p.y + i * 60, p.z]}
                         for i, (label, path) in enumerate(targets)]}


def main(mode='audit'):
    require(mode in ('audit', 'author', 'save', 'verify'), 'Mode must be audit, author, save or verify')
    try:
        H = helpers()
        OUTPUT.mkdir(parents=True, exist_ok=True)
        ready = prerequisites(H)
        before = capture(H)
        owned_or_missing(MESH_FIRE, unreal.AnimSequence)
        report = {'mode': mode, 'prerequisites': ready, 'assets': before,
                  'compile_after_return': [object_path(path) for path in packages()],
                  'placement': placement_proposal(), 'capacity_note': '15 is an editable gameplay default'}
        if mode != 'audit':
            require(all(row['ready'] for row in ready), 'Missing dependencies/native build; run audit and inspect receipt')
        if mode in ('author', 'save'):
            require(not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before asset authoring or saving')
        if mode == 'author':
            dirty = {p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
            require(not dirty.intersection(owned_packages()), 'A target has unsaved edits; preserve/review them before reauthoring')
            backup = ROOT / 'Saved/Backups/PistolInventory' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
            backup.mkdir(parents=True)
            (backup / 'before.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
            for path in owned_packages():
                file = source_file(path)
                if file.is_file():
                    dest = backup / file.relative_to(ROOT / 'Content')
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(file, dest)
            owned_or_missing(MESH_FIRE, unreal.AnimSequence)
            author_weapon_fire()
            for row in ABILITIES:
                author_ability(H, *row)
            author_weapon(H)
            # Create every pickup class before importing references to their generated classes.
            for path in [PICKUP] + [row[0] for row in MAGAZINES]:
                create_blueprint(path, duplicate=PICKUP_TEMPLATE)
            for path, rounds in MAGAZINES:
                author_pickup(H, path, rounds)
            author_pickup(H, PICKUP)
            report.update(status='authored_requires_native_compile_then_save', backup=str(backup), assets=capture(H))
            validate(H, report['assets'])
            report['weapon_fire_animation'] = validate_weapon_fire()
        elif mode in ('save', 'verify'):
            validate(H, before, require_compiled=True)
            report['weapon_fire_animation'] = validate_weapon_fire()
            if mode == 'save':
                for path in owned_packages():
                    require(EAL.save_loaded_asset(load(path, unreal.AnimSequence if path == MESH_FIRE else unreal.Blueprint), only_if_is_dirty=False), 'Asset save failed: ' + path)
            dirty = {p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
            require(not dirty.intersection(owned_packages()), 'Pistol packages still need saving')
            files = []
            for path in owned_packages():
                file = source_file(path)
                require(file.is_file(), 'Saved package missing: ' + str(file))
                files.append({'path': str(file), 'sha256': hashlib.sha256(file.read_bytes()).hexdigest()})
            report.update(status='verified_saved', assets=capture(H), files=files)
        (OUTPUT / (mode + '-readback.json')).write_text(json.dumps(report, indent=2), encoding='utf-8')
        print('PISTOL_INVENTORY ' + json.dumps({'mode': mode, 'status': report.get('status', 'audit'),
                                              'ready': all(row['ready'] for row in ready), 'receipt': str(OUTPUT / (mode + '-readback.json'))}))
        return report
    finally:
        gc.collect()


if __name__ == '__main__':
    main('audit')
