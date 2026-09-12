"""ProgrammaticToolset script. Audit by default; author adds the mode label only.
Back up WBP_AZ_GameHUD before authoring; compile/save through native tools after.
"""
import copy
import json

MODE = 'audit'
HUD = '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD.WBP_AZ_GameHUD'
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'


def call(prefix, name, **args):
    return execute_tool(prefix + name, json.dumps(args))['returnValue']


def ref(path):
    return {'refPath':path}


def read(obj, names):
    schema=json.loads(call(OBJ,'list_properties',instance=obj))
    assert all(name in schema for name in names), names
    return json.loads(call(OBJ,'get_properties',instance=obj,properties=names))


def set_props(obj, values):
    read(obj,list(values))
    assert call(OBJ,'set_properties',instance=obj,values=json.dumps(values))


def run():
    tree=call(UMG,'GetWidgets',widgetBlueprint=ref(HUD))
    widgets={w['widgetName']:w for w in tree['widgets']}
    name=widgets['WeaponNameText']
    style=read(name['widget'],['font','colorAndOpacity','justification','visibility'])
    mode=widgets.get('FireModeText')
    has_widget=mode is not None and isinstance(mode.get('widget'),dict)
    result={'status':'audit','exists':has_widget,'parent':widgets['WeaponContainer']['widget']}
    if MODE=='author':
        # An unbound native BindWidgetOptional appears as a tree entry with
        # widget=None. It is a property placeholder, not an authored UTextBlock.
        if not has_widget:
            mode=call(UMG,'AddWidget',widgetBlueprint=ref(HUD),widgetClass=ref('/Script/UMG.TextBlock'),
                      widgetDisplayName='FireModeText',parentWidget=widgets['WeaponContainer']['widget'])
        values=copy.deepcopy(style)
        values.update(text='',justification='Right',visibility='Collapsed')
        values['font']['size']=13
        set_props(mode['widget'],values)
        slot=read(mode['slot'],['layoutData','bAutoSize','zOrder'])
        slot['layoutData']={'offsets':{'left':200,'top':119,'right':108,'bottom':24},
                            'anchors':{'minimum':{'x':0,'y':0},'maximum':{'x':0,'y':0}},
                            'alignment':{'x':0,'y':0}}
        slot['bAutoSize']=False
        set_props(mode['slot'],slot)
        result.update(status='authored_requires_compile_save',widget=mode,
                      properties=read(mode['widget'],['text','font','justification','visibility']))
    return result
