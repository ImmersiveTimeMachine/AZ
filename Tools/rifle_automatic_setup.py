# @Description: Assign the requested hero fire animations, slot groups and fire-mode input after the native build.
"""Audit by default; main(prepare=True) writes only the explicit owned targets.
Compile/save graph and HUD separately through native tools. No PIE/tests.
"""
import gc
import json
import runpy
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
H = runpy.run_path(str(ROOT/'Tools/rifle_p01_activate.py'), run_name='rifle_automatic_helpers')
BP = '/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC'
PROFILE = H['PROFILE']
CONTROLLER = '/Game/AZ/Blueprints/Player/BP_AZ_PlayerController'
ACTION = '/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IA_RT_ChangeFireMode'
PRIMARY_ACTION = '/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IA_RT_PrimaryAttack'
CONTEXT = H['MAPPING_CONTEXT']
ANIMROOT = '/Game/AZ/Assets/M16/Riffle_RTG_MH/'
SINGLE = ANIMROOT+'AZ_RTG_MH_W2_Stand_Fire_Single_IPC'
AUTO = ANIMROOT+'AZ_RTG_MH_W2_Stand_Fire_Continuous'
BODY = '/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh'


def require(condition,message):
    if not condition:
        raise RuntimeError(message)


def prepare_primary_hold_input():
    """Run after the controller edge-routing fix has been built; touches only PrimaryAttack."""
    require(not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before input authoring')
    primary=H['load'](PRIMARY_ACTION,unreal.InputAction)
    require(primary.get_editor_property('value_type')==unreal.InputActionValueType.BOOLEAN,'Primary input type changed')
    require(not primary.get_editor_property('modifiers'),'Primary input has custom modifiers')
    triggers=list(primary.get_editor_property('triggers'))
    require(not triggers or (len(triggers)==1 and isinstance(triggers[0],unreal.InputTriggerPressed)), 'Primary input has custom triggers')
    dirty={p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    require(PRIMARY_ACTION not in dirty,'Primary input has unsaved changes')
    report={'primary_action':PRIMARY_ACTION,'before':[t.get_class().get_name() for t in triggers]}
    if triggers:
        require(abs(triggers[0].get_editor_property('actuation_threshold')-0.5)<0.0001,'Custom primary threshold needs review')
        backup=ROOT/'Saved/Backups/RifleAutomaticInput'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')
        backup.mkdir(parents=True,exist_ok=True)
        source=ROOT/'Content'/(PRIMARY_ACTION.removeprefix('/Game/')+'.uasset')
        shutil.copy2(source,backup/source.name)
        primary.set_editor_property('triggers',[])
        require(unreal.EditorAssetLibrary.save_asset(PRIMARY_ACTION,only_if_is_dirty=False),'Cannot save primary input')
        report['backup']=str(backup)
    require(not primary.get_editor_property('triggers'),'Primary input still has a pulse trigger')
    report.update(status='primary_hold_input_saved',after=[])
    folder=ROOT/'Saved/RifleAutomatic';folder.mkdir(parents=True,exist_ok=True)
    (folder/'primary-hold-input.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    gc.collect()
    return report


def main(prepare=False, key='X'):
    require(key in ['X','B','MiddleMouseButton'], 'Unsupported requested binding')
    editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(not prepare or not editor.get_game_world(), 'Stop PIE before authoring')
    single=H['load'](SINGLE,unreal.AnimSequence); auto=H['load'](AUTO,unreal.AnimSequence)
    for clip in [single,auto]:
        require(clip.get_editor_property('additive_anim_type')==unreal.AdditiveAnimationType.AAT_NONE,'Clip additive semantics changed')
        require(not clip.get_editor_property('enable_root_motion'),'Firing clip must not drive root motion')
    abp=H['load'](BP,unreal.AnimBlueprint)
    profile=H['load'](PROFILE,unreal.AZ_WeaponAnimationProfile)
    controller=H['load'](CONTROLLER,unreal.Blueprint)
    body=H['load'](BODY,unreal.SkeletalMesh)
    skeletons={H['package'](s):s for s in [abp.get_editor_property('target_skeleton'),body.get_editor_property('skeleton'),single.get_editor_property('skeleton'),auto.get_editor_property('skeleton')]}
    context=H['load'](CONTEXT,unreal.InputMappingContext)
    data=context.get_editor_property('default_key_mappings')
    mappings=list(data.get_editor_property('mappings'))
    action=H['load'](ACTION,unreal.InputAction)
    primary=H['load'](PRIMARY_ACTION,unreal.InputAction)
    primary_triggers=list(primary.get_editor_property('triggers'))
    require(not primary.get_editor_property('modifiers'),'Primary attack has custom modifiers; review before changing held input')
    require(not primary_triggers or (len(primary_triggers)==1 and isinstance(primary_triggers[0],unreal.InputTriggerPressed)),
            'Primary attack has custom triggers; review before changing held input')
    matching=[i for i,r in enumerate(mappings) if r.get_editor_property('action')==action]
    require(len(matching)==1,'Mode action must have one existing key mapping')
    report=dict(status='audit',single=SINGLE,automatic=AUTO,automatic_pulses=4,
                input_key=key,input_action=ACTION,slot='RifleFire',group='WeaponFire',
                skeletons=list(skeletons),native_ready=hasattr(unreal.AZ_SkeletonUtils,'set_animation_slot_group'))
    report['primary_action']=PRIMARY_ACTION
    report['primary_trigger_before']=[t.get_class().get_name() for t in primary_triggers]
    if prepare:
        require(report['native_ready'],'Full native build and editor restart required')
        packages=[BP,PROFILE,CONTROLLER,CONTEXT,ACTION,PRIMARY_ACTION]+list(skeletons)
        dirty={p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
        require(not dirty.intersection(packages),'Target asset has unsaved changes: '+str(dirty.intersection(packages)))
        backup=ROOT/'Saved/Backups/RifleAutomatic'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')
        for package in packages:
            file=ROOT/'Content'/(package.removeprefix('/Game/')+'.uasset')
            if file.exists():
                dest=backup/file.relative_to(ROOT/'Content');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,dest)
        report['backup']=str(backup)
        profile.set_editor_property('single_fire_animation',single)
        profile.set_editor_property('automatic_fire_animation',auto)
        profile.set_editor_property('fire_animation_slot','RifleFire')
        profile.set_editor_property('fire_animation_blend_in',0.04)
        profile.set_editor_property('fire_animation_blend_out',0.08)
        profile.set_editor_property('automatic_fire_animation_shots_per_cycle',4)
        for skeleton in skeletons.values():
            require(unreal.AZ_SkeletonUtils.set_animation_slot_group(skeleton,'RifleFire','WeaponFire'),'Slot registration failed')
            require(str(unreal.AZ_SkeletonUtils.get_animation_slot_group(skeleton,'RifleFire'))=='WeaponFire','Slot readback failed')
        cdo=unreal.get_default_object(controller.generated_class())
        cdo.set_editor_property('change_fire_mode_action',action)
        # Pressed completes on the NEXT FRAME while the button is still held.
        # Default Boolean evaluation keeps the action active until physical release.
        # PC owns primary's single activation edge (including the existing melee buffer).
        primary.set_editor_property('triggers',[])
        require(not primary.get_editor_property('triggers'),'Primary held-input readback failed')
        # Started is the sole mode-change event. Keep the existing action semantics.
        row=mappings[matching[0]]
        require(not row.get_editor_property('triggers') and not row.get_editor_property('modifiers'),'Custom mapping triggers need review')
        if H['mapping_key'](row)!=key:
            # A fresh struct is required: editing a struct-array element in place may silently do nothing.
            replacement=unreal.EnhancedActionKeyMapping()
            require(replacement.import_text(row.export_text()),'Cannot copy mode mapping')
            key_value=unreal.Key();require(key_value.import_text(key),'Unknown key')
            replacement.set_editor_property('key',key_value);mappings[matching[0]]=replacement
            if key=='MiddleMouseButton':
                mappings=[r for r in mappings if r.get_editor_property('action')==action or H['mapping_key'](r)!='MiddleMouseButton']
            else:
                require(not any(H['mapping_key'](r)==key and r.get_editor_property('action')!=action for r in mappings),'Key is already assigned')
            data.set_editor_property('mappings',unreal.Array(unreal.EnhancedActionKeyMapping,mappings))
            context.set_editor_property('default_key_mappings',data)
        for path in [PROFILE,CONTROLLER,CONTEXT,PRIMARY_ACTION]+list(skeletons):
            require(unreal.EditorAssetLibrary.save_asset(path,only_if_is_dirty=False),'Cannot save '+path)
        report['status']='profile_input_and_groups_saved'
    folder=ROOT/'Saved/RifleAutomatic';folder.mkdir(parents=True,exist_ok=True)
    (folder/('prepared.json' if prepare else 'audit.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
    gc.collect()
    return report
