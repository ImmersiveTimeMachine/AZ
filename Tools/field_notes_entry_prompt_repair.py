# @Description: Retire the legacy card key label and remove only its newly authored invalid getter pair.
import gc
import importlib.util
import json
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')


def repair():
    spec = importlib.util.spec_from_file_location('fn_entry_prompt_host', ROOT/'Tools/field_notes_prompt_hosts.py')
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    s = m.gate(); asset = m.ENTRY
    marker = m.OUT/'WBP_AZ_QuickSelectEntry-Bindings.json'
    state = json.loads(marker.read_text())
    m.require(state['state'] == 'complete' and state['asset'] == asset, 'Wrong entry stage')
    before = json.loads((Path(state['backup'])/'before.json').read_text())
    current = m.snapshot(asset)
    diagnostic = json.loads((m.OUT/'entry-key-access-diagnostic.json').read_text())
    m.require(current['graphs'] == diagnostic['graphs'], 'Inspected entry graph changed')
    m.preserve_objects(before, current)
    old_paths = {r['node'] for r in before['graphs']['EventGraph']}
    added = [r for r in current['graphs']['EventGraph'] if r['node'] not in old_paths]
    getters = [r for r in added if r['class'] == 'K2Node_VariableGet' and r['title'] == 'Get KeyText']
    m.require(len(getters) == 1, 'The new invalid getter is not unique')
    getter = getters[0]
    links = [v for p in getter['pins'] if p['name'] == 'KeyText' and p['output'] for v in p['links']]
    m.require(len(links) == 1 and links[0]['pin'] == 'self', 'Unexpected legacy getter consumers')
    setters = [r for r in added if r['node'] == links[0]['node'] and r['class'] == 'K2Node_CallFunction'
               and r['title'] == 'SetVisibility']
    m.require(len(setters) == 1, 'The new legacy setter is not unique')
    g = m.Graph(asset); nodes = {n.get_path_name(): n for n in g.g.editor.list_all_nodes()}
    setter_node = nodes[setters[0]['node']]
    incoming = list(g.b.inp(setter_node, 'execute').list_connected_pins())
    outgoing = list(g.b.out(setter_node, 'then').list_connected_pins())
    m.require(len(incoming) == len(outgoing) == 1, 'Expected one execution edge on each side')
    m.require(incoming[0].get_owning_node().get_path_name() not in old_paths,
              'Do not replace an original execution owner')
    receipt = m.OUT/'entry-key-access-repair.json'
    m.require(not receipt.exists(), 'Repair already started; inspect receipt')
    s.write(receipt, {'state': 'writing', 'before': current,
                      'removed_own_nodes': [getter['node'], setters[0]['node']]})
    m.patch(m.rows(asset)['KeyText']['widget'], m.LEGACY_SHORTCUT_STYLE)
    s.ue().BlueprintGraphPinLibrary.break_pin_links(g.b.inp(setter_node, 'execute'))
    s.ue().BlueprintGraphPinLibrary.break_pin_links(g.b.out(setter_node, 'then'))
    g.b.connect(incoming[0], outgoing[0])
    # These are pure VariableGet / CallFunction data-flow nodes, not member/event
    # declarations. The installed engine RemoveNodes marks them Modified only.
    g.g.editor.remove_nodes([nodes[getter['node']], setter_node])
    after = m.snapshot(asset)
    m.require(not {getter['node'], setters[0]['node']}.intersection(r['node'] for r in after['graphs']['EventGraph']),
              'Invalid nodes were not removed')
    allowed = set()
    for row in before['graphs']['EventGraph']:
        if row['class'] == 'K2Node_Event' and 'OnEntryViewChanged' in row['title']:
            allowed.add((row['node'], 'then', True))
            allowed.add((row['node'], 'View', True))
            for pin in row['pins']:
                if pin['name'] == 'then' and pin['output']:
                    allowed.update((v['node'], v['pin'], False) for v in pin['links'])
    g.o.preserve_existing_graph(before['graphs']['EventGraph'], after['graphs']['EventGraph'], allowed)
    for name, graph in before['graphs'].items():
        if name != 'EventGraph': m.require(after['graphs'][name] == graph, 'Unrelated graph changed')
    m.preserve_objects(before, after, {'KeyText:widget': m.LEGACY_SHORTCUT_STYLE})
    m.require(current['tree'] == after['tree'], 'Card widget identities or hierarchy changed')
    s.write(receipt, {'state': 'complete', 'before': current, 'after': after,
                      'removed_own_nodes': [getter['node'], setters[0]['node']],
                      'original_nodes_preserved': True, 'native_compile_save_required': True})
    state['legacy_label_retired_in_template'] = True
    state['native_compile_save_required'] = True
    s.write(marker, state)
    gc.collect()
    return {'asset': asset, 'removed_own_invalid_nodes': 2, 'original_nodes_preserved': True,
            'legacy_name_and_binding_preserved': True, 'native_compile_save_required': True}
