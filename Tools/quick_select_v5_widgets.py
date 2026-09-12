"""Apply the approved native GIMP V5 geometry to existing selector widgets.

Run via ProgrammaticToolset.execute_tool_script. Default MODE='audit' only
reads. 'author' requires a verified package backup and stopped PIE. Compile
the returned existing widgets with the dedicated UMG tool, then explicitly
save those packages and run 'verify'. No trees, bindings, gameplay, input,
inventory widgets, or native source are replaced. No tests are created/run.
"""
import copy
import json

MODE = 'audit'
BACKUP_READY = False
BASE = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect/'
OUT = 'C:/UnrealEngine/Games/AZ/Saved/QuickSelectV5/'
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
ASSET = 'editor_toolset.toolsets.asset.AssetTools.'
EDITOR = 'EditorToolset.EditorAppToolset.'
NAMES = {
    'root': 'WBP_AZ_QuickSelect', 'entry': 'WBP_AZ_QuickSelectEntry',
    'mode': 'WBP_AZ_QuickSelectFists', 'item': 'WBP_AZ_QuickSelectItemDetails',
    'name': 'WBP_AZ_QuickSelectNameLeaf', 'focus': 'WBP_AZ_QuickSelectFocusDetails',
    'focus_name': 'WBP_AZ_QuickSelectFocusNameLeaf',
    'description': 'WBP_AZ_QuickSelectDescriptionLeaf',
}
ORDER = ['name', 'focus_name', 'description', 'item', 'focus', 'entry', 'mode', 'root']
# Cross local origin maps to (1084,226) at 1920x1080, arrows to (1368,468).
BOXES = {
    'LeftSlot': (116,206,104,72), 'LeftSlotSecond': (0,206,104,72),
    'UpSlot': (232,122,104,72), 'UpSlotSecond': (232,38,104,72),
    'RightSlot': (348,206,104,72), 'RightSlotSecond': (464,206,104,72),
    'DownSlot': (232,290,104,72), 'DownSlotSecond': (232,374,104,72),
}
ARROWS = {'ArrowUp': (280,218,8,6), 'ArrowDown': (280,260,8,6),
          'ArrowLeft': (244,238,6,8), 'ArrowRight': (318,238,6,8)}


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def call(prefix, name, **args):
    return execute_tool(prefix + name, json.dumps(args)).get('returnValue')


def ref(path):
    return {'refPath': path}


def asset(key):
    name = NAMES[key]
    return BASE + name + '.' + name


def merge(before, changes):
    result = copy.deepcopy(before)
    for key, value in changes.items():
        result[key] = merge(result[key], value) if isinstance(value, dict) and isinstance(result.get(key), dict) else copy.deepcopy(value)
    return result


def matches(actual, expected):
    if isinstance(expected, dict):
        return isinstance(actual, dict) and all(k in actual and matches(actual[k], v) for k,v in expected.items())
    if isinstance(expected, (int, float)) and not isinstance(expected, bool):
        return isinstance(actual, (int,float)) and abs(actual - expected) < .0001
    return actual == expected


def props(obj, names):
    schema = json.loads(call(OBJ, 'list_properties', instance=obj))
    require(not set(names) - set(schema), 'Unknown properties: ' + str(set(names) - set(schema)))
    return json.loads(call(OBJ, 'get_properties', instance=obj, properties=list(names)))


def tree(key):
    return call(UMG, 'GetWidgets', widgetBlueprint=ref(asset(key)))


def rows(data):
    return {row['widgetName']: row for row in data['widgets'] if isinstance(row.get('widget'), dict)}


def slot(x,y,w,h,z=0,anchor=(0,0),align=(0,0)):
    return {'layoutData': {'offsets': {'left':x,'top':y,'right':w,'bottom':h},
            'anchors': {'minimum': {'x':anchor[0],'y':anchor[1]}, 'maximum': {'x':anchor[0],'y':anchor[1]}},
            'alignment': {'x':align[0],'y':align[1]}}, 'bAutoSize':False,'zOrder':z}


def make_plan():
    plan = []
    def change(key,name,target,values):
        plan.append({'asset':key,'widget':name,'target':target,'values':values})
    def box(key,name,xywh,z=0):
        change(key,name,'slot',slot(*xywh,z=z))
    def text(key,name,px,xywh=None,align=None):
        values={'font':{'size':px*.75},'autoWrapText':False,'textOverflowPolicy':'Ellipsis'}
        if align: values['justification']=align
        change(key,name,'widget',values)
        if xywh: box(key,name,xywh,5)
    change('root','CrossLayout','slot',slot(0,0,568,640,anchor=(.7125,468/1080),align=(.5,242/640)))
    change('root','CrossLayout','widget',{'renderTransform':{'scale':{'x':1,'y':1}},'renderTransformPivot':{'x':.5,'y':242/640}})
    for name, bounds in BOXES.items():
        box('root',name+'Scale',bounds,2)
        change('root',name,'widget',{'widthOverride':104,'heightOverride':72,
                                   'bOverride_WidthOverride':True,'bOverride_HeightOverride':True})
    # Arrow textures are rotated square widgets. Their 8x8 bounds have equal
    # 24px clearance to the cards; existing rotations/resources stay intact.
    for name, bounds in ARROWS.items():
        x,y,w,h=bounds
        if name in ('ArrowLeft','ArrowRight'):
            box('root',name,(x if name=='ArrowLeft' else x-2,y,8,8),3)
        else:
            box('root',name,(x,y if name=='ArrowUp' else y-2,8,8),3)
        change('root',name,'widget',{'brush':{'imageSize':{'x':8,'y':8}}})
    for name in ('CenterSlot','ModeText'):
        change('root',name,'widget',{'visibility':'Collapsed'})
    change('root','HeaderText','widget',{'visibility':'HitTestInvisible','text':'QUICK SELECT'})
    text('root','HeaderText',16,(0,0,568,24),'Center')
    box('root','FocusDetails',(0,480,568,84),4)
    text('root','FocusNameText',30,(0,480,568,39),'Center')
    text('root','FocusDescriptionText',18,(0,523,568,40),'Center')
    change('root','FocusDescriptionText','widget',{'autoWrapText':True})
    text('root','HintText',15,(0,566,568,36),'Center')
    change('root','HintText','widget',{'autoWrapText':True})
    text('root','StatusText',15,(0,608,568,32),'Center')
    # Every physical card keeps its existing real inventory composite.
    text('entry','NameText',15,(8,4,68,20),'Left')
    text('entry','KeyText',15,(80,5,16,20),'Right')
    text('entry','AmmoText',16,(54,44,42,22),'Right')
    text('entry','StateText',10,(8,50,44,15),'Left')
    text('entry','EmptyMarkText',16,(38,25,28,24),'Center')
    box('entry','IconScale',(8,19,88,28),3)
    box('item','ItemName',(8,4,68,20))
    box('item','IconScale',(8,19,88,28))
    text('name','Text_LeafText',15)
    # Raised 3px from approved draft: ammo y47->44, mode label y50->47.
    box('mode','IconScale',(31,7,42,39),3)
    text('mode','NameText',15,(8,47,88,20),'Center')
    text('mode','KeyText',15,(80,5,16,20),'Right')
    box('focus','ItemName',(0,0,568,39))
    box('focus','ItemDescription',(0,43,568,40))
    text('focus_name','Text_LeafText',30)
    text('description','Text_LeafText',18)
    change('description','Text_LeafText','widget',{'autoWrapText':True})
    # Combine successive changes to the same object before writing/verifying;
    # wrapping descriptions intentionally overrides the shared text defaults.
    combined = {}
    for item in plan:
        identity = (item['asset'], item['widget'], item['target'])
        if identity not in combined:
            combined[identity] = copy.deepcopy(item)
        else:
            combined[identity]['values'] = merge(combined[identity]['values'], item['values'])
    return list(combined.values())


def inspect(plan):
    trees = {key:tree(key) for key in NAMES}
    require(trees['root']['info']['parentClass']==ref('/Script/AZ.AZ_QuickSelectWidget'),'Unexpected root parent')
    for key in ('entry','mode'):
        require(trees[key]['info']['parentClass']==ref('/Script/AZ.AZ_QuickSelectEntryWidget'),'Unexpected entry parent')
    for key in ('item','focus'):
        require(trees[key]['info']['parentClass']==ref('/Script/AZ.AZ_Inv_CommonUI_CompositeWidget'),'Inventory composite contract differs')
    values=[]
    for item in plan:
        widget=rows(trees[item['asset']]).get(item['widget'])
        require(widget and isinstance(widget.get(item['target']),dict),'Missing authored widget/slot: '+str(item))
        values.append(props(widget[item['target']],item['values']))
    return trees,values


def run():
    require(MODE in ('audit','author','verify'),'Unknown mode')
    plan=make_plan()
    trees,before=inspect(plan)
    if MODE=='audit':
        call(ASSET,'write_file',file_path=OUT+'v5-plan.json',content=json.dumps({'plan':plan,'before':before},indent=2))
        return {'status':'read_only_plan_ready','changes':len(plan),'assets':[asset(k) for k in ORDER]}
    if MODE=='author':
        require(BACKUP_READY,'Verified package backups required')
        require(not call(EDITOR,'IsPIERunning'),'Stop PIE before modifying widgets')
        backup=json.loads(call(ASSET,'read_file',file_path=OUT+'backup-receipt.json').lstrip('\ufeff'))
        require(len(backup['assets'])==len(NAMES),'Incomplete backup receipt')
        call(ASSET,'write_file',file_path=OUT+'v5-author-before.json',content=json.dumps({'plan':plan,'before':before,'trees':trees},indent=2))
        for item in plan:
            obj=rows(trees[item['asset']])[item['widget']][item['target']]
            current=props(obj,item['values'])
            require(call(OBJ,'set_properties',instance=obj,values=json.dumps(merge(current,item['values']))),'Widget property update failed')
    after_trees,actual=inspect(plan)
    for item,value in zip(plan,actual):
        require(matches(value,item['values']),'Layout readback differs: '+str(item))
    require(trees==after_trees,'Widget hierarchy, variable flags or native binding identities changed')
    report={'status':'authored_requires_compile_save' if MODE=='author' else 'v5_layout_readback_passed',
            'assets':[asset(k) for k in ORDER], 'plan':plan,'actual':actual,
            'cell_size':[104,72],'center_1920x1080':[1368,468],'cross_bounds':[568,408],
            'card_gap':12,'arrow_clearance':24,'text_lift_px':3,
            'scope':'Existing selector presentation assets only; gameplay, inputs and inventory widgets preserved'}
    call(ASSET,'write_file',file_path=OUT+'v5-'+MODE+'.json',content=json.dumps(report,indent=2))
    return {k:v for k,v in report.items() if k not in ('plan','actual')}
