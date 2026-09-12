"""ProgrammaticToolset script: bind existing recoil crosshair images; default audit.

Run MODE='author' after backing up the leaf and loading the rebuilt native class.
This does not recreate widgets, change artwork/layout, compile, save or start PIE.
"""
import json

MODE = 'audit'
LEAF = '/Game/AZ/Blueprints/Menu/HUD/Reticles/WBP_AZ_Reticle_Rifle.WBP_AZ_Reticle_Rifle'
HUD = '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD.WBP_AZ_GameHUD'
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
NAMES = ['ArmUp','ArmDown','ArmLeft','ArmRight','OutlineUp','OutlineDown','OutlineLeft','OutlineRight']


def call(prefix,name,**args):
    return execute_tool(prefix+name,json.dumps(args)).get('returnValue')


def ref(path):
    return {'refPath':path}


def read(obj,names):
    schema=json.loads(call(OBJ,'list_properties',instance=obj))
    assert all(name in schema for name in names), names
    return json.loads(call(OBJ,'get_properties',instance=obj,properties=names))


def widgets(path):
    return {w['widgetName']:w for w in call(UMG,'GetWidgets',widgetBlueprint=ref(path))['widgets']
            if isinstance(w['widget'],dict)}


def run():
    leaf=widgets(LEAF);hud=widgets(HUD)
    assert all(n in leaf for n in NAMES), 'Existing reticle arm names changed'
    assert 'ReticleHost' in hud, 'Reticle host missing'
    before={}
    for name,w in leaf.items():
        fields=['clipping','renderTransform']
        if name in NAMES:
            fields.append('brush')
        before[name]=read(w['widget'],fields)
    host=read(hud['ReticleHost']['widget'],['clipping'])
    assert host['clipping']=='Inherit','ReticleHost custom clipping needs explicit review'
    if MODE=='author':
        for name in NAMES:
            call(UMG,'ToggleWidgetAsVariable',widgetBlueprint=ref(LEAF),widget=leaf[name]['widget'],bIsVariable=True)
        for name,w in leaf.items():
            if before[name]['clipping']!='Inherit':
                assert call(OBJ,'set_properties',instance=w['widget'],values=json.dumps({'clipping':'Inherit'}))
    after=widgets(LEAF)
    assert set(after)==set(leaf),'Recoil setup must not add or remove reticle widgets'
    for name,w in after.items():
        values=read(w['widget'],list(before[name]))
        assert {k:v for k,v in values.items() if k!='clipping'}=={k:v for k,v in before[name].items() if k!='clipping'},'Reticle artwork/transform changed'
        if MODE=='author':
            assert values['clipping']=='Inherit','Reticle clipping readback failed'
            if name in NAMES:
                assert w['bIsVariable'],'Native arm binding still disabled: '+name
    return {'status':'authored_requires_native_compile_save' if MODE=='author' else 'audit',
            'leaf':LEAF,'widget_count':len(after),'host_clipping':host['clipping'],
            'bindings':{n:after[n]['bIsVariable'] for n in NAMES}}
