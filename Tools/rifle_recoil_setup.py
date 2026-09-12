# @Description: Audit or assign exposed recoil tuning to the existing rifle manifests.
"""Ordinary editor Python. main() audits; main('assign') follows the full native build.

Only the Recoil field changes. Native Blueprint compilation and explicit package
saves happen after this script returns, then main('verify') reads fresh components.
No PIE, gameplay tests, input/animation edits or inventory UI reconstruction.
"""
import gc
import hashlib
import json
import runpy
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUTPUT = ROOT/'Saved/RifleRecoil'
RIFLE = '/Game/AZ/Blueprints/Items/Equippables/Weapons/BPAZ_CommonUI_PickupItem'
MAP = '/Game/AZ/Maps/L_001'
LEAF = '/Game/AZ/Blueprints/Menu/HUD/Reticles/WBP_AZ_Reticle_Rifle'
HUD = '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD'
TUNING = {
    'bEnabled':'True',
    'SpreadRadiusPerShotDegrees':'0.080000',
    'MaxAdditionalSpreadRadiusDegrees':'1.000000',
    'SpreadRecoveryDelaySeconds':'0.150000',
    'SpreadRecoverySpeedDegreesPerSecond':'1.250000',
    'bCameraKickEnabled':'True',
    'CameraPitchKickDegrees':'0.550000',
    'CameraYawKickRadiusDegrees':'0.180000',
    'MaxCameraPitchDegrees':'5.000000',
    'MaxCameraYawDegrees':'1.500000',
    'CameraRecoveryDelaySeconds':'0.120000',
    'CameraRecoverySpeedDegreesPerSecond':'7.000000',
}


def require(condition,message):
    if not condition:
        raise RuntimeError(message)


def helpers():
    return runpy.run_path(str(ROOT/'Tools/rifle_p01_activate.py'),run_name='rifle_recoil_helpers')


def capture(component):
    return {'component':component.get_path_name(),
            'manifest':component.get_editor_property('pickup_item_manifest').export_text(),
            'contained':[m.export_text() for m in component.get_editor_property('initial_contained_item_manifests')]}


def context(H):
    editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world=editor.get_editor_world()
    require(world is not None and world.get_path_name().split('.')[0]==MAP,'Open L_001 in the editor to inspect its existing rifle overrides')
    bp=H['load'](RIFLE,unreal.Blueprint)
    components=[H['item_component_template'](bp)]
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if actor.get_class()==bp.generated_class():
            components.append(actor.get_component_by_class(unreal.AZ_Inv_CommonUI_ItemComponent))
    require(len(components)==3 and all(components),'Rifle instance set changed; inspect before authoring')
    return {'editor':editor,'world':world,'bp':bp,'components':components}


def patch(H,text):
    values=H['fields'](text)
    fragments=H['split_top_level'](H['parenthesized'](dict(values)['Fragments']))
    result=[];count=0
    for fragment in fragments:
        kind,fields=H['fragment_parts'](fragment)
        if kind==H['WEAPON_FRAGMENT']:
            count+=1
            require(dict(fields).get('bUsesDetachableMagazines')=='True','Expected the existing magazine-based rifle')
            fields=H['replace_field'](fields,'Recoil',H['encode_fields'](list(TUNING.items())))
            fragment=kind+H['encode_fields'](fields)
        result.append(fragment)
    require(count==1,'Expected one weapon-state fragment')
    return H['encode_fields'](H['replace_field'](values,'Fragments','('+','.join(result)+')'))


def retained(H,text):
    values=dict(H['fields'](text));fragments=H['split_top_level'](H['parenthesized'](values.pop('Fragments')))
    result=[]
    for fragment in fragments:
        kind,fields=H['fragment_parts'](fragment)
        if kind==H['WEAPON_FRAGMENT']:
            fragment=kind+H['encode_fields']([(k,v) for k,v in fields if k!='Recoil'])
        result.append(fragment)
    return values,result


def tuning_readback(H,text):
    fragments=H['split_top_level'](H['parenthesized'](dict(H['fields'](text))['Fragments']))
    values=next(fields for kind,fields in map(H['fragment_parts'],fragments) if kind==H['WEAPON_FRAGMENT'])
    recoil=dict(H['fields'](dict(values)['Recoil']))
    for key,expected in TUNING.items():
        if expected in ('True','False'):
            require(recoil.get(key)==expected,'Boolean recoil readback mismatch: '+key)
        else:
            require(abs(float(recoil[key])-float(expected))<0.00001,'Recoil tuning readback mismatch: '+key)
    return recoil


def main(mode='audit'):
    try:
        require(mode in ['audit','assign','verify'],'Unknown recoil setup mode')
        H=helpers();C=context(H)
        before=[capture(c) for c in C['components']]
        report={'status':'audit','native_ready':hasattr(unreal,'AZ_FirearmRecoilSettings'),
                'components':before,'tuning':TUNING,'save_packages':[RIFLE,MAP,LEAF],
                'compile_after_return':[RIFLE+'.BPAZ_CommonUI_PickupItem',LEAF+'.WBP_AZ_Reticle_Rifle']}
        for r in before:
            require(retained(H,r['manifest'])==retained(H,patch(H,r['manifest'])),'Unrelated manifest field would change')
        OUTPUT.mkdir(parents=True,exist_ok=True)
        if mode=='assign':
            require(report['native_ready'],'Full native build and editor restart required')
            require(not C['editor'].get_game_world(),'Stop PIE before authoring recoil defaults')
            owned=[RIFLE,MAP,LEAF,HUD]
            dirty={p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()}
            require(not dirty.intersection(owned),'A target asset has unsaved changes: '+str(dirty.intersection(owned)))
            backup=ROOT/'Saved/Backups/RifleRecoil'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')
            files=[]
            for package in owned:
                source=ROOT/'Content'/(package.removeprefix('/Game/')+('.umap' if package==MAP else '.uasset'))
                require(source.is_file(),'Missing target file: '+str(source))
                dest=backup/source.relative_to(ROOT/'Content');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)
                files.append({'package':package,'file':str(source),'backup':str(dest),'sha256':hashlib.sha256(source.read_bytes()).hexdigest()})
            baseline={'components':before,'backup':str(backup),'files':files}
            (OUTPUT/'baseline.json').write_text(json.dumps(baseline,indent=2),encoding='utf-8')
            candidates=[]
            for record in before:
                detached=unreal.AZ_Inv_CommonUI_ItemManifest()
                require(detached.import_text(patch(H,record['manifest'])),'Cannot import detached recoil manifest')
                require(retained(H,record['manifest'])==retained(H,detached.export_text()),'Unrelated field changed in roundtrip')
                tuning_readback(H,detached.export_text());candidates.append(detached)
            C['bp'].modify();C['world'].modify()
            for component,manifest in zip(C['components'],candidates):
                component.modify()
                component.set_editor_property('pickup_item_manifest',manifest,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
            report.update(status='authored_requires_native_compile_save',backup=str(backup))
        if mode!='audit':
            baseline=json.loads((OUTPUT/'baseline.json').read_text(encoding='utf-8'))
            originals={r['component']:r for r in baseline['components']}
            fresh=[capture(c) for c in context(H)['components']]
            require(set(originals)=={r['component'] for r in fresh},'Rifle components changed after authoring')
            for record in fresh:
                old=originals[record['component']]
                require(retained(H,old['manifest'])==retained(H,record['manifest']),'Unrelated field changed')
                require(old['contained']==record['contained'],'Contained magazine defaults changed')
                tuning_readback(H,record['manifest'])
            report['components']=fresh
            if mode=='verify':
                dirty={p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()}
                require(not set(report['save_packages']).intersection(dirty),'Targets still need saving')
                report['status']='verified_saved_recoil_defaults'
        (OUTPUT/(mode+'-readback.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
        return report
    finally:
        gc.collect()
