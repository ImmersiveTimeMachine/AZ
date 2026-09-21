# @Description: Author neutral quest examples and persistent pickup identities in an Idle editor.
"""Import/offline planning is inert. Root executes editor stages explicitly.

prepare_assets() -> native BP compile -> configure_assets() -> native BP compile
and explicit save -> assign_pickup_ids() -> explicit level save -> reviewed
placement_plan(...) -> place_examples(plan, geometry_reviewed=True) -> explicit save.
No compile/save/PIE/test calls are hidden in this recipe. Originals, existing actor
transforms and compass preview actors are preserved. See quest_content_README.md.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/QuestMapImplementation/ContentAuthoring'
AUDIT = ROOT / 'Saved/QuestMapImplementation/reopened-editor-audit.json'
DEST = '/Game/AZ/Blueprints/Quests/Examples'
WORLD_PACKAGE = '/Game/AZ/Maps/L_001'
WORLD_OBJECT = WORLD_PACKAGE + '.L_001'
MAP_ID, LAYER_ID = 'L_001', 'Outdoor'
OWNER_KEY, OWNER = 'AZ.Quest.ContentAuthor', 'quest_content_setup:v1'
STAGE_KEY = 'AZ.Quest.ContentStage'
ACTOR_TAG = 'AZ.Quest.Example'
MAIN_ID, SIDE_ID = 'TEST.Main.Route', 'TEST.Side.Inventory'
DEFINITIONS = {
    'main': DEST + '/DA_AZ_Quest_Example_Main',
    'side': DEST + '/DA_AZ_Quest_Example_Side',
}
# Native runtime behavior is inherited; these BPs only contain authored defaults.
BLUEPRINTS = {
    'main_offer': (DEST + '/BP_AZ_QuestExample_MainOffer', 'AZ_QuestWorldActor'),
    'main_reach': (DEST + '/BP_AZ_QuestExample_MainReach', 'AZ_QuestReachArea'),
    'main_use': (DEST + '/BP_AZ_QuestExample_MainUse', 'AZ_QuestWorldActor'),
    'optional_reach': (DEST + '/BP_AZ_QuestExample_OptionalReach', 'AZ_QuestReachArea'),
    'side_offer': (DEST + '/BP_AZ_QuestExample_SideOffer', 'AZ_QuestWorldActor'),
    'side_delivery': (DEST + '/BP_AZ_QuestExample_SideDelivery', 'AZ_QuestWorldActor'),
    'checkpoint': (DEST + '/BP_AZ_QuestExample_Checkpoint', 'AZ_CampaignCheckpoint'),
}
TARGET_IDS = {key: 'TEST.L001.' + key for key in BLUEPRINTS if key != 'checkpoint'}
CHECKPOINT_ID = 'TEST.L001.Checkpoint'


def ue():
    import unreal
    return unreal


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def write(name, data):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / (name + '.json')).write_text(json.dumps(data, indent=2), encoding='utf-8')
    return data


def idle():
    response = ue().ToolsetRegistry.execute_tool('EditorToolset.EditorAppToolset', 'IsPIERunning', '{}')
    require(response.is_complete and not response.error, 'Cannot establish editor state: ' + str(response.error))
    require(not json.loads(response.value)['returnValue'], 'PIE active: leave the user in control and stop authoring.')


def package_file(package, map_package=False):
    require(package.startswith('/Game/') and ':' not in package and '.' not in package,
            'Unsupported package path: ' + package)
    path = (ROOT / 'Content' / (package[6:] + ('.umap' if map_package else '.uasset'))).resolve()
    require(path.is_relative_to((ROOT / 'Content').resolve()), 'Package escaped project content.')
    return path


def backup_packages(packages, purpose):
    """Refuse unsaved user work in packages whose current disk state is backed up."""
    u = ue()
    dirty = {str(p.get_name()) for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    dirty |= {str(p.get_name()) for p in u.EditorLoadingAndSavingUtils.get_dirty_map_packages()}
    require(not (set(packages) & dirty), 'Save existing touched packages first: ' + str(sorted(set(packages) & dirty)))
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    folder = OUT / ('Backup_' + purpose + '_' + stamp)
    folder.mkdir(parents=True, exist_ok=True)
    rows = []
    for package in sorted(set(packages)):
        source = package_file(package, package == WORLD_PACKAGE)
        require(source.exists(), 'Cannot back up unsaved package: ' + package)
        target = folder / source.relative_to(ROOT / 'Content')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        rows.append({'package': package, 'source': str(source), 'backup': str(target),
                     'sha256': hashlib.sha256(source.read_bytes()).hexdigest()})
    return write('backup-' + purpose + '-' + stamp, {'purpose': purpose, 'packages': rows})


def source_choice():
    """Read the real source receipt; no guessed item tag or visual-name inference."""
    audit = json.loads(AUDIT.read_text(encoding='utf-8'))
    require(audit['world'] == WORLD_OBJECT, 'Source pickup receipt is for a different map.')
    wanted = '/Game/AZ/Blueprints/Items/Consumables/BPAZ_CommonUI_Potion_Blue.BPAZ_CommonUI_Potion_Blue_C'
    rows = [p for p in audit['pickups'] if p['class'] == wanted]
    require(rows, 'Verified blue sample pickup is absent from source receipt.')
    tags = {re.search(r'ItemTypeTag=\(TagName="([^"]+)"\)', p['manifest']).group(1) for p in rows}
    require(tags == {'Item.Type.Consumable.Buff'}, 'Blue sample type changed; review the content choice.')
    require(all('ItemCategory=Consumable' in p['manifest'] and 'StackCount=1' in p['manifest'] for p in rows),
            'Source sample capacity/category needs review.')
    return {'class': wanted, 'item_type': next(iter(tags)), 'count': 1,
            'source_components': [p['component'] for p in rows],
            'receipt': str(AUDIT), 'receipt_sha256': hashlib.sha256(AUDIT.read_bytes()).hexdigest()}


def specification():
    """File-only plan and stable identities. Offer actors accept; they are not progress objectives."""
    choice = source_choice()
    return {'map_id': MAP_ID, 'layer_id': LAYER_ID, 'sample_only': True, 'source_choice': choice,
            'quests': {
                'main': {'quest_id': MAIN_ID, 'title': 'TEST — Route and panel', 'category': 'STORY',
                         'description': 'Neutral implementation example. Accept, reach the marked area, then use the test panel. No story canon.',
                         'objectives': [
                             {'id': 'Reach', 'kind': 'REACH_AREA', 'text': 'TEST: reach the marked area', 'target': TARGET_IDS['main_reach']},
                             {'id': 'UsePanel', 'kind': 'INTERACT', 'text': 'TEST: use the panel to finish', 'target': TARGET_IDS['main_use'], 'requires': ['Reach']},
                             {'id': 'OptionalReach', 'kind': 'REACH_AREA', 'text': 'Optional TEST: visit the second area before finishing',
                              'target': TARGET_IDS['optional_reach'], 'requires': ['Reach'], 'optional': True},
                         ]},
                'side': {'quest_id': SIDE_ID, 'title': 'TEST — Supply hand-off', 'category': 'SIDE',
                         'description': 'Neutral inventory example. After accepting, possess one blue sample item and deliver one matching item. No healing/story claim.',
                         'objectives': [
                             {'id': 'PossessSample', 'kind': 'POSSESS_ITEM', 'text': 'TEST: have one blue sample in the backpack', 'item_type': choice['item_type']},
                             {'id': 'DeliverSample', 'kind': 'DELIVER_ITEM', 'text': 'TEST: deliver one sample', 'item_type': choice['item_type'],
                              'target': TARGET_IDS['side_delivery'], 'requires': ['PossessSample']},
                         ]}},
            'definitions': DEFINITIONS, 'blueprints': BLUEPRINTS, 'target_ids': TARGET_IDS,
            'checkpoint_id': CHECKPOINT_ID}


def load(path):
    asset = ue().load_asset(path)
    require(asset is not None, 'Missing asset: ' + path)
    return asset


def owned(path):
    asset = load(path)
    require(ue().EditorAssetLibrary.get_metadata_tag(asset, OWNER_KEY) == OWNER, 'Refusing unowned destination: ' + path)
    return asset


def required_classes():
    result = {}
    for name in {'AZ_QuestDefinition', 'AZ_NavigationTargetComponent', 'AZ_Inv_CommonUI_ItemComponent'} | {v[1] for v in BLUEPRINTS.values()}:
        result[name] = ue().load_class(None, '/Script/AZ.' + name)
        require(result[name] is not None, 'Required reflected class is not loaded: ' + name)
    return result


def prepare_assets():
    idle()
    u = ue(); classes = required_classes(); spec = specification()
    all_paths = list(DEFINITIONS.values()) + [v[0] for v in BLUEPRINTS.values()]
    # Validate every destination's ownership before creating any asset.
    existing = [p for p in all_paths if u.EditorAssetLibrary.does_asset_exist(p)]
    for path in existing:
        owned(path)
    created = []
    for path in DEFINITIONS.values():
        if path in existing:
            continue
        factory = u.DataAssetFactory()
        factory.set_editor_property('data_asset_class', classes['AZ_QuestDefinition'])
        asset = u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/', 1)[1], DEST, u.DataAsset, factory)
        require(asset is not None, 'Quest DataAsset factory failed: ' + path)
        u.EditorAssetLibrary.set_metadata_tag(asset, OWNER_KEY, OWNER)
        u.EditorAssetLibrary.set_metadata_tag(asset, STAGE_KEY, 'prepared')
        created.append(path)
    for path, native in BLUEPRINTS.values():
        if path in existing:
            continue
        factory = u.BlueprintFactory(); factory.set_editor_property('parent_class', classes[native])
        bp = u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/', 1)[1], DEST, u.Blueprint, factory)
        require(bp is not None, 'Blueprint factory failed: ' + path)
        u.EditorAssetLibrary.set_metadata_tag(bp, OWNER_KEY, OWNER)
        u.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'prepared')
        created.append(path)
    return write('assets-prepared', {'created': created, 'existing': existing, 'specification': spec,
                                    'next': 'Dedicated native BP compile, then configure_assets(). No level mutations.'})


def gameplay_tag(name):
    result = ue().GameplayTag()
    require(result.import_text(name) and str(result.get_editor_property('tag_name')) == name,
            'Item type is not a registered gameplay tag: ' + name)
    return result


def validate_native(result, description):
    # UE Python collapses bool + FString out to Optional[str]: None means false.
    # These validators leave the error empty on success; truthiness is incorrect.
    if isinstance(result, str):
        require(result == '', description + ': ' + result)
    elif isinstance(result, tuple):
        flags = [v for v in result if isinstance(v, bool)]
        require(len(flags) == 1 and flags[0], description + ': ' + str(result))
    else:
        require(result is True, description + ': unexpected/failed result ' + str(result))


def objective(row):
    u = ue(); value = u.AZ_QuestObjectiveDefinition()
    value.set_editor_property('ObjectiveId', u.Name(row['id']))
    value.set_editor_property('Description', u.Text(row['text']))
    value.set_editor_property('Kind', getattr(u.AZ_QuestObjectiveKind, row['kind']))
    value.set_editor_property('RequiredCount', 1)
    value.set_editor_property('bOptional', row.get('optional', False))
    value.set_editor_property('bFailureFailsQuest', False)
    value.set_editor_property('PrerequisiteObjectives', [u.Name(x) for x in row.get('requires', [])])
    value.set_editor_property('InteractionRadius', 250.0)
    if 'item_type' in row:
        value.set_editor_property('ItemType', gameplay_tag(row['item_type']))
    descriptor = u.AZ_NavigationTargetDescriptor()
    if 'target' in row:
        descriptor.set_editor_property('TargetId', u.Name(row['target']))
        descriptor.set_editor_property('MapId', u.Name(MAP_ID))
        descriptor.set_editor_property('LayerId', u.Name(LAYER_ID))
    value.set_editor_property('Target', descriptor)
    return value


def catalog_readback():
    u = ue(); result = {}
    definitions = [owned(p) for p in DEFINITIONS.values()]
    for key, path in DEFINITIONS.items():
        data = owned(path); validate_native(data.validate_definition(), 'Invalid ' + path)
        result[key] = {'path': path, 'quest_id': str(data.get_editor_property('QuestId')),
                       'title': str(data.get_editor_property('Title')),
                       'category': str(data.get_editor_property('Category')),
                       'objectives': [o.export_text() for o in data.get_editor_property('Objectives')]}
    validate_native(u.AZ_QuestDefinition.validate_quest_catalog(definitions), 'Catalog validation')
    return result


def configure_assets():
    idle()
    u = ue(); spec = specification(); required_classes()
    paths = list(DEFINITIONS.values()) + [v[0] for v in BLUEPRINTS.values()]
    assets = {p: owned(p) for p in paths}
    if all(u.EditorAssetLibrary.get_metadata_tag(a, STAGE_KEY) == 'configured' for a in assets.values()):
        return write('configure-noop', {'catalog': catalog_readback()})
    require(all(u.EditorAssetLibrary.get_metadata_tag(a, STAGE_KEY) == 'prepared' for a in assets.values()),
            'Partial/unknown configuration stage; inspect instead of overwriting.')
    for key, (path, _) in BLUEPRINTS.items():
        require(assets[path].generated_class() is not None, 'Compile Blueprint first: ' + path)
    for asset in assets.values():
        u.EditorAssetLibrary.set_metadata_tag(asset, STAGE_KEY, 'configuring')
    for key, row in spec['quests'].items():
        data = assets[DEFINITIONS[key]]
        data.set_editor_property('QuestId', u.Name(row['quest_id']))
        data.set_editor_property('Title', u.Text(row['title']))
        data.set_editor_property('Description', u.Text(row['description']))
        data.set_editor_property('Category', getattr(u.AZ_QuestCategory, row['category']))
        data.set_editor_property('PrerequisiteQuests', [])
        data.set_editor_property('bRequireOptionalObjectives', False)
        values = [objective(o) for o in row['objectives']]
        data.set_editor_property('Objectives', values)
        require([x.export_text() for x in data.get_editor_property('Objectives')] == [x.export_text() for x in values],
                'Objective struct-array write did not persist: ' + DEFINITIONS[key])
    defaults = {
        'main_offer': {'Action': u.AZ_QuestWorldAction.OFFER_QUEST, 'OfferDefinition': assets[DEFINITIONS['main']],
                       'InteractionPrompt': u.Text('Press E — accept TEST main quest')},
        'side_offer': {'Action': u.AZ_QuestWorldAction.OFFER_QUEST, 'OfferDefinition': assets[DEFINITIONS['side']],
                       'InteractionPrompt': u.Text('Press E — accept TEST side quest')},
        'main_use': {'Action': u.AZ_QuestWorldAction.INTERACT_OBJECTIVE, 'QuestId': u.Name(MAIN_ID), 'ObjectiveId': u.Name('UsePanel'),
                     'InteractionPrompt': u.Text('Press E — finish TEST panel objective')},
        'side_delivery': {'Action': u.AZ_QuestWorldAction.DELIVER_ITEMS, 'QuestId': u.Name(SIDE_ID), 'ObjectiveId': u.Name('DeliverSample'),
                          'InteractionPrompt': u.Text('Press E — deliver one TEST sample')},
        'main_reach': {'QuestId': u.Name(MAIN_ID), 'ObjectiveId': u.Name('Reach')},
        'optional_reach': {'QuestId': u.Name(MAIN_ID), 'ObjectiveId': u.Name('OptionalReach')},
        'checkpoint': {'SaveRadius': 250.0, 'bEnabled': True},
    }
    for key, (path, _) in BLUEPRINTS.items():
        bp = assets[path]
        require(bp.generated_class() is not None, 'Compile Blueprint first: ' + path)
        cdo = u.get_default_object(bp.generated_class())
        for name, value in defaults[key].items():
            cdo.set_editor_property(name, value)
        # Target/Checkpoint identities are authored on placed INSTANCES only.
    result = catalog_readback()
    for asset in assets.values():
        u.EditorAssetLibrary.set_metadata_tag(asset, STAGE_KEY, 'configured')
    return write('catalog-configured', {'catalog': result, 'source_choice': spec['source_choice'],
                                       'blueprints': BLUEPRINTS, 'next': 'Native BP compile and explicit save. No auto-accept graph.'})


def editor_scene():
    idle(); u = ue()
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    require(world is not None and world.get_path_name() == WORLD_OBJECT, 'Only the reviewed L_001 editor world is authorized.')
    actors = [a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if a.get_world() == world]
    # Engine EditorActorSubsystem.cpp:369 explicitly excludes templates/CDOs,
    # transient actors, pending-kill actors, builder brush and WorldSettings.
    return world, actors


def pickup_candidates(actors):
    u = ue(); candidates, skipped = [], []
    subsystem = u.get_engine_subsystem(u.SubobjectDataSubsystem)
    lib = u.SubobjectDataBlueprintFunctionLibrary
    for actor in actors:
        # CreationMethod is not exposed by this editor's Python bindings.
        # Editor subobject data resolves SCS templates to their live instances.
        # UCS/instance-only components have no authored template and are excluded.
        authored = set()
        for handle in subsystem.k2_gather_subobject_data_for_instance(actor):
            data = lib.get_data(handle)
            instance = lib.get_associated_object(data)
            if instance and (lib.is_native_component(data) or
                             (not lib.is_instanced_component(data) and lib.get_blueprint(data) is not None)):
                authored.add(instance.get_path_name())
        for component in actor.get_components_by_class(u.AZ_Inv_CommonUI_ItemComponent):
            if (component.get_owner() != actor or component.get_outer() != actor or
                    component.get_world() != actor.get_world() or component.get_package() != actor.get_package() or
                    component.get_path_name() not in authored):
                skipped.append({'component': component.get_path_name(), 'reason': 'Not an actor-owned serialized Native/SCS component.'})
                continue
            candidates.append(component)
    return candidates, skipped


def guid_string(value):
    return value.to_string()  # native Guid ScriptMethod, not Python wrapper str(...)


def set_pickup_id(component, value):
    # PropertyAccessUtil classifies edit-inline components inside a World asset
    # as templates. Edit only the verified instance's reference-backed Guid;
    # Modify() records undo and package dirtiness, without archetype propagation.
    owner = component.get_owner()
    require(owner and component.get_outer() == owner and owner.get_world().get_path_name() == WORLD_OBJECT,
            'Campaign identity may only be authored on a placed component instance.')
    component.modify(); owner.modify()
    instance_value = component.get_editor_property('CampaignPickupId')
    require(instance_value.import_text(value.export_text()), 'Placed pickup Guid import failed')
    require(guid_string(component.get_editor_property('CampaignPickupId')) == guid_string(value), 'Pickup ID readback failed')


def inspect_pickup_ids():
    _, actors = editor_scene(); u = ue()
    candidates, skipped = pickup_candidates(actors)
    rows, seen, duplicates = [], {}, []
    for component in candidates:
        value = component.get_editor_property('CampaignPickupId')
        valid = u.GuidLibrary.is_valid_guid(value)
        key = guid_string(value) if valid else None
        row = {'component': component.get_path_name(), 'actor': component.get_owner().get_path_name(),
               'package': component.get_owner().get_package().get_name(), 'guid': key, 'empty': not valid}
        rows.append(row)
        if valid:
            if key in seen:
                duplicates.append({'guid': key, 'components': [seen[key], row['component']]})
            seen[key] = row['component']
    return write('pickup-guid-preflight', {'world': WORLD_OBJECT, 'rows': rows, 'skipped': skipped, 'duplicates': duplicates})


def assign_pickup_ids():
    """Existing actors: CampaignPickupId is the ONLY property changed."""
    u = ue(); before = inspect_pickup_ids()
    require(not before['duplicates'], 'Duplicate authored GUIDs: no IDs changed. Resolve the reported instances explicitly.')
    missing = [r for r in before['rows'] if r['empty']]
    if not missing:
        return write('pickup-guids-noop', before)
    backup = backup_packages({r['package'] for r in missing}, 'pickup-guids')
    changed = []
    for row in missing:
        component = u.load_object(None, row['component'])
        require(component is not None, 'Pickup disappeared since preflight.')
        require(not u.GuidLibrary.is_valid_guid(component.get_editor_property('CampaignPickupId')), 'Pickup ID changed after preflight; stopped.')
        component.modify(); component.get_owner().modify()
        value = u.GuidLibrary.new_guid()
        set_pickup_id(component, value)
        require(guid_string(component.get_editor_property('CampaignPickupId')) == guid_string(value), 'GUID write readback failed.')
        changed.append({**row, 'guid': guid_string(value)})
    after = inspect_pickup_ids()
    require(not after['duplicates'] and not any(r['empty'] for r in after['rows']), 'Final GUID audit failed.')
    return write('pickup-guids-assigned', {'backup': backup, 'changed': changed, 'after': after,
                                         'saved': False, 'next': 'Explicitly save the listed level/actor packages before Play or shutdown.'})


def placement_plan(anchor=(0.0, 0.0), ground_z=0.0):
    """FILE-ONLY proposed coordinates; root must review real floor/clearance first."""
    require(len(anchor) == 2 and all(math.isfinite(float(v)) for v in (*anchor, ground_z)), 'Invalid planning anchor.')
    offsets = {'main_offer': (300, 300, 80), 'main_reach': (1100, 300, 110), 'main_use': (1900, 300, 80),
               'optional_reach': (1100, 1000, 110), 'side_offer': (300, -500, 80),
               'side_delivery': (1900, -500, 80), 'checkpoint': (-400, 300, 100), 'sample_pickup': (1050, -500, 100)}
    rows = []
    for key, (x, y, z) in offsets.items():
        rows.append({'key': key, 'label': 'QuestExample_' + key, 'position': [anchor[0] + x, anchor[1] + y, ground_z + z],
                     'yaw': 0.0, 'target_id': TARGET_IDS.get(key),
                     'blueprint': BLUEPRINTS[key][0] if key in BLUEPRINTS else None})
    return {'world': WORLD_OBJECT, 'owner': OWNER, 'sample_only': True, 'geometry_verified': False,
            'source_choice': source_choice(), 'actors': rows,
            'notes': ['Proposed Z values are not terrain measurements. Verify actual pickup mesh/floor alignment.',
                      'Keep checkpoint/interaction actor origin above the floor so visibility trace does not hit the ground endpoint.']}


def place_examples(plan, *, geometry_reviewed=False):
    """Only explicitly reviewed coordinates; tag-owned reruns reuse actors and never move them."""
    require(geometry_reviewed, 'Root must inspect floor/clearance and explicitly approve these exact coordinates.')
    require(plan['world'] == WORLD_OBJECT and plan['owner'] == OWNER, 'Foreign placement plan.')
    require({r['key'] for r in plan['actors']} == set(BLUEPRINTS) | {'sample_pickup'} and len(plan['actors']) == 8,
            'Plan must contain each of the eight fixtures exactly once.')
    _, actors = editor_scene(); u = ue(); required_classes()
    catalog_readback()
    require(plan['source_choice'] == source_choice(), 'Pickup receipt/class changed; regenerate and review the placement plan.')
    existing = {}
    classes = {}
    for row in plan['actors']:
        tag = ACTOR_TAG + '.' + row['key']
        matches = [a for a in actors if u.Name(ACTOR_TAG) in a.tags and u.Name(tag) in a.tags]
        require(len(matches) <= 1, 'Duplicate owned fixture: ' + row['key'])
        require(len(row['position']) == 3 and all(math.isfinite(float(v)) for v in row['position'])
                and math.isfinite(float(row['yaw'])), 'Invalid fixture transform.')
        if matches:
            existing[row['key']] = matches[0]
        if row['key'] == 'sample_pickup':
            classes[row['key']] = u.load_class(None, plan['source_choice']['class'])
        else:
            bp = owned(BLUEPRINTS[row['key']][0])
            require(u.EditorAssetLibrary.get_metadata_tag(bp, STAGE_KEY) == 'configured', 'Configure/compile every example BP before placement.')
            classes[row['key']] = bp.generated_class()
        require(classes[row['key']] is not None, 'Fixture class is unavailable: ' + row['key'])
        if matches:
            require(matches[0].get_class() == classes[row['key']], 'Owned fixture class changed; inspect instead of replacing.')
    # Do not hijack another actor's navigation identity, even if labels match.
    for actor in actors:
        for nav in actor.get_components_by_class(u.AZ_NavigationTargetComponent):
            identity = str(nav.get_editor_property('TargetId'))
            for key, target in TARGET_IDS.items():
                require(identity != target or existing.get(key) == actor, 'Foreign/duplicate target identity: ' + identity)
    backup = backup_packages({WORLD_PACKAGE}, 'example-placement') if len(existing) != len(plan['actors']) else None
    subsystem = u.get_editor_subsystem(u.EditorActorSubsystem)
    result = []
    for row in plan['actors']:
        key = row['key']; actor = existing.get(key); created = actor is None
        cls = classes[key]
        if created:
            actor = subsystem.spawn_actor_from_class(cls, u.Vector(*row['position']), u.Rotator(yaw=row['yaw']), transient=False)
            require(actor is not None, 'Fixture spawn failed: ' + key)
            actor.set_editor_property('tags', list(actor.tags) + [u.Name(ACTOR_TAG), u.Name(ACTOR_TAG + '.' + key)])
            actor.set_actor_label(row['label']); actor.set_folder_path('QuestMap/TEST_Examples')
            if key in TARGET_IDS:
                nav = actor.get_component_by_class(u.AZ_NavigationTargetComponent)
                require(nav is not None, 'Fixture lacks native navigation target.')
                nav.set_editor_property('TargetId', u.Name(TARGET_IDS[key]))
                nav.set_editor_property('MapId', u.Name(MAP_ID)); nav.set_editor_property('LayerId', u.Name(LAYER_ID))
            if key == 'checkpoint':
                actor.set_editor_property('CheckpointId', u.Name(CHECKPOINT_ID))
            if key == 'sample_pickup':
                component = actor.get_component_by_class(u.AZ_Inv_CommonUI_ItemComponent)
                require(component is not None, 'Sample class lacks canonical item component.')
                set_pickup_id(component, u.GuidLibrary.new_guid())
        require(actor.get_class() == cls, 'Owned fixture class changed; inspect rather than replace.')
        if key in TARGET_IDS:
            nav = actor.get_component_by_class(u.AZ_NavigationTargetComponent)
            require(nav is not None and str(nav.get_editor_property('TargetId')) == TARGET_IDS[key]
                    and str(nav.get_editor_property('MapId')) == MAP_ID and str(nav.get_editor_property('LayerId')) == LAYER_ID,
                    'Existing fixture identity/context differs; preserve it and inspect.')
        if key == 'checkpoint':
            require(str(actor.get_editor_property('CheckpointId')) == CHECKPOINT_ID, 'Existing checkpoint ID differs.')
        location = actor.get_actor_location()
        result.append({'key': key, 'actor': actor.get_path_name(), 'created': created,
                       'actual_location': [location.x, location.y, location.z], 'planned_location': row['position'],
                       'existing_transform_preserved': not created})
    ids = inspect_pickup_ids()
    require(not ids['duplicates'], 'GUID duplicate after placement; inspect before saving.')
    return write('examples-placed', {'backup': backup, 'plan': plan, 'actors': result, 'pickup_audit': ids,
                                    'saved': False, 'gameplay_verified': False, 'next': 'Explicitly save map/actor packages. User performs Play checks.'})
