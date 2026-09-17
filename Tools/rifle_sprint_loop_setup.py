"""Rifle-only sprint loop replacement; preserves other chooser selections.

Author only while PIE is stopped. No AnimBP changes, C++ compile, PIE or tests.
"""
import json
import re
import runpy
import shutil
import hashlib
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT=Path('C:/UnrealEngine/Games/AZ')
OUT=ROOT/'Saved/RifleSprintReplacement'
MAIN='/Game/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations'
SEQ='/Game/AZ/Assets/M16/Riffle_RTG_MH/AZ_RTG_MH_Rifle_SprintLoop'
DB='/Game/AZ/Blueprints/Animation/MotionMatching/RifleP01/PSD_Rifle_MH_Sprint'
SCHEMA='/Game/AZ/Blueprints/Animation/MotionMatching/PSS_v2_SurvivalMan_Loco'
OLD='/Game/Assets/RTG_AZ/MovementAnimsetPro/AnimPro_SprintFwdLoop1'
CU=unreal.AZ_ChooserUtils
PU=unreal.AZ_PoseSearchUtils
EAL=unreal.EditorAssetLibrary


def require(ok,msg):
    if not ok:raise RuntimeError(msg)


def helpers():
    return [runpy.run_path(str(ROOT/'Tools'/n),run_name='rifle_sprint_dependency') for n in
            ['rifle_p01_setup.py','rifle_p01_activate.py','rifle_p01_transition_setup.py','rifle_p01_pivot_setup.py']]


def pkg(obj):return obj.get_path_name().split('.')[0]


def idle():
    require(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None,
            'PIE active; no sprint asset mutations')


def save(obj):
    require(EAL.save_loaded_asset(obj,False),'Asset save failed: '+pkg(obj))


def snapshot():
    a=unreal.load_asset(MAIN)
    result_lines=[str(x) for x in CU.dump_chooser_full_tree(MAIN) if re.match(r'\s*\{\s*"i"\s*:',str(x))]
    results=[re.search(r'"out":\s*"([^"]*)"',line).group(1) for line in result_lines]
    return {'columns':[x.export_text() for x in a.get_editor_property('columns_structs')],
            'results':results,
            'disabled':list(CU.get_chooser_disabled_rows(MAIN))}


def verify():
    baseline=json.loads((OUT/'asset-before.json').read_text(encoding='utf-8'))
    meta=baseline['meta'];base=meta['count'];src=meta['source_row'];pos=meta['positive'];neg=meta['negative']
    setup,text,transition,pivot=helpers()
    after=snapshot();require(len(after['results'])==base+1,'Unexpected row count')
    require(after['results'][:base]==baseline['results'],'An existing chooser result changed')
    require(after['disabled'][:base]==baseline['disabled'],'An existing row enabled state changed')
    a=unreal.load_asset(MAIN)
    oldcols=[]
    for encoded in baseline['columns']:
        c=unreal.InstancedStruct();require(c.import_text(encoded),'Baseline parse failed');oldcols.append(c)
    cols=list(a.get_editor_property('columns_structs'))
    require(len(cols)==len(oldcols),'Column structure changed')
    for i,(old,new) in enumerate(zip(oldcols,cols)):
        oldrows=transition['column_data'](old,text)[3]
        desired=list(oldrows)+[oldrows[src]]
        if i==pos:desired[base]='(GameplayTags=((TagName="Weapon.Rifle")))'
        if i==neg:
            desired[src]='(GameplayTags=((TagName="Weapon.Rifle")))'
            desired[base]='()'
        expected=transition['with_rows'](old,desired,text).export_text()
        require(new.export_text()==expected,'Unexpected chooser cell/property change in column '+str(i))
    require(after['results'][base]=='Asset[AnimSequence]:'+SEQ.rsplit('/',1)[-1],'New rifle result differs')
    seq=unreal.load_asset(SEQ);db=unreal.load_asset(DB)
    require(seq.get_editor_property('loop') and float(seq.get_editor_property('rate_scale'))==1.,'Loop/rate mismatch')
    require(not seq.get_editor_property('enable_root_motion'),'In-place motion policy changed')
    require(db and db.get_num_animation_assets()==1 and pkg(db.get_animation_asset(0))==SEQ,'Database membership differs')
    branches=setup['branch_notifies'](seq)
    require(len(branches)==1 and branches[0]['database']==DB,'BranchIn binding differs')
    report={'status':'verified','row_count':base+1,'shared_row':src,'rifle_row':base,
            'rifle_sequence':SEQ,'database':DB,'preserved_results':base,'play_rate':1.0,
            'contact_policy':'conservative unplanted: new in-place clip has no corroborated contact intervals'}
    (OUT/'verified.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    return report


def author():
    idle();OUT.mkdir(exist_ok=True)
    setup,text,transition,pivot=helpers()
    a=unreal.load_asset(MAIN);seq=unreal.load_asset(SEQ)
    require(a and seq,'Missing chooser or new sequence')
    baseline=snapshot();base=len(baseline['results'])
    require(not any(SEQ.rsplit('/',1)[-1] in row for row in baseline['results']),'Replacement already referenced; use verify')
    srcs=[i for i,row in enumerate(baseline['results']) if row=='Asset[AnimSequence]:'+OLD.rsplit('/',1)[-1]]
    require(len(srcs)==1,'Expected exactly one shared sprint row');src=srcs[0]
    snap=setup['chooser_snapshot'](MAIN)
    require(snap['rows'][src]['cells'][0]=='= Locomotion Loop' and snap['rows'][src]['cells'][2]=='= Sprint',
            'Shared sprint template differs')
    cols,mapped=pivot['columns_for'](a,snap,text,transition)
    pos,neg=mapped['positive'],mapped['negative']
    require(not pivot['tag_names'](transition['column_data'](cols[neg],text)[3][src],text),
            'Shared sprint already rifle-gated')
    require(seq.get_editor_property('skeleton').get_path_name().startswith('/Game/MetaHumans/'),
            'Replacement is not the expected MH skeleton')
    dirty={p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    require(not dirty.intersection([MAIN,SEQ,DB]),'Affected packages have unsaved user changes')
    backup=OUT/('Backup_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f'));backup.mkdir()
    protected={}
    for path in [MAIN,SEQ,OLD,'/Game/AZ/Blueprints/Animation/MotionMatching/PSD_v2_SprintLoco']:
        file=ROOT/'Content'/(path.removeprefix('/Game/')+'.uasset')
        require(file.exists(),'Missing package file: '+str(file))
        copy=backup/file.name;shutil.copy2(file,copy)
        protected[path]=hashlib.sha256(file.read_bytes()).hexdigest()
    baseline['meta']={'count':base,'source_row':src,'positive':pos,'negative':neg,
                      'backup':str(backup),'hashes':protected}
    (OUT/'asset-before.json').write_text(json.dumps(baseline,indent=2),encoding='utf-8')
    # This supplied clip is genuinely in-place. Preserve its root-motion policy and1x rate.
    seq.set_editor_property('loop',True)
    measure=json.loads((OUT/'contact-measurement.json').read_text(encoding='utf-8'))
    for name,record in measure['curves'].items():
        require(not unreal.AnimationLibrary.does_curve_exist(seq,name,unreal.RawCurveTrackTypes.RCT_FLOAT),
                'Existing contact curve must not be overwritten')
        unreal.AnimationLibrary.add_curve(seq,name,unreal.RawCurveTrackTypes.RCT_FLOAT)
        keys=record['keys']
        unreal.AnimationLibrary.add_float_curve_keys(seq,name,keys['times'],keys['values'])
    require(unreal.load_asset(DB) is None,'New database path already occupied')
    folder,name=DB.rsplit('/',1)
    db=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.PoseSearchDatabase,None)
    require(db,'Database creation failed')
    db.set_editor_property('schema',unreal.load_asset(SCHEMA));save(db)
    require(not setup['branch_notifies'](seq),'Existing BranchIn must not be replaced')
    require(PU.add_branch_in_notify(seq,db,0.,0.),'BranchIn creation failed')
    save(seq);save(db)
    require(db.get_num_animation_assets()==1,'BranchIn did not synchronize membership')
    require(PU.set_disable_reselection_on_database(db,True)==1,'DB continuity setup failed');save(db)
    require(CU.duplicate_row_on_sub(MAIN,'',src)==base,'Unexpected append row index')
    require(CU.set_cell_asset_on_sub(MAIN,'',base,seq),'Rifle result assignment failed')
    cols=list(a.get_editor_property('columns_structs'))
    for i in [pos,neg]:
        values=transition['column_data'](cols[i],text)[3]
        values=list(values)
        if i==pos:values[base]='(GameplayTags=((TagName="Weapon.Rifle")))'
        else:
            values[src]='(GameplayTags=((TagName="Weapon.Rifle")))';values[base]='()'
        cols[i]=transition['with_rows'](cols[i],values,text)
    a.set_editor_property('columns_structs',cols)
    require(CU.compile_and_save(MAIN),'Chooser compile/save failed')
    result=verify()
    for path in [OLD,'/Game/AZ/Blueprints/Animation/MotionMatching/PSD_v2_SprintLoco']:
        file=ROOT/'Content'/(path.removeprefix('/Game/')+'.uasset')
        require(hashlib.sha256(file.read_bytes()).hexdigest()==protected[path],'Shared sprint package changed')
    return result
