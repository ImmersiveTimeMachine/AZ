# @Description: Give the journal a full-height list with accessible expandable task details.
"""Root-only structural correction on MapPage; no graphs, progression or map transforms change.
Run author(), native compile/save the page externally, then verify().
"""
import importlib.util
import json
import shutil
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
PAGE = '/Game/AZ/Blueprints/Menu/Map/WBP_AZ_QuestMapPage'
NEW = {'FNSelectedTaskDetails', 'FNDetailsHeader', 'FNDetailsBody'}


def support():
    spec = importlib.util.spec_from_file_location('fn_journal_layout_base', ROOT/'Tools/field_notes_map_setup.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module


def call(name, **kwargs):
    m = support()
    assert name in ('AddWidget','MoveWidget','SetNamedSlotContent','GetWidgets')
    r = m.S.ue().ToolsetRegistry.execute_tool('UMGToolSet.UMGToolSet', name, json.dumps(kwargs))
    assert r.is_complete and not r.error, str(r.error)
    return json.loads(r.value)['returnValue']


def patch(ref, values):
    s = support().S; obj = s.resolve(ref)
    assert obj.get_package().get_name() == PAGE
    before = s.read_object(obj); s._validate_patch(before['schema'], values)
    obj.modify(); assert s.ue().ToolsetLibrary.set_object_properties(obj, json.dumps(values))
    assert s._contains(s.read_object(obj)['values'], values)


def author():
    m = support(); s = m.S; s.idle(); out = s.OUT/'JournalLayout'
    assert not (out/'started.json').exists(), 'Inspect the existing journal layout receipt before resuming'
    before = m.capture(PAGE); rows = m.rows(PAGE)
    assert not NEW.intersection(rows), 'Journal disclosure already exists'
    assert rows['DetailsScroll']['parent'] == rows['JournalLayout']['widget']
    folder = out/('Before-'+s.stamp()); s.write(folder/'snapshot.json', before)
    shutil.copy2(s.package_file(PAGE), folder/s.package_file(PAGE).name)
    s.write(out/'started.json', {'backup': str(folder), 'complete': False})
    area = call('AddWidget', widgetBlueprint=s.ref(PAGE), widgetClass={'refPath':'/Script/UMG.ExpandableArea'},
                widgetDisplayName='FNSelectedTaskDetails', parentWidget=rows['JournalLayout']['widget'], childIndex=3)
    patch(area['widget'], {'bIsExpanded': False, 'maxHeight': 220,
                          'headerPadding': {'left':4,'top':8,'right':4,'bottom':8},
                          'areaPadding': {'left':0,'top':4,'right':0,'bottom':4},
                          'borderBrush': {'drawAs':'NoDrawType'},
                          'style': {'collapsedImage': {'tintColor': s.slate(s.linear('Paper','text'))},
                                    'expandedImage': {'tintColor': s.slate(s.linear('Paper','text'))}}})
    patch(area['slot'], {'size': {'value':1,'sizeRule':'Automatic'},
                        'padding': {'left':0,'top':10,'right':0,'bottom':8}})
    header = call('SetNamedSlotContent', widgetBlueprint=s.ref(PAGE), hostWidget=area['widget'], slotName='Header',
                  widgetClass={'refPath':'/Script/UMG.TextBlock'}, widgetName='FNDetailsHeader')
    patch(header['widget'], {'text':'SELECTED TASK DETAILS', **m._text('hint_max','muted'),
                            'visibility':'HitTestInvisible'})
    body = call('SetNamedSlotContent', widgetBlueprint=s.ref(PAGE), hostWidget=area['widget'], slotName='Body',
                widgetClass={'refPath':'/Script/UMG.SizeBox'}, widgetName='FNDetailsBody')
    patch(body['widget'], {'maxDesiredHeight':180,'bOverride_MaxDesiredHeight':True,
                          'visibility':'SelfHitTestInvisible'})
    moved = call('MoveWidget', widgetBlueprint=s.ref(PAGE), widget=rows['DetailsScroll']['widget'], newParent=body['widget'])
    patch(moved['slot'], {'padding': {'left':0,'top':0,'right':0,'bottom':0},
                          'horizontalAlignment':'HAlign_Fill','verticalAlignment':'VAlign_Fill'})
    patch(rows['QuestList']['slot'], {'size': {'value':1,'sizeRule':'Fill'}})
    moved_back = call('MoveWidget', widgetBlueprint=s.ref(PAGE), widget=rows['InventoryButton']['widget'],
                      newParent=rows['FooterLayout']['widget'], childIndex=0)
    patch(moved_back['slot'], {'size': {'value':1,'sizeRule':'Automatic'},
                             'padding': {'left':0,'top':0,'right':12,'bottom':0},
                             'verticalAlignment':'VAlign_Center'})
    patch(rows['Header']['slot'], {'layoutData': {'offsets': {'top':32,'bottom':60}}})
    patch(rows['ChalkTitle']['slot'], {'padding': {'left':0,'top':0,'right':310,'bottom':0}})
    patch(rows['MapTabTitle']['widget'], {'text':'Map & journal'})
    patch(rows['MapTabTitle']['slot'], {'padding': {'left':0,'top':0,'right':0,'bottom':0}})
    after = m.capture(PAGE); current = m.rows(PAGE)
    assert set(current) == set(rows) | NEW
    assert all(current[n]['widget'] == r['widget'] for n,r in rows.items())
    s.write(out/'applied.json', {'backup': str(folder), 'before': before, 'after': after,
                                'new_widgets': sorted(NEW), 'runtime_verified':False})
    s.write(out/'started.json', {'backup':str(folder), 'complete':True})
    return {'asset':PAGE,'native_compile_then_save_required':True,'callbacks_and_widget_objects_preserved':True}


def verify():
    m = support(); s = m.S; rows = m.rows(PAGE)
    assert rows['DetailsScroll']['parent'] == rows['FNDetailsBody']['widget']
    assert rows['FNDetailsBody']['namedSlotHost'] == rows['FNSelectedTaskDetails']['widget']
    assert rows['FNDetailsHeader']['namedSlotHost'] == rows['FNSelectedTaskDetails']['widget']
    assert rows['InventoryButton']['parent'] == rows['FooterLayout']['widget']
    values = lambda name: s.read_object(s.resolve(rows[name]['widget']))['values']
    assert values('FNSelectedTaskDetails')['bIsExpanded'] is False
    assert values('FNDetailsBody')['maxDesiredHeight'] == 180
    assert s.read_object(s.resolve(rows['QuestList']['slot']))['values']['size'] == {'value':1,'sizeRule':'Fill'}
    return s.write(s.OUT/'JournalLayout/verified.json', {'asset':PAGE,'description_accessible':True,
                     'track_button_preserved':True,'list_fills_remaining_height':True,'runtime_verified':False})
