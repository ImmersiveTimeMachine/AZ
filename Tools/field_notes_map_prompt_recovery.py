# @Description: Resume only the captured incomplete Map prompt visibility tail.
"""Two explicit stages with an external native Blueprint compile between them.
No Blueprint compile/save, gameplay input or PIE is performed here.
"""
import importlib.util
import json
import gc
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')


def host():
    spec = importlib.util.spec_from_file_location('fn_map_prompt_host', ROOT/'Tools/field_notes_prompt_hosts.py')
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def state(m):
    marker = m.OUT/'WBP_AZ_QuestMapPage-Bindings.json'
    value = json.loads(marker.read_text())
    m.require(value['state'] == 'writing' and value['asset'] == m.MAP
              and value['recipe_version'] == m.OWNER, 'Not the known incomplete Map binding stage')
    before = json.loads((Path(value['backup'])/'before.json').read_text())
    return marker, value, before


def allowed_splice(before):
    result = set()
    for row in before['graphs']['EventGraph']:
        if row['class'] == 'K2Node_Event' and 'OnMapActionBindingsChanged' in row['title']:
            result.add((row['node'], 'then', True))
            for pin in row['pins']:
                if pin['name'] == 'then' and pin['output']:
                    result.update((v['node'], v['pin'], False) for v in pin['links'])
    return result


def expose_footer_variables():
    m = host(); s = m.gate(); marker, value, before = state(m)
    receipt = m.OUT/'map-footer-variable-recovery.json'
    m.require(not receipt.exists(), 'Footer recovery already started; inspect its receipt')
    current = m.snapshot(m.MAP)
    captured = json.loads((m.OUT/'map-bindings-interrupted.json').read_text())
    m.require(current['graphs'] == captured['graphs'], 'Incomplete graph changed since inspection')
    m.preserve_objects(before, current)
    m.module('field_notes_overlay_setup.py').preserve_existing_graph(
        before['graphs']['EventGraph'], current['graphs']['EventGraph'], allowed_splice(before))
    rows = m.rows(m.MAP)
    expected = {'BackHint': '/Script/UMG.TextBlock', 'FooterLayout': '/Script/UMG.HorizontalBox'}
    for name, cls in expected.items():
        m.require(rows[name]['widgetClassPath']['refPath'] == cls and not rows[name]['bIsVariable'],
                  'Reviewed static footer widget changed: '+name)
    s.write(receipt, {'phase': 'exposing', 'before': current, 'binding_backup': value['backup']})
    for name in expected:
        m.tool('ToggleWidgetAsVariable', widgetBlueprint=s.ref(m.MAP), widget=rows[name]['widget'], bIsVariable=True)
    after = m.snapshot(m.MAP)
    m.preserve_objects(current, after)
    m.require(current['graphs'] == after['graphs'], 'Exposing footer variables changed a graph')
    before_rows = {r['widgetName']: r for r in current['tree']['widgets']}
    for row in after['tree']['widgets']:
        desired = dict(before_rows[row['widgetName']])
        if row['widgetName'] in expected: desired['bIsVariable'] = True
        m.require(row == desired, 'Unexpected tree change while exposing footer variables')
    s.write(receipt, {'phase': 'variables_exposed', 'before': current, 'after': after,
                      'binding_backup': value['backup']})
    gc.collect()
    return {'asset': m.MAP, 'variables_exposed': list(expected), 'next': 'External native compile, then finish_tail()'}


def finish_tail():
    m = host(); s = m.gate(); marker, value, before = state(m)
    receipt = m.OUT/'map-footer-variable-recovery.json'
    recovery = json.loads(receipt.read_text())
    m.require(recovery['phase'] == 'variables_exposed', 'Wrong recovery phase')
    current = m.snapshot(m.MAP)
    m.require(current['graphs'] == recovery['after']['graphs'], 'Graph changed during the external compile')
    m.preserve_objects(before, current)
    for name in ('BackHint', 'FooterLayout'):
        m.require(m.rows(m.MAP)[name]['bIsVariable'], 'Footer variable is not exposed')
    old = {r['node'] for r in before['graphs']['EventGraph']}
    added = [r for r in current['graphs']['EventGraph'] if r['node'] not in old]
    tails = [r for r in added if r['title'] == 'SetVisibility'
             and any(p['name'] == 'self' and any(v['pin'] == 'FN_MapSectionNavigation' for v in p['links']) for p in r['pins'])]
    casts = [r for r in added if r['class'] == 'K2Node_DynamicCast'
             and any(''.join(c for c in p['name'] if c.isalnum()).lower() == 'asazplayercontroller'
                     and p['output'] for p in r['pins'])]
    m.require(len(tails) == len(casts) == 1, 'Known incomplete tail/owner cast is not unique')
    m.require(any(p['name'] == 'then' and p['output'] and not p['links'] for p in tails[0]['pins']),
              'Tail already has a continuation')
    g = m.Graph(m.MAP)
    nodes = {n.get_path_name(): n for n in g.g.editor.list_all_nodes()}
    cast_pin = next(p['name'] for p in casts[0]['pins'] if p['output']
                    and ''.join(c for c in p['name'] if c.isalnum()).lower() == 'asazplayercontroller')
    recovery['phase'] = 'appending_tail'; s.write(receipt, recovery)
    m._map_visibility_tail(g, g.b.out(nodes[tails[0]['node']], 'then'),
                          g.b.out(nodes[casts[0]['node']], cast_pin))
    after = m.snapshot(m.MAP)
    g.o.preserve_existing_graph(before['graphs']['EventGraph'], after['graphs']['EventGraph'], allowed_splice(before))
    for name, graph in before['graphs'].items():
        if name != 'EventGraph': m.require(after['graphs'][name] == graph, 'Unrelated graph changed')
    m.preserve_objects(before, after)
    result = m.finish(s, marker, Path(value['backup']), event='OnMapActionBindingsChanged',
                      old_event_body_runs_first=True, new_command_bindings=0,
                      recovered_missing_widget_variables=['BackHint', 'FooterLayout'])
    recovery['phase'] = 'complete'; s.write(receipt, recovery)
    gc.collect()
    return result


def finish_device_branch():
    """Complete the known second stop: native cast exists, device branch absent."""
    m = host(); s = m.gate(); marker, value, before = state(m)
    receipt = m.OUT/'map-footer-variable-recovery.json'
    recovery = json.loads(receipt.read_text())
    m.require(recovery['phase'] == 'appending_tail', 'Not the inspected cast-name stop')
    current = m.snapshot(m.MAP)
    m.preserve_objects(before, current)
    old = {r['node'] for r in before['graphs']['EventGraph']}
    added = [r for r in current['graphs']['EventGraph'] if r['node'] not in old]
    backs = [r for r in added if r['title'] == 'SetVisibility'
             and any(p['name'] == 'self' and any(v['pin'] == 'BackHint' for v in p['links']) for p in r['pins'])]
    casts = [r for r in added if r['class'] == 'K2Node_DynamicCast'
             and ''.join(c for c in r['title'] if c.isalnum()).lower() == 'casttocommoninputsubsystem']
    m.require(len(backs) == len(casts) == 1, 'Inspected Back/cast nodes are not unique')
    m.require(any(p['name'] == 'then' and p['output'] and not p['links'] for p in backs[0]['pins']),
              'Back visibility already has a continuation')
    m.require(not any(r['title'] == 'IsInputMethodActive' for r in added), 'Device branch already exists')
    g = m.Graph(m.MAP)
    g.o.preserve_existing_graph(before['graphs']['EventGraph'], current['graphs']['EventGraph'], allowed_splice(before))
    nodes = {n.get_path_name(): n for n in g.g.editor.list_all_nodes()}
    result_pins = [p for p in s.ue().BlueprintEditorLibrary.list_output_pins(nodes[casts[0]['node']])
                   if str(p.get_pin_name()).startswith('As')]
    m.require(len(result_pins) == 1 and not list(result_pins[0].list_connected_pins()), 'Cast result already consumed')
    s.write(m.OUT/'map-device-branch-before.json', current)
    recovery['phase'] = 'completing_device_branch'; s.write(receipt, recovery)
    execute, method = g.call(g.b.out(nodes[backs[0]['node']], 'then'), '/Script/CommonInput.CommonInputSubsystem',
                             'IsInputMethodActive', {'self': result_pins[0]}, {'InputMethod': 'Gamepad'})
    pad, kbm = g.branch(execute, g.b.out(method))
    for name, visibility in (('FN_MapCommandBar', 'SelfHitTestInvisible'), ('FooterLayout', 'Collapsed')):
        pad = g.visibility(pad, name, visibility)
    for name, visibility in (('FN_MapCommandBar', 'Collapsed'), ('FooterLayout', 'SelfHitTestInvisible')):
        kbm = g.visibility(kbm, name, visibility)
    after = m.snapshot(m.MAP)
    g.o.preserve_existing_graph(before['graphs']['EventGraph'], after['graphs']['EventGraph'], allowed_splice(before))
    for name, graph in before['graphs'].items():
        if name != 'EventGraph': m.require(after['graphs'][name] == graph, 'Unrelated graph changed')
    m.preserve_objects(before, after)
    result = m.finish(s, marker, Path(value['backup']), event='OnMapActionBindingsChanged',
                      old_event_body_runs_first=True, new_command_bindings=0,
                      recovered_missing_widget_variables=['BackHint', 'FooterLayout'],
                      recovered_actual_cast_pin=True)
    recovery['phase'] = 'complete'; s.write(receipt, recovery)
    gc.collect()
    return result
