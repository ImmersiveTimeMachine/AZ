# @Description: Audit or author rifle magazine reload ability, four pose variants and R input.
"""Ordinary Unreal Python. Audit by default; assign follows full native build.
Compile the returned full object paths natively after return, save only returned
packages, then verify. Uses detached manifests; no PIE, tests or source-clip edits.
"""
import gc
import json
import runpy
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT=Path('C:/UnrealEngine/Games/AZ')
OUTPUT=ROOT/'Saved/RifleReload'
RIFLE='/Game/AZ/Blueprints/Items/Equippables/Weapons/BPAZ_CommonUI_PickupItem'
ABILITY='/Game/AZ/Blueprints/AbilitySystem/Hero/Abilities/BP_AZ_GA_FirearmReload'
PROFILE='/Game/AZ/Blueprints/Animation/MotionMatching/RifleP01/DA_WeaponAnim_P01'
INPUT='/Game/AZ/Blueprints/Input/AZ_InputConfig'
ACTION='/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IA_RT_Reload'
MAP='/Game/AZ/Maps/L_001'
OWNER_KEY='AZ.RifleReload.Owner'
OWNER='rifle_reload_setup:v1'
CLIPS={
    'standing_reload_animation':'AZ_RTG_MH_W2_Stand_Relaxed_Reload_IPC',
    'standing_aim_reload_animation':'AZ_RTG_MH_W2_Stand_Aim_Reload_IPC',
    'crouching_reload_animation':'AZ_RTG_MH_W2_Crouch_Rlx_Reload',
    'crouching_aim_reload_animation':'AZ_RTG_MH_W2_Crouch_Aim_Reload_IPC',
}


def require(condition,message):
    if not condition: raise RuntimeError(message)


def patch(H,text):
    fields=H['fields'](text);parts=H['split_top_level'](H['parenthesized'](dict(fields)['Fragments']))
    expected=[H['object_reference']('/Script/Engine.BlueprintGeneratedClass',p,True)
              for p in [H['ABILITY'],H['FIRE_ABILITY'],ABILITY]]
    result=[];count=0
    for fragment in parts:
        kind,values=H['fragment_parts'](fragment)
        if kind==H['GRANT_FRAGMENT']:
            count+=1
            grants=H['split_top_level'](H['parenthesized'](dict(values).get('AbilitiesToGrant','()')))
            require(all(grants.count(g)==1 for g in expected[:2]),'Working aim/fire grants must exist once')
            require(len(grants)==len(set(grants)) and all(g in expected for g in grants),'Unknown or duplicate grants')
            if expected[2] not in grants: grants.append(expected[2])
            fragment=kind+H['encode_fields'](H['replace_field'](values,'AbilitiesToGrant','('+','.join(grants)+')'))
        result.append(fragment)
    require(count==1,'Expected one ability-grant fragment')
    return H['encode_fields'](H['replace_field'](fields,'Fragments','('+','.join(result)+')'))


def retained(H,text):
    fields=dict(H['fields'](text));parts=H['split_top_level'](H['parenthesized'](fields.pop('Fragments')));result=[]
    for fragment in parts:
        kind,values=H['fragment_parts'](fragment)
        if kind==H['GRANT_FRAGMENT']:
            fragment=kind+H['encode_fields']([(k,v) for k,v in values if k!='AbilitiesToGrant'])
        result.append(fragment)
    return fields,result


def readback_context():
    # Exact saved editor-object paths remain readable while the user is in PIE;
    # GetEditorWorld may return None there. Never inspect/mutate the PIE clones.
    baseline=json.loads((OUTPUT/'baseline.json').read_text(encoding='utf-8'))
    components=[unreal.load_object(None,row['component']) for row in baseline['components']]
    require(all(components),'A saved rifle component no longer resolves')
    return {'components':components}


def main(mode='audit'):
    try:
        require(mode in ['audit','assign','verify'],'Unknown mode')
        H=runpy.run_path(str(ROOT/'Tools/rifle_p01_activate.py'),run_name='reload_helpers')
        S=runpy.run_path(str(ROOT/'Tools/rifle_recoil_setup.py'),run_name='reload_context')
        C=readback_context() if mode=='verify' else S['context'](H)
        before=[S['capture'](c) for c in C['components']]
        profile=H['load'](PROFILE,unreal.AZ_WeaponAnimationProfile)
        clips={};durations={}
        for field,name in CLIPS.items():
            clip=H['load']('/Game/AZ/Assets/M16/Riffle_RTG_MH/'+name,unreal.AnimSequence)
            require(clip.get_editor_property('additive_anim_type')==unreal.AdditiveAnimationType.AAT_NONE
                    and not clip.get_editor_property('enable_root_motion'),'Reload clip semantics changed: '+name)
            require(clip.get_editor_property('rate_scale')>0,'Reload clip has invalid rate')
            clips[field]=clip;durations[field]=clip.get_editor_property('sequence_length')/clip.get_editor_property('rate_scale')
        config=H['load'](INPUT,unreal.AZ_InputConfig);action=H['load'](ACTION,unreal.InputAction)
        rows=list(config.get_editor_property('ability_input_actions'))
        tag=H['tag']('Input.Action.Reload')
        matches=[i for i,row in enumerate(rows) if str(row.get_editor_property('input_tag').get_editor_property('tag_name'))=='Input.Action.Reload']
        require(len(matches)<=1,'Duplicate Reload input rows')
        for record in before:
            require(retained(H,record['manifest'])==retained(H,patch(H,record['manifest'])),'Unrelated manifest field would change')
        packages=[ABILITY,PROFILE,INPUT,RIFLE,MAP]
        report={'status':'audit','native_ready':hasattr(unreal,'AZ_GA_FirearmReload'),
                'durations_at_rate_one':durations,'components':before,'save_packages':packages,
                'compile_after_return':[ABILITY+'.BP_AZ_GA_FirearmReload',RIFLE+'.BPAZ_CommonUI_PickupItem'],
                'reload_input_indexes':matches,'magazine_policy':'swap_fullest_preserve_rounds'}
        OUTPUT.mkdir(parents=True,exist_ok=True)
        if mode=='assign':
            require(report['native_ready'],'Full native build/restart required')
            require(not C['editor'].get_game_world(),'Stop PIE before reload authoring')
            dirty={p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()}
            require(not dirty.intersection(packages),'Target package has unsaved changes')
            existing=unreal.load_asset(ABILITY) if unreal.EditorAssetLibrary.does_asset_exist(ABILITY) else None
            if existing:
                require(unreal.EditorAssetLibrary.get_metadata_tag(existing,OWNER_KEY)==OWNER,'Unowned reload Blueprint exists')
            backup=ROOT/'Saved/Backups/RifleReload'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')
            for package in packages:
                file=ROOT/'Content'/(package.removeprefix('/Game/')+('.umap' if package==MAP else '.uasset'))
                if file.is_file():
                    dest=backup/file.relative_to(ROOT/'Content');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,dest)
            baseline={'components':before,'input_rows':[r.export_text() for r in rows],'input_indexes':matches,'backup':str(backup)}
            (OUTPUT/'baseline.json').write_text(json.dumps(baseline,indent=2),encoding='utf-8')
            if not existing:
                folder,name=ABILITY.rsplit('/',1);factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.AZ_GA_FirearmReload)
                existing=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.Blueprint,factory)
                require(existing,'Cannot create reload Blueprint')
                unreal.EditorAssetLibrary.set_metadata_tag(existing,OWNER_KEY,OWNER)
            cdo=unreal.get_default_object(existing.generated_class());require(isinstance(cdo,unreal.AZ_GA_FirearmReload),'Wrong reload parent')
            existing.modify();cdo.set_editor_property('input_tag',tag)
            cdo.set_editor_property('activation_required_tags',H['tags']('Weapon.Rifle'))
            cdo.set_editor_property('activation_owned_tags',H['tags']())
            cdo.set_editor_property('source_object_must_equal_current_weapon_to_activate',False)
            cdo.set_editor_property('activate_ability_on_granted',False)
            profile.modify()
            for field,clip in clips.items(): profile.set_editor_property(field,clip)
            for field,value in [('reload_animation_play_rate',1.0),('reload_animation_blend_in',0.1),('reload_animation_blend_out',0.15)]:
                profile.set_editor_property(field,value)
            newrow=unreal.AZ_InputAction();newrow.set_editor_property('input_action',action);newrow.set_editor_property('input_tag',tag)
            index=matches[0] if matches else len(rows)
            if matches: rows[index]=newrow
            else: rows.append(newrow)
            config.modify();config.set_editor_property('ability_input_actions',rows)
            C['bp'].modify();C['world'].modify()
            for component,record in zip(C['components'],before):
                detached=unreal.AZ_Inv_CommonUI_ItemManifest();require(detached.import_text(patch(H,record['manifest'])),'Cannot resolve reload grant')
                require(retained(H,record['manifest'])==retained(H,detached.export_text()),'Unrelated import change')
                component.modify();component.set_editor_property('pickup_item_manifest',detached,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
            report.update(status='authored_requires_native_compile_save',backup=str(backup))
        if mode!='audit':
            baseline=json.loads((OUTPUT/'baseline.json').read_text(encoding='utf-8'));originals={r['component']:r for r in baseline['components']}
            fresh_context=readback_context() if mode=='verify' else S['context'](H)
            fresh=[S['capture'](c) for c in fresh_context['components']]
            require(set(originals)=={r['component'] for r in fresh},'Rifle set changed')
            for record in fresh:
                old=originals[record['component']]
                require(retained(H,old['manifest'])==retained(H,record['manifest']),'Unrelated manifest state changed')
                require(old['contained']==record['contained'],'Initial magazine definitions changed')
                require(patch(H,record['manifest'])==record['manifest'],'Reload grant missing after readback')
            for field,clip in clips.items(): require(profile.get_editor_property(field)==clip,'Reload clip assignment differs')
            current=list(config.get_editor_property('ability_input_actions'));matching=[i for i,row in enumerate(current) if str(row.get_editor_property('input_tag').get_editor_property('tag_name'))=='Input.Action.Reload']
            require(len(matching)==1 and current[matching[0]].get_editor_property('input_action')==action,'Reload input readback failed')
            for i,old in enumerate(baseline['input_rows']):
                if i not in baseline['input_indexes']: require(current[i].export_text()==old,'Unrelated input row changed')
            report['components']=fresh
            if mode=='verify':
                dirty={p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()}
                require(not dirty.intersection(packages),'Reload packages still need saving')
                for objpath in report['compile_after_return']:
                    require(unreal.load_asset(objpath).get_editor_property('status')==unreal.BlueprintStatus.BS_UP_TO_DATE,'Reload Blueprint is not UpToDate')
                report['status']='verified_compiled_saved_reload'
        (OUTPUT/(mode+'-readback.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
        return report
    finally:
        gc.collect()
