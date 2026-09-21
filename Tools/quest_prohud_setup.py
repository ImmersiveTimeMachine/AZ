# @Description: Stage isolated CHALK quest widgets and exact ProHUD helper dependencies.
"""Import is inert, including outside Unreal. No asset compilation/save or PIE.

Offline: python Tools/quest_prohud_setup.py --analyze
Editor stages (root invokes): preflight(), prepare(), remap_references(),
module_fields(); return to the native compiler, then record_native_compile().
These stages intentionally do NOT attach a HUD, prune helpers, rewrite progress,
or bind runtime delegates. Receipt status never claims that context is adapted.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import re
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/QuestPlanning/ProHUDAuthoring'
DEST = '/Game/AZ/Blueprints/Menu/HUD/Navigation/Quests'
SOURCE = '/Game/ProHUDV2_Horror'
CONTAINER = SOURCE + '/Widgets/Content/WB_HUD_Container_H'
MODULE = DEST + '/WBP_AZ_QuestModule'
OWNER_KEY, OWNER = 'AZ.Quest.AuthoringOwner', 'quest_prohud_setup:v1'
STAGE_KEY = 'AZ.Quest.PreparationStage'
COPIES = {
    SOURCE + '/Widgets/Content/MissionNotification/WB_MissionNotification_H': DEST + '/WBP_AZ_MissionNotification',
    SOURCE + '/Widgets/Content/MissionNotification/WB_Mission_H': DEST + '/WBP_AZ_Mission',
    SOURCE + '/Widgets/Content/MissionNotification/WB_Task_H': DEST + '/WBP_AZ_Task',
}
LIBRARIES = {
    'settings': (SOURCE + '/Blueprints/Libraries/BP_PHV2_Functions_H', DEST + '/BFL_AZ_QuestSettings'),
    'widgets': (SOURCE + '/Blueprints/Libraries/BP_PHV2_Widget_Functions_H', DEST + '/BFL_AZ_QuestWidgets'),
}
ALL_COPIES = {**COPIES, **{source: target for source, target in LIBRARIES.values()}}
INPUTS = {
    'tasks': ROOT / 'Saved/QuestPlanning/vendor-task-graphs.json',
    'helpers': ROOT / 'Saved/CompassIntegration/helper-closure-source.json',
    'facade': ROOT / 'Saved/QuestPlanning/vendor-mission-facade.json',
    'context': ROOT / 'Saved/QuestPlanning/quest-context-details.json',
    'icon': ROOT / 'Saved/QuestPlanning/quest-icon-and-extra-defaults.json',
}
BOUNDARY = ('settings', 'GetHUD_Container_H')
CONTAINER_SEEDS = ('GetMissionNotiBaseInfo', 'GetMissionTasksBaseInfo', 'FindFontInfo')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def ue():
    import unreal
    return unreal


def normalized(value):
    return re.sub(r'[^a-z0-9]', '', value.lower())


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / (name + '.json')).write_text(json.dumps(value, indent=2), encoding='utf-8')
    return value


def tool(name, **kwargs):
    group, function = name.rsplit('.', 1)
    response = ue().ToolsetRegistry.execute_tool(group, function, json.dumps(kwargs))
    require(response.is_complete and not response.error, name + ': ' + str(response.error))
    return json.loads(response.value)['returnValue']


def idle():
    require(not tool('EditorToolset.EditorAppToolset.IsPIERunning'),
            'PIE is running; wait for the user to stop it. No editor testing is authorized by this script.')


def package_file(package):
    require(package.startswith('/Game/'), 'Not a project package: ' + package)
    result = (ROOT / 'Content' / (package[6:] + '.uasset')).resolve()
    require(result.is_relative_to((ROOT / 'Content').resolve()), 'Invalid package path')
    return result


def digest_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(package):
    asset = ue().load_asset(package)
    require(asset is not None, 'Missing asset: ' + package)
    return asset


def owned(package):
    asset = load(package)
    require(ue().EditorAssetLibrary.get_metadata_tag(asset, OWNER_KEY) == OWNER,
            'Refusing unowned destination: ' + package)
    require(not isinstance(asset, ue().AnimBlueprint), 'Animation assets are out of scope')
    return asset


def node_blocks(lines):
    current = None
    for line in lines:
        header = re.match(r'^\s*\[([A-F0-9]+)\] (\S+) \| (.*)$', line)
        if header:
            if current:
                yield current
            current = {'guid': header[1], 'kind': header[2], 'title': header[3], 'lines': []}
        elif current:
            current['lines'].append(line)
    if current:
        yield current


def analyze_sources():
    """File-only closure with owner-qualified calls; never match leaf functions by title alone."""
    data = {key: json.loads(path.read_text(encoding='utf-8')) for key, path in INPUTS.items()}
    libraries = {key: dict(data['helpers'][key]['graphs']) for key in LIBRARIES}
    # Prefer the newer facade export where the same library function exists.
    for key, (source, _) in LIBRARIES.items():
        libraries[key].update(data['facade'].get(source, {}).get('graphs', {}))
    indices = {}
    for key, graphs in libraries.items():
        index = {}
        for name in graphs:
            index.setdefault(normalized(name), []).append(name)
        indices[key] = index
    owner_groups = {source.rsplit('/', 1)[1] + '_C': key for key, (source, _) in LIBRARIES.items()}
    dependencies, local_calls, unresolved = [], [], []

    def find_calls(lines, source_asset, source_graph, current_group=None):
        found = set()
        for node in node_blocks(lines):
            if node['kind'] != 'CallFunction':
                continue
            raw = node['title']
            caption, _, target_label = raw.partition(' Target is ')
            owner = None
            connected_self = False
            for line in node['lines']:
                match = re.search(r'in\s+self:object = Default__(\w+)', line)
                if match:
                    owner = match[1]
                connected_self |= bool(re.search(r'in\s+self:object\s*<-', line))
            group = owner_groups.get(owner)
            # Implicit self is valid only while traversing a known library body.
            # A leaf's FindFontInfo/SetSize/etc stays local even if a library has
            # the same display title. Explicit native/connected targets also win.
            if not group and not owner and not target_label and not connected_self and current_group:
                if normalized(caption) in indices[current_group]:
                    group = current_group
            evidence = {'asset': source_asset, 'graph': source_graph, 'node': node['guid'], 'title': raw}
            if group:
                matches = indices[group].get(normalized(caption), [])
                if len(matches) != 1:
                    unresolved.append({**evidence, 'owner': group, 'reason': 'Missing/ambiguous library function body'})
                else:
                    name = matches[0]
                    found.add((group, name))
                    dependencies.append({**evidence, 'library': group, 'function': name, 'owner_evidence': owner or 'implicit library self'})
            elif not owner and not connected_self and not target_label:
                local_calls.append(evidence)
        return found

    pending = {('settings', 'GetMissionNotificationInfo'), ('settings', 'GetMissionTasksInfo')}
    for source in COPIES:
        require(source in data['tasks'], 'Missing source widget receipt: ' + source)
        for name, lines in data['tasks'][source]['graphs'].items():
            pending.update(find_calls(lines, source, name))
    container_graphs = dict(data['facade'].get(CONTAINER, {}).get('graphs', {}))
    if 'FindFontInfo' in data['context']:
        container_graphs['FindFontInfo'] = data['context']['FindFontInfo']
    for name in CONTAINER_SEEDS:
        require(name in container_graphs, 'Missing source mission config graph: ' + name)
        pending.update(find_calls(container_graphs[name], CONTAINER, name))
    keep = set()
    while pending:
        group, name = sorted(pending)[0]
        pending.remove((group, name))
        if (group, name) in keep:
            continue
        keep.add((group, name))
        if (group, name) == BOUNDARY:
            continue  # Replace this global discovery boundary with QuestContext.
        require(name in libraries[group], 'Missing required helper body: ' + group + '/' + name)
        pending.update(find_calls(libraries[group][name], LIBRARIES[group][0], name, group) - keep)

    # WB_Icon_H is a shared visual leaf, not automatically another owned copy.
    # Prove that its helper closure has no global settings/player lookup first.
    icon_source = SOURCE + '/Widgets/Base/WB_Icon_H'
    icon_pending = set()
    for name, lines in data['icon']['graphs'].items():
        icon_pending.update(find_calls(lines, icon_source, name))
    icon_keep = set()
    while icon_pending:
        group, name = sorted(icon_pending)[0]
        icon_pending.remove((group, name))
        if (group, name) in icon_keep:
            continue
        icon_keep.add((group, name))
        require(group == 'widgets', 'Icon has a settings/global dependency; include an owned Icon copy before authoring.')
        lines = libraries[group][name]
        require(not any(re.search(r'Get Player|Get All Widgets|Get HUD|Get Game Instance', line) for line in lines),
                'Icon helper requires external context: ' + name)
        icon_pending.update(find_calls(lines, LIBRARIES[group][0], name, group) - icon_keep)

    fields = {'MissionNotificationInfo', 'MissionTasksInfo'}
    for group, name in keep:
        if group == 'settings' and (group, name) != BOUNDARY:
            for node in node_blocks(libraries[group][name]):
                if node['kind'] == 'VariableGet' and node['title'].startswith('Get '):
                    fields.add(node['title'][4:])
    for name in CONTAINER_SEEDS:
        for node in node_blocks(container_graphs[name]):
            if node['kind'] == 'VariableGet' and node['title'].startswith('Get '):
                fields.add(node['title'][4:])

    # Source container's FindFontInfo is local, not the leaf's same-title helper.
    missing_container = sorted({call['title'] for call in local_calls
                                if call['asset'] == CONTAINER
                                and normalized(call['title']) not in {normalized(name) for name in container_graphs}})
    def unique(rows):
        return [json.loads(item) for item in sorted({json.dumps(row, sort_keys=True) for row in rows})]
    embedded = sorted({row.get('widgetClassPath', {}).get('refPath', '')
                       for row in data['context'].get('task_tree', {}).get('widgets', [])
                       if row.get('widgetClassPath', {}).get('refPath', '').startswith(SOURCE)})
    available_defaults = {normalized(name) for name in data['context'].get('defaults', {})}
    available_defaults.update(normalized(name) for name in data['icon'].get('missing_defaults', {}))
    return write('source-analysis', {
        'input_hashes': {str(path): digest_file(path) for path in INPUTS.values()},
        'helper_closure': {key: sorted(name for group, name in keep if group == key) for key in LIBRARIES},
        'global_boundary_to_replace': {'library': BOUNDARY[0], 'function': BOUNDARY[1]},
        'helper_calls': unique(dependencies), 'unresolved_library_calls': unique(unresolved),
        'local_calls_not_retargeted_by_title': unique(local_calls),
        'module_source_fields': sorted(fields),
        'container_graphs_needed': list(CONTAINER_SEEDS),
        'missing_container_local_bodies': missing_container,
        'embedded_vendor_widgets': embedded,
        'shared_icon': {'path': icon_source, 'parent': data['icon']['tree']['info']['parentClass'],
                        'helpers': sorted(name for _, name in icon_keep),
                        'settings_or_player_lookup': False,
                        'decision': 'Keep unmodified shared visual leaf: all inputs are explicit widget/brush/size/color values.'},
        'missing_source_default_fields': [field for field in sorted(fields) if normalized(field) not in available_defaults],
        'pending_live_evidence': ['Exact live source field type preflight is performed by module_fields()',
                                  'Local optional-only function default; RemoveAll collection snapshot semantics'],
        'scope': 'Three owned mission widgets/two helper libraries; verified explicit-input shared WB_Icon_H remains unchanged.',
        'runtime_ready': False,
    })


def preflight():
    idle()
    analysis = analyze_sources()
    require(not analysis['unresolved_library_calls'], 'Missing helper bodies; inspect source-analysis receipt before preparing copies.')
    dirty = {p.get_name() for p in ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    require(not (dirty & set(ALL_COPIES)), 'Source assets have unsaved changes: ' + str(dirty & set(ALL_COPIES)))
    for source, destination in ALL_COPIES.items():
        asset = load(source)
        require(not isinstance(asset, ue().AnimBlueprint), 'Unexpected animation source')
        require(asset.generated_class() is not None, 'Source has no generated class: ' + source)
        if ue().EditorAssetLibrary.does_asset_exist(destination):
            owned(destination)
    if ue().EditorAssetLibrary.does_asset_exist(MODULE):
        owned(MODULE)
    hashes = {source: digest_file(package_file(source)) for source in ALL_COPIES}
    baseline = OUT / 'copy-baseline.json'
    if baseline.exists():
        require(json.loads(baseline.read_text(encoding='utf-8'))['source_hashes'] == hashes,
                'Source packages changed since this preparation began; inspect before continuing.')
    else:
        write('copy-baseline', {'source_hashes': hashes, 'copies': ALL_COPIES})
    return write('preflight', {'copies': ALL_COPIES, 'module': MODULE,
                              'source_hashes': hashes,
                              'helper_closure': analysis['helper_closure'],
                              'context_adaptation_pending': True, 'runtime_ready': False})


def prepare():
    """Only independent duplicates + empty context widget. No active tree changes."""
    before = preflight()
    created, retained = [], []
    for source, destination in ALL_COPIES.items():
        if ue().EditorAssetLibrary.does_asset_exist(destination):
            owned(destination)
            retained.append(destination)
            continue
        asset = ue().EditorAssetLibrary.duplicate_asset(source, destination)
        require(asset is not None, 'Duplication failed: ' + destination)
        ue().EditorAssetLibrary.set_metadata_tag(asset, OWNER_KEY, OWNER)
        ue().EditorAssetLibrary.set_metadata_tag(asset, STAGE_KEY, 'copied')
        created.append(destination)
    if not ue().EditorAssetLibrary.does_asset_exist(MODULE):
        tool('UMGToolSet.UMGToolSet.CreateWidgetBlueprint', folderPath=DEST,
             assetName=MODULE.rsplit('/', 1)[1], parentClass={'refPath': '/Script/UMG.UserWidget'})
        asset = load(MODULE)
        ue().EditorAssetLibrary.set_metadata_tag(asset, OWNER_KEY, OWNER)
        ue().EditorAssetLibrary.set_metadata_tag(asset, STAGE_KEY, 'empty-context')
        created.append(MODULE)
    else:
        owned(MODULE)
        retained.append(MODULE)
    require(all(digest_file(package_file(source)) == value for source, value in before['source_hashes'].items()),
            'Source package changed during duplication')
    return write('prepared', {'created': created, 'retained': retained,
                             'native_compile_required': list(ALL_COPIES.values()) + [MODULE],
                             'next': 'Run remap_references before pruning helper graphs; native compile after scripts return.',
                             'saved_by_this_script': False, 'runtime_ready': False})


def remap_references():
    """Preserve class/graph/CDO references with the already-built scoped native helper."""
    idle()
    targets = list(ALL_COPIES.values())
    assets = [owned(path) for path in targets]
    if all(ue().EditorAssetLibrary.get_metadata_tag(asset, STAGE_KEY) == 'remapped' for asset in assets):
        return {'state': 'already remapped', 'runtime_ready': False}
    require(all(ue().EditorAssetLibrary.get_metadata_tag(asset, STAGE_KEY) == 'copied' for asset in assets),
            'Partial remap/advanced stage; inspect and resume explicitly rather than rerunning destructive preparation.')
    helper = getattr(ue(), 'AZ_BlueprintNodeUtils', None)
    require(helper is not None and hasattr(helper, 'remap_copied_blueprint_references'), 'Loaded native remap helper unavailable')
    for asset in assets:
        ue().EditorAssetLibrary.set_metadata_tag(asset, STAGE_KEY, 'remapping')
    result = list(helper.remap_copied_blueprint_references(targets, list(ALL_COPIES), targets))
    write('remap-result', {'result': [str(item) for item in result], 'runtime_ready': False})
    require(result and not any(str(item).startswith('ERROR') for item in result), 'Native remap preflight failed; inspect remap-result')
    for asset in assets:
        ue().EditorAssetLibrary.set_metadata_tag(asset, STAGE_KEY, 'remapped')
    return {'state': 'remapped; native compile and context adaptation required', 'result': [str(item) for item in result]}


def module_fields():
    """Add exact source property types and QuestContext references; do not guess defaults."""
    idle()
    analysis = analyze_sources()
    library = ue().BlueprintEditorLibrary
    source, module = load(CONTAINER), owned(MODULE)
    require(module.generated_class() is not None, 'Native compile the empty context widget first')
    names = {str(name) for name in library.list_member_variable_names(source, False)}
    fields = analysis['module_source_fields']
    require(set(fields) <= names, 'Unknown source fields: ' + str(sorted(set(fields) - names)))
    # Preflight every source type before adding any field.
    types = {name: library.get_member_variable_type(source, name) for name in fields}
    require(all(value is not None for value in types.values()), 'A source field type is unavailable')
    current = {str(name) for name in library.list_member_variable_names(module, False)}
    for name in set(fields) & current:
        actual = library.get_member_variable_type(module, name)
        require(actual is not None, 'Existing setting has no reflected type: ' + name)
        source_schema = json.loads(library.pin_type_to_json_schema(types[name], source.generated_class()))
        target_schema = json.loads(library.pin_type_to_json_schema(actual, module.generated_class()))
        require(source_schema == target_schema, 'Existing module field type differs from source: ' + name)
    added = []
    for name, pin_type in types.items():
        if name not in current:
            require(library.add_member_variable(module, name, pin_type), 'Cannot add typed setting: ' + name)
            library.set_blueprint_variable_category(module, name, 'Quest|Source Settings')
            added.append(name)
    context_type = library.get_object_reference_type(module.generated_class())
    for path in COPIES.values():
        asset = owned(path)
        if 'QuestContext' not in {str(name) for name in library.list_member_variable_names(asset, False)}:
            require(library.add_member_variable(asset, 'QuestContext', context_type), 'Cannot add QuestContext to ' + path)
            library.set_blueprint_variable_category(asset, 'QuestContext', 'Quest|Context')
            library.set_blueprint_variable_expose_on_spawn(asset, 'QuestContext', True)
            library.set_blueprint_variable_instance_editable(asset, 'QuestContext', True)
    return write('module-fields', {'module': MODULE, 'source': CONTAINER, 'fields': fields, 'added': added,
                                  'defaults_copied': False, 'runtime_ready': False,
                                  'next': 'Native compile module and three owned leaves, then inspect source defaults and adapt helpers.'})


def support():
    """Reuse only read/connection helpers; importing the existing adapter is inert."""
    global _SUPPORT
    if '_SUPPORT' not in globals():
        spec = importlib.util.spec_from_file_location('quest_compass_graph_support', ROOT / 'Tools/compass_graph_adapter.py')
        _SUPPORT = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(_SUPPORT)
    return _SUPPORT


def object_path(package):
    return package + '.' + package.rsplit('/', 1)[1]


def widget_rows(package):
    data = tool('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint={'refPath': object_path(package)})
    return {row['widgetName']: row for row in data['widgets']}


def object_properties(reference, fields=None):
    prefix = 'editor_toolset.toolsets.object.ObjectTools.'
    schema = json.loads(tool(prefix + 'list_properties', instance=reference))
    names = list(schema) if fields is None else fields
    return json.loads(tool(prefix + 'get_properties', instance=reference, properties=names))


def replace_embedded_widgets():
    """Archive remapping cannot change the class of an existing WidgetTree instance."""
    idle()
    specs = ((DEST + '/WBP_AZ_Mission', DEST + '/WBP_AZ_Task',
              ('Task_Empty', 'Task_Checked', 'Task_Unchecked', 'Task_Active')),
             (DEST + '/WBP_AZ_MissionNotification', DEST + '/WBP_AZ_Mission', ('WB_Mission',)))
    report = []
    for package, child, names in specs:
        owned(package)
        replacement = owned(child).generated_class()
        require(replacement is not None, 'Native compile replacement class first: ' + child)
        for name in names:
            rows = widget_rows(package)
            require(name in rows, 'Missing expected embedded child: ' + package + ':' + name)
            row = rows[name]
            current = row['widgetClassPath']['refPath'].split('.')[0]
            if current == child:
                report.append({'package': package, 'widget': name, 'state': 'already owned'})
                continue
            expected = next(source for source, target in COPIES.items() if target == child)
            require(current == expected, 'Unexpected child class; preserve user edits: ' + current)
            before_slot = object_properties(row['slot']) if isinstance(row.get('slot'), dict) else None
            result = tool('UMGToolSet.UMGToolSet.ReplaceWidgetWithTemplate',
                          widgetBlueprint={'refPath': object_path(package)}, widgetToReplace=row['widget'],
                          templateClass={'refPath': replacement.get_path_name()})
            report.append({'package': package, 'widget': name, 'report': result})
            write('embedded-widget-progress', report)
            require(result.get('bSuccess') and not result.get('unmatchedReferencedProperties')
                    and not result.get('unmatchedReferencedFunctions') and not result.get('missingReferencesWarning'),
                    'Embedded replacement has referenced incompatibilities; inspect progress receipt')
            after_rows = widget_rows(package)
            require(set(after_rows) == set(rows), 'Replacement changed widget names/tree membership')
            after = after_rows[name]
            require(after['widgetClassPath']['refPath'].split('.')[0] == child, 'Replacement class readback failed')
            require(after.get('parent') == row.get('parent') and after.get('bIsVariable') == row.get('bIsVariable'),
                    'Replacement changed parent or variable contract')
            after_slot = object_properties(after['slot']) if isinstance(after.get('slot'), dict) else None
            require(before_slot == after_slot, 'Replacement changed slot properties')
    return write('embedded-widgets', {'widgets': report, 'native_compile_required': True, 'runtime_ready': False})


def module_defaults():
    """Copy source settings once; never overwrite later CHALK styling on rerun."""
    idle()
    module = owned(MODULE)
    key = 'AZ.Quest.SourceDefaults'
    if ue().EditorAssetLibrary.get_metadata_tag(module, key) == 'v1':
        return {'state': 'source defaults already copied; preserve subsequent styling'}
    fields = analyze_sources()['module_source_fields']
    source = ue().get_default_object(load(CONTAINER).generated_class())
    target = ue().get_default_object(module.generated_class())
    values = {name: source.get_editor_property(name) for name in fields}
    # Read every target field before the first write, catching uncompiled fields.
    for name in fields:
        target.get_editor_property(name)
    for name, value in values.items():
        target.set_editor_property(name, value)
    snapshots = []
    for obj in (source, target):
        data = object_properties({'refPath': obj.get_path_name()})
        normalized_fields = {normalized(name): value for name, value in data.items()}
        snapshots.append({name: normalized_fields[normalized(name)] for name in fields})
    require(snapshots[0] == snapshots[1], 'Source/module serialized defaults differ')
    ue().EditorAssetLibrary.set_metadata_tag(module, key, 'v1')
    return write('module-defaults', {'values': snapshots[1], 'saved': False,
                                   'next': 'Native compile after Python returns, then explicit save/readback.', 'runtime_ready': False})


def prepare_helper_signatures():
    """Prune only top-level library functions, then add explicit QuestContext."""
    idle()
    A = support()
    closure = analyze_sources()['helper_closure']
    context_type = A.BL.get_object_reference_type(owned(MODULE).generated_class())
    report = {}
    for group, (_, package) in LIBRARIES.items():
        bp = owned(package)
        key = 'AZ.Quest.HelperSignatures'
        stage = ue().EditorAssetLibrary.get_metadata_tag(bp, key)
        if stage == 'v1':
            report[group] = 'already prepared'
            continue
        require(not stage, 'Interrupted helper signature stage; inspect before retry: ' + package)
        require(ue().EditorAssetLibrary.get_metadata_tag(bp, STAGE_KEY) == 'remapped', 'Remap before pruning library graphs')
        entries = {}
        top_level = []
        for graph in A.BL.list_graphs(bp):
            if graph.get_outer() != bp:
                continue
            editor = A.GE.get_graph_editor(graph)
            nodes = [node for node in editor.list_all_nodes() if node.get_class().get_name() == 'K2Node_FunctionEntry']
            if nodes:
                top_level.append(graph.get_name())
                require(len(nodes) == 1, 'Ambiguous library function entry')
                entries[graph.get_name()] = (editor, nodes[0])
        require(set(closure[group]) <= set(entries), 'Required source library graph is absent')
        ue().EditorAssetLibrary.set_metadata_tag(bp, key, 'preparing')
        for name in top_level:
            if name not in closure[group]:
                A.BL.remove_function_graph(bp, name)
        for name in closure[group]:
            editor, entry = entries[name]
            if not any(str(A.PL.get_pin_name(pin)) == 'QuestContext' for pin in A.BL.list_output_pins(entry)):
                require(editor.add_graph_input_parameter('QuestContext', context_type).is_valid(), 'Cannot add QuestContext: ' + name)
        ue().EditorAssetLibrary.set_metadata_tag(bp, key, 'v1')
        report[group] = {'retained': closure[group], 'removed': sorted(set(top_level) - set(closure[group]))}
    return write('helper-signatures', {'libraries': report, 'native_compile_required': True, 'runtime_ready': False})


def prepare_module_signatures():
    idle()
    A = support()
    source, module = load(CONTAINER), owned(MODULE)
    report = {}
    for name in CONTAINER_SEEDS:
        key = 'AZ.Quest.Signature.' + name
        stage = ue().EditorAssetLibrary.get_metadata_tag(module, key)
        if stage == 'v1':
            report[name] = 'already prepared'
            continue
        require(not stage and A.BL.find_graph(module, name) is None, 'Unexpected/partial module function: ' + name)
        _, original = A.graph_nodes(source, name)
        entry = next(node for node in original.list_all_nodes() if node.get_class().get_name() == 'K2Node_FunctionEntry')
        results = [node for node in original.list_all_nodes() if node.get_class().get_name() == 'K2Node_FunctionResult']
        require(len(results) == 1, 'Unexpected source return count: ' + name)
        inputs = [(str(A.PL.get_pin_name(pin)), A.PL.get_pin_type(pin)) for pin in A.BL.list_output_pins(entry)
                  if str(A.PL.get_pin_name(pin)) not in ('then', '__WorldContext')]
        outputs = [(str(A.PL.get_pin_name(pin)), A.PL.get_pin_type(pin)) for pin in A.BL.list_input_pins(results[0])
                   if str(A.PL.get_pin_name(pin)) != 'execute']
        ue().EditorAssetLibrary.set_metadata_tag(module, key, 'preparing')
        editor = A.GE.create_and_edit_function_graph(module, name)
        editor.set_function_is_public()
        for pin_name, pin_type in inputs:
            require(editor.add_graph_input_parameter(pin_name, pin_type).is_valid(), 'Cannot copy exact function input type')
        for pin_name, pin_type in outputs:
            require(editor.add_graph_output_parameter(pin_name, pin_type) is not None, 'Cannot copy exact function output type')
        editor.set_is_pure_function(True)
        ue().EditorAssetLibrary.set_metadata_tag(module, key, 'v1')
        report[name] = {'inputs': [name for name, _ in inputs], 'outputs': [name for name, _ in outputs]}
    return write('module-signatures', {'functions': report, 'native_compile_required': True, 'runtime_ready': False})


def _helper_signature_matches(node, group, function):
    """Compare exposed parameter names with the actual owned function signature."""
    A = support()
    _, editor = A.graph_nodes(owned(LIBRARIES[group][1]), function)
    nodes = list(editor.list_all_nodes())
    entry = next(item for item in nodes if item.get_class().get_name() == 'K2Node_FunctionEntry')
    returns = [item for item in nodes if item.get_class().get_name() == 'K2Node_FunctionResult']
    input_names = {str(A.PL.get_pin_name(pin)) for pin in A.BL.list_input_pins(node)}
    output_names = {str(A.PL.get_pin_name(pin)) for pin in A.BL.list_output_pins(node)}
    def present(name, names):
        return name in names or any(candidate.startswith(name + '_') for candidate in names)
    expected_inputs = {str(A.PL.get_pin_name(pin)) for pin in A.BL.list_output_pins(entry)} - {'then', '__WorldContext'}
    expected_outputs = {str(A.PL.get_pin_name(pin)) for result in returns for pin in A.BL.list_input_pins(result)} - {'execute'}
    return all(present(name, input_names) for name in expected_inputs) and all(present(name, output_names) for name in expected_outputs)


def _typed_context_pin(node):
    A = support()
    pins = [pin for pin in A.BL.list_input_pins(node) if str(A.PL.get_pin_name(pin)) == 'QuestContext']
    if not pins:
        return None
    require(len(pins) == 1, 'Ambiguous QuestContext pin')
    type_text = str(A.PL.get_pin_type_as_json_schema(pins[0])) + str(A.PL.get_pin_type_display_string(pins[0]))
    require(normalized(MODULE.rsplit('/', 1)[1]) in normalized(type_text), 'QuestContext has an unexpected class: ' + type_text)
    return pins[0]


def _helper_call(node, current_group, closure):
    """Use exposed context type/signature when Blueprint APIs hide static self pins."""
    A = support()
    caption = normalized(A.title(node))
    matches = [(key, name) for key, names in closure.items() for name in names if normalized(name) == caption]
    context_pin = _typed_context_pin(node)
    if context_pin is not None:
        # Local GetSize/FindFontInfo methods have no QuestContext parameter. A
        # matching title alone never qualifies; require the owned module type
        # plus a unique, compatible actual library function signature.
        require(len(matches) == 1, 'Typed context call is not a unique audited helper: ' + A.title(node))
        require(_helper_signature_matches(node, *matches[0]), 'Helper call signature differs from its owned definition: ' + A.title(node))
        return matches[0]
    self_pins = [pin for pin in A.BL.list_input_pins(node) if str(A.PL.get_pin_name(pin)) == 'self']
    group = None
    if self_pins:
        schema = str(A.PL.get_pin_type_as_json_schema(self_pins[0]))
        display = str(A.PL.get_pin_type_display_string(self_pins[0]))
        for key, paths in LIBRARIES.items():
            if any(normalized(path.rsplit('/', 1)[1]) in normalized(schema + display) for path in paths):
                group = key
        if group is None:
            return None  # Native/instance target owns this method, regardless of its title.
    else:
        return None  # No class/context evidence: never guess by function title alone.
    if group:
        names = [name for name in closure[group] if normalized(name) == caption]
        require(len(names) == 1, 'Unresolved helper on a known library class: ' + A.title(node))
        return group, names[0]
    return None


def _quest_context(editor, parameter=False):
    A = support()
    if parameter:
        entry = next(node for node in editor.list_all_nodes() if node.get_class().get_name() == 'K2Node_FunctionEntry')
        return A.pin(entry, 'QuestContext', True)
    node = editor.add_get_member_variable_node('QuestContext')
    require(node is not None, 'QuestContext is not compiled on this widget')
    return A.pin(node, 'QuestContext', True)


def _helper_path(group, function):
    package = LIBRARIES[group][1]
    return package + '.SKEL_' + package.rsplit('/', 1)[1] + '_C:' + function


def adapt_helpers():
    idle()
    A = support()
    analysis = analyze_sources()
    closure = analysis['helper_closure']
    module_class = owned(MODULE).generated_class().get_path_name()
    report = {}
    for group, (_, package) in LIBRARIES.items():
        bp = owned(package)
        key = 'AZ.Quest.HelperContext'
        stage = ue().EditorAssetLibrary.get_metadata_tag(bp, key)
        if stage == 'v1':
            report[group] = 'already adapted'
            continue
        require(not stage and ue().EditorAssetLibrary.get_metadata_tag(bp, 'AZ.Quest.HelperSignatures') == 'v1',
                'Prepare/native-compile helper signatures first; partial authoring requires inspection')
        # Every needed signature must exist before any body is changed.
        for name in closure[group]:
            _, editor = A.graph_nodes(bp, name)
            _quest_context(editor, True)
        ue().EditorAssetLibrary.set_metadata_tag(bp, key, 'adapting')
        counts = {'module_getters': 0, 'boundary_calls': 0, 'helper_calls': 0}
        for name in closure[group]:
            if (group, name) == BOUNDARY:
                continue
            _, editor = A.graph_nodes(bp, name)
            context = _quest_context(editor, True)
            if group == 'settings':
                # Retarget source-container variable getters before removing the
                # old discovery call, so no connected HUD_Container output is lost.
                for node in list(editor.list_all_nodes()):
                    if node.get_class().get_name() != 'K2Node_VariableGet':
                        continue
                    self_pin = node.find_input_pin('self')
                    if not self_pin.is_valid():
                        continue
                    type_text = str(A.PL.get_pin_type_as_json_schema(self_pin))
                    if 'WB_HUD_Container_H' not in type_text:
                        continue
                    outputs = [pin for pin in A.BL.list_output_pins(node) if str(A.PL.get_pin_name(pin)) not in ('then', 'else')]
                    require(len(outputs) == 1, 'Unexpected source setting getter shape')
                    field = str(A.PL.get_pin_name(outputs[0]))
                    require(field in analysis['module_source_fields'], 'Unplanned source setting: ' + field)
                    new = editor.add_get_member_variable_node(field, module_class)
                    A.connect(context, A.pin(new, 'self'))
                    A.transfer_node(node, new, editor, ('self',))
                    counts['module_getters'] += 1
            for node in list(editor.list_all_nodes()):
                if node.get_class().get_name() != 'K2Node_CallFunction':
                    continue
                identity = _helper_call(node, group, closure)
                if identity is None:
                    continue
                if identity == BOUNDARY:
                    old_container = node.find_output_pin('HUD_Container')
                    require(not old_container.is_valid() or not old_container.list_connected_pins(),
                            'Source container still has consumers; inspect before removing lookup')
                    new = editor.add_call_function_node('/Script/Engine.KismetSystemLibrary:IsValid')
                    A.connect(context, A.pin(new, 'Object'))
                    A.transfer_node(node, new, editor, ('self', '__WorldContext', 'QuestContext', 'HUD_Container'))
                    counts['boundary_calls'] += 1
                else:
                    target_group, function = identity
                    new = editor.add_call_function_node(function if target_group == group else _helper_path(target_group, function))
                    require(new is not None, 'Missing compiled helper signature: ' + function)
                    A.connect(context, A.pin(new, 'QuestContext'))
                    A.transfer_node(node, new, editor, ('self', '__WorldContext', 'QuestContext'))
                    counts['helper_calls'] += 1
        if group == 'settings':
            # Keep all 11 audited functions, but the old lookup becomes an explicit
            # context identity function. No GetAllWidgets/player-zero discovery.
            _, editor = A.graph_nodes(bp, BOUNDARY[1])
            nodes = list(editor.list_all_nodes())
            entry = next(node for node in nodes if node.get_class().get_name() == 'K2Node_FunctionEntry')
            result = next(node for node in nodes if node.get_class().get_name() == 'K2Node_FunctionResult')
            for node in (entry, result):
                for pin in A.BL.list_all_pins(node):
                    A.PL.break_pin_links(pin)
            editor.remove_nodes([node for node in nodes if node not in (entry, result)])
            require(editor.remove_graph_output_parameter('HUD_Container'), 'Cannot replace source container output type')
            context_type = A.BL.get_object_reference_type(owned(MODULE).generated_class())
            require(editor.add_graph_output_parameter('HUD_Container', context_type) is not None, 'Cannot add explicit context output')
            returns = [node for node in editor.list_all_nodes() if node.get_class().get_name() == 'K2Node_FunctionResult']
            require(len(returns) == 1, 'Unexpected boundary return count')
            result = returns[0]
            context = A.pin(entry, 'QuestContext', True)
            valid = editor.add_call_function_node('/Script/Engine.KismetSystemLibrary:IsValid')
            A.connect(context, A.pin(valid, 'Object'))
            A.connect(A.pin(valid, 'ReturnValue', True), A.pin(result, 'ReturnValue'))
            A.connect(context, A.pin(result, 'HUD_Container'))
            A.connect(A.pin(entry, 'then', True), A.pin(result, 'execute'))
        ue().EditorAssetLibrary.set_metadata_tag(bp, key, 'v1')
        report[group] = counts
    return write('helper-context', {'libraries': report, 'native_compile_required': True, 'runtime_ready': False})


def adapt_leaf_context():
    idle()
    A = support()
    closure = analyze_sources()['helper_closure']
    require(all(ue().EditorAssetLibrary.get_metadata_tag(owned(path), 'AZ.Quest.HelperContext') == 'v1'
                for _, path in LIBRARIES.values()), 'Adapt/native-compile helper bodies first')
    report = {}
    for package in COPIES.values():
        bp = owned(package)
        key = 'AZ.Quest.LeafContext'
        stage = ue().EditorAssetLibrary.get_metadata_tag(bp, key)
        calls = _leaf_context_calls(bp, closure)
        missing = [item for item in calls if not item[2].list_connected_pins()]
        if stage in ('v1', 'v2-visible-context'):
            require(not missing, 'Stamped leaf still lacks context; run repair_false_zero_leaf_context() for the verified zero-change receipt')
            report[package] = {'state': 'already adapted and links verified', 'calls': len(calls)}
            continue
        require(not stage, 'Interrupted leaf context stage: ' + package)
        ue().EditorAssetLibrary.set_metadata_tag(bp, key, 'adapting')
        changed = _connect_leaf_context_calls(calls)
        ue().EditorAssetLibrary.set_metadata_tag(bp, key, 'v2-visible-context')
        report[package] = {'calls': len(calls), 'context_links_added': changed, 'calls_replaced': 0}
    return write('leaf-context-v2', {'assets': report, 'native_compile_required': True, 'runtime_ready': False})


def _leaf_context_calls(bp, closure):
    A = support()
    result = []
    for graph in A.graphs_under(bp):
        editor = A.GE.get_graph_editor(graph)
        for node in editor.list_all_nodes():
            if node.get_class().get_name() != 'K2Node_CallFunction':
                continue
            identity = _helper_call(node, None, closure)
            if identity is None:
                continue
            require(identity != BOUNDARY, 'Unexpected direct container lookup in mission leaf')
            context = _typed_context_pin(node)
            require(context is not None, 'Compile current helper signatures before leaf repair')
            require(len(context.list_connected_pins()) <= 1, 'Unexpected context input fan-in')
            result.append((editor, node, context))
    return result


def _connect_leaf_context_calls(calls):
    A = support()
    getters, changed = {}, 0
    for editor, node, pin in calls:
        if pin.list_connected_pins():
            continue
        graph_name = editor.get_graph().get_path_name()
        if graph_name not in getters:
            getters[graph_name] = _quest_context(editor)
        A.connect(getters[graph_name], pin)
        changed += 1
    return changed


def repair_false_zero_leaf_context():
    """Repair ONLY the documented three v1 zero-write leaves; preserve existing calls."""
    idle()
    A = support()
    original = OUT / 'leaf-context.json'
    require(original.exists(), 'Missing original zero-change authoring receipt')
    recorded = json.loads(original.read_text(encoding='utf-8')).get('calls_replaced', {})
    require(set(recorded) == set(COPIES.values()) and all(type(value) is int and value == 0 for value in recorded.values()),
            'Receipt is not the known all-three false-zero stage')
    closure = analyze_sources()['helper_closure']
    planned = []
    for package in COPIES.values():
        bp = owned(package)
        stage = ue().EditorAssetLibrary.get_metadata_tag(bp, 'AZ.Quest.LeafContext')
        calls = _leaf_context_calls(bp, closure)
        require(calls, 'Classifier did not identify any helper calls: ' + package)
        require(stage in ('v1', 'v2-visible-context'), 'Unexpected leaf repair state: ' + stage)
        if stage == 'v2-visible-context':
            require(all(pin.list_connected_pins() for _, _, pin in calls), 'Partial v2 context repair needs inspection')
        else:
            require(all(not pin.list_connected_pins() for _, _, pin in calls),
                    'v1 graph has intervening context edits; preserve and inspect them')
        planned.append((package, bp, stage, calls))
    report = {'source_receipt': str(original), 'source_receipt_sha256': digest_file(original), 'assets': {}}
    for package, bp, stage, calls in planned:
        if stage == 'v2-visible-context':
            report['assets'][package] = {'state': 'already repaired', 'calls': len(calls)}
            continue
        ue().EditorAssetLibrary.set_metadata_tag(bp, 'AZ.Quest.LeafContext', 'repairing-zero-v1')
        changed = _connect_leaf_context_calls(calls)
        require(changed == len(calls), 'Not every planned context link was connected')
        ue().EditorAssetLibrary.set_metadata_tag(bp, 'AZ.Quest.LeafContext', 'v2-visible-context')
        report['assets'][package] = {'calls': len(calls), 'context_links_added': changed, 'calls_replaced': 0}
        write('leaf-context-zero-repair', report)
    report['native_compile_required'] = True
    return write('leaf-context-zero-repair', report)


def repair_missing_library_context_connections():
    """Connect any missed cross-library context pins in already-adapted owned helpers."""
    idle()
    A = support()
    closure = analyze_sources()['helper_closure']
    planned = []
    for group, (_, package) in LIBRARIES.items():
        bp = owned(package)
        require(ue().EditorAssetLibrary.get_metadata_tag(bp, 'AZ.Quest.HelperContext') == 'v1', 'Library body was not adapted')
        for function in closure[group]:
            _, editor = A.graph_nodes(bp, function)
            context = _quest_context(editor, True)
            for node in editor.list_all_nodes():
                if node.get_class().get_name() == 'K2Node_CallFunction' and _helper_call(node, group, closure):
                    pin = _typed_context_pin(node)
                    require(pin is not None and len(pin.list_connected_pins()) <= 1, 'Unexpected helper context signature')
                    if not pin.list_connected_pins():
                        planned.append((package, function, node, context, pin))
    report = []
    for package, function, node, context, pin in planned:
        A.connect(context, pin)
        report.append({'asset': package, 'function': function, 'node': node.get_path_name()})
    return write('library-context-link-repair', {'context_links_added': report, 'calls_replaced': 0, 'native_compile_required': bool(report)})


class ModuleGraph:
    """Small exact recipes for the three audited source configuration functions."""
    def __init__(self, name):
        self.A = support()
        self.bp = owned(MODULE)
        _, self.editor = self.A.graph_nodes(self.bp, name)
        nodes = list(self.editor.list_all_nodes())
        entries = [node for node in nodes if node.get_class().get_name() == 'K2Node_FunctionEntry']
        returns = [node for node in nodes if node.get_class().get_name() == 'K2Node_FunctionResult']
        require(len(entries) == len(returns) == 1 and len(nodes) == 2, 'Module body is not the prepared empty graph: ' + name)
        self.entry, self.result = entries[0], returns[0]
        require(not self.A.pin(self.entry, 'then', True).list_connected_pins(), 'Module body already wired')
        self.actions = [str(action) for action in self.editor.list_available_nodes([])]
        self._self = None

    def param(self, name):
        return self.A.pin(self.entry, name, True)

    def get(self, name):
        node = self.editor.add_get_member_variable_node(name)
        require(node is not None, 'Missing module setting: ' + name)
        return self.A.pin(node, name, True)

    def action(self, title, expected_class):
        if expected_class == 'K2Node_Select' and normalized(title) == 'select':
            matches = [action for action in self.actions if action == 'Utilities|Select']
        else:
            matches = [action for action in self.actions if normalized(action.rsplit('|', 1)[-1]) == normalized(title)]
        require(len(matches) == 1, 'Missing/ambiguous action ' + title + ': ' + str(matches))
        node = self.editor.create_node_from_name(matches[0], ue().Vector2D(0, 0), [], None)
        require(node is not None and node.get_class().get_name() == expected_class, 'Wrong action class: ' + title)
        return node

    def context(self):
        if self._self is None:
            node = self.action('Get a reference to self', 'K2Node_Self')
            self._self = self.A.pin(node, 'self', True)
        return self._self

    def helper(self, group, name, **inputs):
        node = self.editor.add_call_function_node(_helper_path(group, name))
        require(node is not None, 'Helper signature is not compiled: ' + name)
        self.A.connect(self.context(), self.A.pin(node, 'QuestContext'))
        for field, value in inputs.items():
            self.A.connect(value, self.A.pin(node, field))
        return node

    def local(self, name, **inputs):
        node = self.editor.add_call_function_node(name)
        require(node is not None, 'Module signature is not compiled: ' + name)
        for field, value in inputs.items():
            self.A.connect(value, self.A.pin(node, field))
        return self.A.pin(node, 'ReturnValue', True)

    def member(self, node, prefix, output=False):
        pins = self.A.BL.list_output_pins(node) if output else self.A.BL.list_input_pins(node)
        matches = [pin for pin in pins if str(self.A.PL.get_pin_name(pin)) == prefix
                   or str(self.A.PL.get_pin_name(pin)).startswith(prefix + '_')]
        require(len(matches) == 1, 'Missing/ambiguous struct member: ' + prefix)
        return matches[0]

    def struct_pair(self, struct_name, value):
        break_node = self.action('Break ' + struct_name, 'K2Node_BreakStruct')
        make_node = self.action('Make ' + struct_name, 'K2Node_MakeStruct')
        self.A.connect(value, self.A.pin(break_node, struct_name))
        # Preserve all fields rather than constructing a reduced lookalike struct.
        for pin in self.A.BL.list_output_pins(break_node):
            name = str(self.A.PL.get_pin_name(pin))
            self.A.connect(pin, self.A.pin(make_node, name))
        return break_node, make_node

    def replace_member(self, make_node, prefix, value):
        destination = self.member(make_node, prefix)
        self.A.PL.break_pin_links(destination)
        self.A.connect(value, destination)

    def select(self, condition, when_false, when_true):
        node = self.action('Select', 'K2Node_Select')
        self.A.connect(condition, self.A.pin(node, 'Index'))
        choices = [pin for pin in self.A.BL.list_input_pins(node) if str(self.A.PL.get_pin_name(pin)) != 'Index']
        require(len(choices) == 2, 'Unexpected Select options')
        self.A.connect(when_false, choices[0])
        # Connecting a wildcard can reconstruct type information: reacquire pins.
        choices = [pin for pin in self.A.BL.list_input_pins(node) if str(self.A.PL.get_pin_name(pin)) != 'Index']
        self.A.connect(when_true, choices[1])
        return self.A.pin(node, 'ReturnValue', True)

    def finish(self, output, value):
        self.A.connect(value, self.A.pin(self.result, output))
        self.A.connect(self.A.pin(self.entry, 'then', True), self.A.pin(self.result, 'execute'))


def repair_partial_find_font_info():
    """Reset only the exact owned four-node partial body left by Select ambiguity."""
    idle()
    A = support()
    bp = owned(MODULE)
    key = 'AZ.Quest.ConfigBody.FindFontInfo'
    stage = ue().EditorAssetLibrary.get_metadata_tag(bp, key)
    _, editor = A.graph_nodes(bp, 'FindFontInfo')
    nodes = list(editor.list_all_nodes())
    if stage == 'v1':
        return {'state': 'FindFontInfo already complete; unchanged'}
    if not stage and len(nodes) == 2:
        require({node.get_class().get_name() for node in nodes} == {'K2Node_FunctionEntry', 'K2Node_FunctionResult'},
                'Unexpected reset graph shape')
        return {'state': 'already reset; run wire_module_config'}
    require(stage == 'wiring' and len(nodes) == 4, 'Not the known Select-ambiguity partial body; inspect without deleting')
    by_class = {}
    for node in nodes:
        by_class.setdefault(node.get_class().get_name(), []).append(node)
    expected = {'K2Node_FunctionEntry', 'K2Node_FunctionResult', 'K2Node_BreakStruct', 'K2Node_MakeStruct'}
    require(set(by_class) == expected and all(len(value) == 1 for value in by_class.values()),
            'Partial graph contains unexpected/user-authored nodes')
    entry = by_class['K2Node_FunctionEntry'][0]
    result = by_class['K2Node_FunctionResult'][0]
    break_node, make_node = by_class['K2Node_BreakStruct'][0], by_class['K2Node_MakeStruct'][0]
    require(A.pin(break_node, 'S_TextInfo_H').is_valid() and A.pin(make_node, 'S_TextInfo_H', True).is_valid(),
            'Partial body is not the expected text-info Make/Break pair')
    for pin in (A.pin(entry, 'then', True), A.pin(result, 'execute'), A.pin(result, 'ReturnValue'),
                A.pin(make_node, 'S_TextInfo_H', True)):
        require(not pin.list_connected_pins(), 'Partial graph already has completed output/work; preserve it')
    for node in nodes:
        for pin in A.BL.list_all_pins(node):
            require(all(other.get_owning_node() in nodes for other in pin.list_connected_pins()),
                    'Partial graph has external connections')
    receipt = {'asset': MODULE, 'function': 'FindFontInfo', 'metadata_before': stage,
               'removed_nodes': [break_node.get_path_name(), make_node.get_path_name()],
               'preserved_nodes': [entry.get_path_name(), result.get_path_name()],
               'reason': 'Verified Utilities|Select versus AZ|QuickBar|Select ambiguity before Select creation'}
    editor.remove_nodes([break_node, make_node])
    ue().EditorAssetLibrary.set_metadata_tag(bp, key, '')
    require(len(editor.list_all_nodes()) == 2, 'Unexpected result after narrow partial-body removal')
    return write('find-font-info-partial-repair', receipt)


def wire_module_config():
    """Equivalent to source local functions; no HUD-container copy/global lookup."""
    idle()
    A = support()
    module = owned(MODULE)
    require(all(ue().EditorAssetLibrary.get_metadata_tag(module, 'AZ.Quest.Signature.' + name) == 'v1'
                for name in CONTAINER_SEEDS), 'Prepare and native-compile module signatures first')
    for struct_name in ('S_TextInfo_H', 'S_MissionNotiBaseInfo_H', 'S_TaskBaseInfo'):
        load(SOURCE + '/Blueprints/Structs/' + struct_name)
    report = {}
    # FindFontInfo must exist before its two callers are authored.
    for name in ('FindFontInfo', 'GetMissionNotiBaseInfo', 'GetMissionTasksBaseInfo'):
        key = 'AZ.Quest.ConfigBody.' + name
        stage = ue().EditorAssetLibrary.get_metadata_tag(module, key)
        if stage == 'v1':
            report[name] = 'already wired'
            continue
        require(not stage, 'Interrupted module recipe: ' + name)
        graph = ModuleGraph(name)
        ue().EditorAssetLibrary.set_metadata_tag(module, key, 'wiring')
        if name == 'FindFontInfo':
            old, new = graph.struct_pair('S_TextInfo_H', graph.param('S_TextInfo'))
            use_manual = graph.member(old, 'UseManualFontInfo', True)
            font = graph.select(use_manual, graph.param('StylesheetFont'), graph.member(old, 'FontInfo', True))
            original_color = graph.member(old, 'ColorAndOpacity', True)
            changed = graph.helper('widgets', 'HasColorChanged', InColor=original_color)
            color = graph.select(A.pin(changed, 'ReturnValue', True), graph.param('StylesheetColor'), original_color)
            graph.replace_member(new, 'FontInfo', font)
            graph.replace_member(new, 'ColorAndOpacity', color)
            graph.finish('ReturnValue', A.pin(new, 'S_TextInfo_H', True))
        elif name == 'GetMissionNotiBaseInfo':
            old, new = graph.struct_pair('S_MissionNotiBaseInfo_H', graph.get('MissionNotificationInfo'))
            headline = graph.local('FindFontInfo', S_TextInfo=graph.member(old, 'HeadlineTextInfo', True),
                                   StylesheetFont=graph.get('HeadlineFont'), StylesheetColor=graph.get('FontColor1'))
            original_color = graph.member(old, 'BackgroundColor', True)
            changed = graph.helper('widgets', 'HasColorChanged', InColor=original_color)
            color = graph.select(A.pin(changed, 'ReturnValue', True), graph.get('ContrastColor1'), original_color)
            graph.replace_member(new, 'HeadlineTextInfo', headline)
            graph.replace_member(new, 'BackgroundColor', color)
            graph.finish('S_MissionNotiBaseInfo', A.pin(new, 'S_MissionNotiBaseInfo_H', True))
        else:
            old, new = graph.struct_pair('S_TaskBaseInfo', graph.get('MissionTasksInfo'))
            for state in ('Empty', 'Checked', 'Failed', 'Active'):
                text = graph.local('FindFontInfo', S_TextInfo=graph.member(old, 'Text' + state, True),
                                   StylesheetFont=graph.get('BodyFont2' if state == 'Active' else 'BodyFont'),
                                   StylesheetColor=graph.get('FontColor1'))
                icon = graph.helper('settings', 'FindIconColor', S_IconInfo=graph.member(old, 'Icon' + state, True),
                                    StylesheetColor=graph.get('BaseColor1' if state == 'Active' else 'BaseColor2'))
                graph.replace_member(new, 'Text' + state, text)
                graph.replace_member(new, 'Icon' + state, A.pin(icon, 'ReturnValue', True))
            graph.finish('S_TaskBaseInfo', A.pin(new, 'S_TaskBaseInfo', True))
        ue().EditorAssetLibrary.set_metadata_tag(module, key, 'v1')
        report[name] = 'source semantics wired'
    return write('module-config-bodies', {'functions': report, 'native_compile_required': True, 'runtime_ready': False})


def prepare_context_propagation():
    idle()
    A = support()
    result = {}
    for package in COPIES.values():
        bp = owned(package)
        key = 'AZ.Quest.PropagationSignature'
        stage = ue().EditorAssetLibrary.get_metadata_tag(bp, key)
        if stage == 'v1':
            result[package] = 'already prepared'
            continue
        require(not stage and A.BL.find_graph(bp, 'ApplyQuestContext') is None, 'Unowned/partial context propagation function')
        require('QuestContext' in {str(name) for name in A.BL.list_member_variable_names(bp, False)}, 'Missing context member')
        ue().EditorAssetLibrary.set_metadata_tag(bp, key, 'preparing')
        editor = A.GE.create_and_edit_function_graph(bp, 'ApplyQuestContext')
        editor.set_function_is_public()
        require(editor.add_return_node() is not None, 'Cannot create void propagation result')
        ue().EditorAssetLibrary.set_metadata_tag(bp, key, 'v1')
        result[package] = 'public ApplyQuestContext signature prepared'
    return write('propagation-signatures', {'widgets': result, 'native_compile_required': True, 'runtime_ready': False})


def _preview_children(package):
    if package.endswith('/WBP_AZ_MissionNotification'):
        return [('WB_Mission', DEST + '/WBP_AZ_Mission')]
    if package.endswith('/WBP_AZ_Mission'):
        return [(name, DEST + '/WBP_AZ_Task') for name in ('Task_Empty', 'Task_Checked', 'Task_Unchecked', 'Task_Active')]
    return []


def wire_context_propagation():
    """Module must set notification.QuestContext and call ApplyQuestContext before hosting it.

    Dynamic children receive owner/context before AddChild can construct Slate.
    Existing designer preview children are configured recursively by that same
    entry point, with a PreConstruct refresh for preview/reconstruction.
    """
    idle()
    A = support()
    report = {}
    for package in COPIES.values():
        bp = owned(package)
        key = 'AZ.Quest.PropagationBody'
        stage = ue().EditorAssetLibrary.get_metadata_tag(bp, key)
        if stage == 'v1':
            report[package] = 'already wired'
            continue
        require(not stage and ue().EditorAssetLibrary.get_metadata_tag(bp, 'AZ.Quest.PropagationSignature') == 'v1',
                'Prepare/native-compile context propagation signatures first')
        _, editor = A.graph_nodes(bp, 'ApplyQuestContext')
        nodes = list(editor.list_all_nodes())
        require(len(nodes) == 2, 'Unexpected propagation body')
        entry = next(node for node in nodes if node.get_class().get_name() == 'K2Node_FunctionEntry')
        result = next(node for node in nodes if node.get_class().get_name() == 'K2Node_FunctionResult')
        rows = widget_rows(package)
        for name, child_package in _preview_children(package):
            require(name in rows and rows[name]['widgetClassPath']['refPath'].split('.')[0] == child_package,
                    'Replace/native-compile embedded widgets before propagation')
        ue().EditorAssetLibrary.set_metadata_tag(bp, key, 'wiring')
        tails = [A.pin(entry, 'then', True)]
        context = _quest_context(editor)
        owner = editor.add_call_function_node('/Script/UMG.Widget:GetOwningPlayer')
        for name, child_package in _preview_children(package):
            child_class = owned(child_package).generated_class().get_path_name()
            child = editor.add_get_member_variable_node(name)
            child_pin = A.pin(child, name, True)
            valid = editor.add_call_function_node('/Script/Engine.KismetSystemLibrary:IsValid')
            A.connect(child_pin, A.pin(valid, 'Object'))
            branch = editor.add_branch_node()
            A.connect(A.pin(valid, 'ReturnValue', True), A.pin(branch, 'Condition'))
            for tail in tails:
                A.connect(tail, A.pin(branch, 'execute'))
            assign = editor.add_set_member_variable_node('QuestContext', child_class)
            A.connect(child_pin, A.pin(assign, 'self'))
            A.connect(context, A.pin(assign, 'QuestContext'))
            A.connect(A.pin(branch, 'then', True), A.pin(assign, 'execute'))
            set_owner = editor.add_call_function_node('/Script/UMG.UserWidget:SetOwningPlayer')
            A.connect(child_pin, A.pin(set_owner, 'self'))
            A.connect(A.pin(owner, 'ReturnValue', True), A.pin(set_owner, 'LocalPlayerController'))
            A.connect(A.pin(assign, 'then', True), A.pin(set_owner, 'execute'))
            apply = editor.add_call_function_node(child_class + ':ApplyQuestContext')
            require(apply is not None, 'Native compile child propagation signature first')
            A.connect(child_pin, A.pin(apply, 'self'))
            A.connect(A.pin(set_owner, 'then', True), A.pin(apply, 'execute'))
            tails = [A.pin(apply, 'then', True), A.pin(branch, 'else', True)]
        for tail in tails:
            A.connect(tail, A.pin(result, 'execute'))

        factories = 0
        for graph in A.graphs_under(bp):
            factory_editor = A.GE.get_graph_editor(graph)
            for node in list(factory_editor.list_all_nodes()):
                if node.get_class().get_name() != 'K2Node_CreateWidget':
                    continue
                class_value = str(A.PL.get_pin_value(A.pin(node, 'Class')))
                child_packages = [candidate for candidate in COPIES.values()
                                  if re.search(r'\b' + re.escape(candidate.rsplit('/', 1)[1]) + r'_C\b', class_value)]
                require(len(child_packages) == 1, 'Unexpected mission widget factory class: ' + class_value)
                child_class = owned(child_packages[0]).generated_class().get_path_name()
                parent_owner = factory_editor.add_call_function_node('/Script/UMG.Widget:GetOwningPlayer')
                owner_pin = A.pin(node, 'OwningPlayer')
                require(not owner_pin.list_connected_pins(), 'Factory owning player already wired; inspect before replacement')
                A.connect(A.pin(parent_owner, 'ReturnValue', True), owner_pin)
                parent_context = _quest_context(factory_editor)
                spawn_context = node.find_input_pin('QuestContext')
                if spawn_context.is_valid():
                    require(not spawn_context.list_connected_pins(), 'Factory spawn context already wired')
                    A.connect(parent_context, spawn_context)
                assign = factory_editor.add_set_member_variable_node('QuestContext', child_class)
                A.connect(A.pin(node, 'ReturnValue', True), A.pin(assign, 'self'))
                A.connect(parent_context, A.pin(assign, 'QuestContext'))
                apply = factory_editor.add_call_function_node(child_class + ':ApplyQuestContext')
                require(apply is not None, 'Native compile child ApplyQuestContext first')
                A.connect(A.pin(node, 'ReturnValue', True), A.pin(apply, 'self'))
                following = list(A.pin(node, 'then', True).list_connected_pins())
                require(len(following) <= 1, 'Unexpected factory execution fanout')
                A.PL.break_pin_links(A.pin(node, 'then', True))
                A.connect(A.pin(node, 'then', True), A.pin(assign, 'execute'))
                A.connect(A.pin(assign, 'then', True), A.pin(apply, 'execute'))
                for next_pin in following:
                    A.connect(A.pin(apply, 'then', True), next_pin)
                factories += 1
        expected = 0 if package.endswith('/WBP_AZ_Task') else 1
        require(factories == expected, 'Unexpected number of mission widget factories: ' + str(factories))

        _, events = A.graph_nodes(bp, 'EventGraph')
        preconstruct = [node for node in events.list_all_nodes() if node.get_class().get_name() == 'K2Node_Event'
                        and normalized(A.title(node)) in ('preconstruct', 'eventpreconstruct')]
        require(len(preconstruct) <= 1, 'Ambiguous PreConstruct event')
        event = preconstruct[0] if preconstruct else A.BL.add_event_override(bp, 'PreConstruct', ue().IntPoint(0, 0))
        require(event is not None, 'Could not add PreConstruct override')
        then = A.pin(event, 'then', True)
        following = list(then.list_connected_pins())
        require(len(following) <= 1, 'PreConstruct execution fanout changed')
        call = events.add_call_function_node('ApplyQuestContext')
        require(call is not None, 'Missing ApplyQuestContext signature')
        A.PL.break_pin_links(then)
        A.connect(then, A.pin(call, 'execute'))
        for next_pin in following:
            A.connect(A.pin(call, 'then', True), next_pin)
        ue().EditorAssetLibrary.set_metadata_tag(bp, key, 'v1')
        report[package] = {'preview_children': len(_preview_children(package)), 'dynamic_factories': factories}
    return write('context-propagation', {'widgets': report, 'native_compile_required': True,
                                        'module_contract': 'Set notification QuestContext=self; call ApplyQuestContext BEFORE hosting/initialization.',
                                        'runtime_ready': False})


def audit_context():
    """Readback only after native compile. This is not a gameplay/PIE check."""
    A = support()
    closure = analyze_sources()['helper_closure']
    findings, counts = [], {}
    targets = list(COPIES.values()) + [path for _, path in LIBRARIES.values()] + [MODULE]
    for package in targets:
        bp = owned(package)
        group = next((key for key, (_, path) in LIBRARIES.items() if path == package), None)
        summary = {'helper_calls': 0, 'functions': [], 'errors': []}
        for graph in A.graphs_under(bp):
            editor = A.GE.get_graph_editor(graph)
            summary['functions'].append(graph.get_name())
            summary['errors'].extend(node.get_path_name() for node in editor.list_nodes_with_errors())
            for node in editor.list_all_nodes():
                if node.get_class().get_name() != 'K2Node_CallFunction':
                    continue
                label = normalized(A.title(node))
                if label in ('getallwidgetsofclass', 'getplayercontroller', 'getplayercameramanager', 'gethudmanagerh'):
                    findings.append({'asset': package, 'node': node.get_path_name(), 'issue': 'Global context lookup remains'})
                identity = _helper_call(node, group, closure)
                if identity:
                    summary['helper_calls'] += 1
                    context = node.find_input_pin('QuestContext')
                    if not context.is_valid() or not context.list_connected_pins():
                        findings.append({'asset': package, 'node': node.get_path_name(), 'issue': 'Helper context is not connected'})
                    self_pin = node.find_input_pin('self')
                    if self_pin.is_valid() and 'BP_PHV2_' in str(A.PL.get_pin_type_as_json_schema(self_pin)):
                        findings.append({'asset': package, 'node': node.get_path_name(), 'issue': 'Source helper owner remains'})
        counts[package] = summary
    result = write('context-readback', {'assets': counts, 'findings': findings,
                                      'runtime_ready': False, 'scope': 'Context/config only; no active HUD attachment or progress hardening claimed.'})
    require(not findings and not any(item['errors'] for item in counts.values()), 'Context/readback errors; inspect saved receipt')
    return result


def record_native_compile(results):
    """Receipt only. Caller passes actual dedicated-tool results; never invokes compilation."""
    require(isinstance(results, dict) and results, 'Pass actual per-package native compile results')
    require(set(results) <= set(ALL_COPIES.values()) | {MODULE}, 'Unrelated package in compile receipt')
    return write('native-compile', {'timestamp_utc': datetime.now(timezone.utc).isoformat(),
                                    'reported_native_results': results, 'runtime_ready': False})


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--analyze', action='store_true', help='Read saved source receipts only')
    args = parser.parse_args()
    require(args.analyze, 'Only --analyze is exposed on the command line; editor stages must be called explicitly')
    report = analyze_sources()
    print(json.dumps({key: report[key] for key in ('helper_closure', 'module_source_fields',
                                                 'missing_container_local_bodies', 'unresolved_library_calls', 'runtime_ready')}, indent=2))
