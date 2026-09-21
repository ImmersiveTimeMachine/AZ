# @Description: Create isolated, generic CommonUI actions and scoped Quick/Map mapping contexts.
"""Root-owned asset preparation only; no host assignment, gameplay remaps or PIE.
Use prepare(max_actions=5) until complete, then prepare_contexts(), verify(), save.
"""
import importlib.util
import json
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
SPEC = ROOT/'Saved/FieldNotesImplementation/UIInputNativeProposal/mapping-requirements.json'
DEST = '/Game/AZ/Blueprints/Input/FieldNotes'
META = DEST + '/DA_FN_UICommandMetadata'
OWNER_KEY = 'AZ.FieldNotes.UICommandOwner'
OWNER = 'field_notes_ui_commands:v2'
LABELS = {'FocusLeft': 'Focus left', 'FocusRight': 'Focus right', 'FocusUp': 'Focus up', 'FocusDown': 'Focus down',
          'Activate': 'Select', 'Assignment': 'Assign', 'PreviousCandidate': 'Previous item',
          'NextCandidate': 'Next item', 'Cancel': 'Cancel', 'ZoomIn': 'Zoom in', 'ZoomOut': 'Zoom out',
          'Recenter': 'Recenter', 'PlaceWaypoint': 'Place marker', 'ClearWaypoint': 'Clear marker',
          'TrackSelection': 'Track task'}


def support():
    spec = importlib.util.spec_from_file_location('fn_commands_styles', ROOT/'Tools/field_notes_styles_setup.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module


def records():
    data = json.loads(SPEC.read_text(encoding='utf-8'))
    assert data['version'] >= 2, 'Use reviewed non-consuming Map contract'
    assert data['map']['action_properties']['bConsumeInput'] is False
    result = [(group, entry) for group in ('quick', 'map') for entry in data[group]['actions']]
    assert len(result) == 15 and len({r['proposed_asset'] for _, r in result}) == 15
    assert all(r['proposed_asset'].startswith(DEST+'/IA_FN_') for _, r in result)
    return data, result


def owned(path, cls):
    s = support(); u = s.ue()
    obj = u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
    if obj:
        assert isinstance(obj, cls) and u.EditorAssetLibrary.get_metadata_tag(obj, OWNER_KEY) == OWNER, 'Unowned path collision: '+path
    return obj


def prepare(max_actions=5):
    s = support(); u = s.ue(); s.idle(); data, entries = records()
    out = s.OUT/'Module7/CommandAssets'; out.mkdir(parents=True, exist_ok=True)
    # Preflight every destination before creating anything.
    for _, row in entries: owned(row['proposed_asset'], u.InputAction)
    metadata = owned(META, u.CommonMappingContextMetadata)
    if not metadata:
        factory = u.DataAssetFactory(); factory.set_editor_property('data_asset_class', u.CommonMappingContextMetadata)
        metadata = u.AssetToolsHelpers.get_asset_tools().create_asset(META.rsplit('/',1)[1], DEST, u.CommonMappingContextMetadata, factory)
        assert metadata
        u.EditorAssetLibrary.set_metadata_tag(metadata, OWNER_KEY, OWNER)
        generic = u.new_object(u.CommonInputMetadata, outer=metadata, name='GenericUICommand')
        generic.set_editor_property('is_generic_input_action', True)
        metadata.set_editor_property('enhanced_input_metadata', generic)
    assert metadata.get_editor_property('enhanced_input_metadata').get_editor_property('is_generic_input_action')
    made = []
    for group, row in entries:
        path = row['proposed_asset']
        if owned(path, u.InputAction): continue
        if len(made) >= max_actions: break
        action = u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1], DEST, u.InputAction, None)
        assert action
        u.EditorAssetLibrary.set_metadata_tag(action, OWNER_KEY, OWNER)
        short = path.rsplit('_',1)[1]; label = LABELS[short]
        for key, value in {'value_type': u.InputActionValueType.BOOLEAN, 'consume_input': group == 'quick',
                           'consumes_action_and_axis_mappings': False, 'trigger_when_paused': True,
                           'triggers': [], 'modifiers': [], 'action_description': u.Text(label)}.items():
            action.set_editor_property(key, value)
        settings = u.new_object(u.PlayerMappableKeySettings, outer=action, name='UICommandKeySettings')
        settings.set_editor_property('name', u.Name('AZ.UI.'+group+'.'+short))
        settings.set_editor_property('display_name', u.Text(label))
        settings.set_editor_property('display_category', u.Text('Quick Select' if group == 'quick' else 'Map'))
        settings.set_editor_property('metadata', metadata)
        action.set_editor_property('player_mappable_key_settings', settings)
        made.append(path)
    pending = [r['proposed_asset'] for _, r in entries if not u.EditorAssetLibrary.does_asset_exist(r['proposed_asset'])]
    return s.write(out/'prepared.json', {'created': made, 'pending': pending, 'spec_sha256': s.digest(SPEC),
                                        'host_assignments': [], 'complete': not pending})


def prepare_contexts():
    s = support(); u = s.ue(); s.idle(); data, entries = records()
    assert all(owned(r['proposed_asset'], u.InputAction) for _, r in entries), 'Create all actions first'
    result = []
    for group in ('quick','map'):
        path = data[group]['context']; assert path.startswith(DEST+'/IMC_FN_')
        context = owned(path, u.InputMappingContext)
        if not context:
            context = u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1], DEST, u.InputMappingContext, None)
            assert context
            u.EditorAssetLibrary.set_metadata_tag(context, OWNER_KEY, OWNER)
        assert not context.get_editor_property('mapping_profile_overrides'), 'Unexpected authored profile override'
        before = context.get_editor_property('default_key_mappings')
        old = list(before.get_editor_property('mappings'))
        assert not old, 'Context already configured; run verification instead of resetting it'
        mappings = []
        for row in data[group]['actions']:
            binding = u.EnhancedActionKeyMapping(); key = u.Key(); assert key.import_text(row['recommended_pad_key'])
            binding.set_editor_property('action', u.load_asset(row['proposed_asset']))
            binding.set_editor_property('key', key)
            binding.set_editor_property('triggers', []); binding.set_editor_property('modifiers', [])
            mappings.append(binding)
        copied = before.copy(); copied.set_editor_property('mappings', mappings)
        context.set_editor_property('default_key_mappings', copied)
        result.append(path)
    return {'contexts': result, 'host_assignments': [], 'save_required': True}


def verify():
    s = support(); u = s.ue(); data, entries = records(); metadata = owned(META, u.CommonMappingContextMetadata)
    assert metadata and metadata.get_editor_property('enhanced_input_metadata').get_editor_property('is_generic_input_action')
    paths = [META]; names = set()
    for group, row in entries:
        action = owned(row['proposed_asset'], u.InputAction); assert action
        assert action.get_editor_property('value_type') == u.InputActionValueType.BOOLEAN
        assert action.get_editor_property('consume_input') == (group == 'quick')
        assert not action.get_editor_property('consumes_action_and_axis_mappings')
        assert not action.get_editor_property('triggers') and not action.get_editor_property('modifiers')
        settings = action.get_editor_property('player_mappable_key_settings'); assert settings
        assert settings.get_editor_property('metadata') == metadata
        name = str(settings.get_editor_property('name')); assert name not in names; names.add(name)
        paths.append(row['proposed_asset'])
    for group in ('quick','map'):
        path = data[group]['context']; context = owned(path, u.InputMappingContext); assert context
        assert not context.get_editor_property('mapping_profile_overrides')
        rows = list(context.get_editor_property('default_key_mappings').get_editor_property('mappings'))
        assert len(rows) == len(data[group]['actions'])
        for actual, expected in zip(rows, data[group]['actions']):
            assert actual.get_editor_property('action').get_path_name().split('.')[0] == expected['proposed_asset']
            assert str(actual.get_editor_property('key').export_text()) == expected['recommended_pad_key']
            assert not actual.get_editor_property('triggers') and not actual.get_editor_property('modifiers')
        paths.append(path)
    return s.write(s.OUT/'Module7/CommandAssets/verified.json', {'assets': paths, 'actions': 15, 'contexts': 2,
                    'generic_metadata_verified': True, 'map_non_consuming_verified': True,
                    'host_assignments': [], 'runtime_verified': False})
