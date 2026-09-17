"""Allow existing rifle/pistol turn-start fallbacks for armed moving180 reversals.

Chooser-only. No source animation changes, C++ build, PIE or tests.
"""
from pathlib import Path
from datetime import datetime, timezone
import copy
import hashlib
import json
import re
import runpy
import shutil
import unreal

ROOT=Path('C:/UnrealEngine/Games/AZ')
OUT=ROOT/'Saved/WeaponMoving180'
MAIN='/Game/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations'
CU=unreal.AZ_ChooserUtils


def require(ok,msg):
    if not ok:raise RuntimeError(msg)


def helpers():
    names=['rifle_p01_setup.py','rifle_p01_activate.py','rifle_p01_transition_setup.py','rifle_p01_pivot_setup.py']
    return [runpy.run_path(str(ROOT/'Tools'/n),run_name='weapon_turn_helper') for n in names]


def columns(asset,text,tr):
    return [tr['column_data'](c,text)[3] for c in asset.get_editor_property('columns_structs')]


def result(row):
    return re.search(r'"out":\s*"([^"]*)"',row['line']).group(1)


def matches_tags(value,tag):
    return ('TagName="'+tag+'"') in value


def audit():
    setup,text,tr,pivot=helpers()
    asset=unreal.load_asset(MAIN);require(asset,'Main chooser missing')
    snap=setup['chooser_snapshot'](MAIN)
    data=columns(asset,text,tr)
    cols,mapped=pivot['columns_for'](asset,snap,text,tr)
    names={}
    for i,c in enumerate(cols):
        sig=json.loads(pivot['signature'](c,text,tr))
        if sig[4]==['Weapon.Pistol']:
            names['not_pistol' if sig[3]=='True' else 'pistol']=i
    require(set(names)=={'pistol','not_pistol'},'Pistol filter columns differ')
    mapped.update(names)
    shared_run=[];shared_sprint=[];rifle=[];pistol=[]
    for row in snap['rows']:
        i=row['index'];cells=row['cells'];name=result(row)
        if cells.get(mapped['start']) not in ['= L180','= R180']:continue
        gait=cells.get(mapped['gait']);moving=cells.get(mapped['moving'])
        if name.startswith('Asset[AnimSequence]:AnimPro_RunFwdTurn180_') and moving=='True':
            if gait=='= Run':shared_run.append(i)
            elif gait=='= Sprint':shared_sprint.append(i)
        if moving=='True' and gait=='= Run' and matches_tags(data[mapped['positive']][i],'Weapon.Rifle'):
            rifle.append(i)
        if gait in ['= Run','= Sprint'] and matches_tags(data[mapped['pistol']][i],'Weapon.Pistol'):
            require(name in ['Asset[AnimSequence]:AS_Pistol_WalkFwdStart180_L_RM','Asset[AnimSequence]:AS_Pistol_WalkFwdStart180_R_RM'],
                    'Unexpected pistol pivot source')
            require(moving=='False','Pistol moving condition already changed; inspect/verify')
            pistol.append(i)
    require(len(shared_run)==4 and len(shared_sprint)==4 and len(rifle)==4 and len(pistol)==4,
            'Expected four shared rows per gait, four rifle run variants and four pistol starts')
    require(all(not data[mapped['negative']][i].strip('()') for i in shared_sprint),
            'Shared Sprint already rifle-gated')
    require(all(not matches_tags(data[mapped['not_pistol']][i],'Weapon.Pistol') for i in shared_run+shared_sprint),
            'Shared moving turns already pistol-gated')
    # Validate clips already used by rifle Run and pistol standing starts, without changing them.
    paths=unreal.AssetRegistryHelpers.get_asset_registry().get_dependencies(MAIN,unreal.AssetRegistryDependencyOptions())
    by_name={str(p).rsplit('/',1)[-1]:str(p) for p in paths}
    measurements=[]
    for name in sorted(set(result(snap['rows'][i]).split(':',1)[1] for i in rifle+pistol)):
        path=by_name.get(name);seq=unreal.load_asset(path) if path else None
        require(isinstance(seq,unreal.AnimSequence),'Could not resolve existing turn '+name)
        require(seq.get_editor_property('enable_root_motion') and not seq.get_editor_property('loop')
                and abs(float(seq.get_editor_property('rate_scale'))-1)<1e-6,'Turn flags/rate mismatch '+name)
        require(seq.get_editor_property('skeleton').get_path_name().startswith('/Game/MetaHumans/'),
                'Existing turn is not on exact MH skeleton '+name)
        length=float(unreal.AnimationLibrary.get_sequence_length(seq))
        start=unreal.AnimationLibrary.get_bone_pose_for_time(seq,'root',0,False)
        end=unreal.AnimationLibrary.get_bone_pose_for_time(seq,'root',length,False)
        measurements.append({'path':path,'duration':length,'root_start':str(start),'root_end':str(end)})
    return {'rows':len(snap['rows']),'mapped':mapped,'shared_run':shared_run,'shared_sprint':shared_sprint,
            'rifle_templates':rifle,'pistol_rows':pistol,'measurements':measurements,
            'results':[result(r) for r in snap['rows']],
            'columns':[c.export_text() for c in cols],'disabled':list(CU.get_chooser_disabled_rows(MAIN))}


def expected_columns(before,text,tr):
    mapped=before['mapped'];base=before['rows'];copies=before['rifle_templates']
    encoded=before['columns'];old=[]
    for item in encoded:
        c=unreal.InstancedStruct();require(c.import_text(item),'Could not restore baseline struct');old.append(c)
    data=[tr['column_data'](c,text)[3] for c in old]
    wanted=[]
    for i,c in enumerate(old):
        vals=list(data[i])+[data[i][source] for source in copies]
        if i==mapped['gait']:
            for j in range(base,base+len(copies)):vals[j]=data[i][before['shared_sprint'][0]]
        if i==mapped['moving']:
            # Observed native BoolColumn enum encoding (the diagnostic prints Any).
            for j in before['pistol_rows']:vals[j]='MatchAny'
        if i==mapped['negative']:
            for j in before['shared_sprint']:vals[j]='(GameplayTags=((TagName="Weapon.Rifle")))'
        if i==mapped['not_pistol']:
            for j in before['shared_run']+before['shared_sprint']:vals[j]='(GameplayTags=((TagName="Weapon.Pistol")))'
        if i==mapped['not_sprinting']:
            for j in range(base,base+len(copies)):vals[j]='()'
        wanted.append(tr['with_rows'](c,vals,text))
    return wanted


def verify():
    before=json.loads((OUT/'before.json').read_text(encoding='utf-8'))
    setup,text,tr,pivot=helpers();asset=unreal.load_asset(MAIN)
    snap=setup['chooser_snapshot'](MAIN);base=before['rows'];n=len(before['rifle_templates'])
    require(len(snap['rows'])==base+n,'Unexpected chooser row count')
    results=[result(r) for r in snap['rows']]
    require(results==before['results']+[before['results'][i] for i in before['rifle_templates']],
            'Unexpected result asset changes')
    current=list(asset.get_editor_property('columns_structs'))
    wanted=expected_columns(before,text,tr)
    require(len(current)==len(wanted),'Column count changed')
    for i,(actual,expected) in enumerate(zip(current,wanted)):
        require(actual.export_text()==expected.export_text(),'Unexpected chooser filter/output change in column '+str(i))
    disabled=list(CU.get_chooser_disabled_rows(MAIN))
    require(disabled[:base]==before['disabled'] and not any(disabled[base:]),'Enabled-state change')
    report={'status':'verified','rows':base+n,'preserved_results':base,
            'rifle_sprint_rows':list(range(base,base+n)),
            'pistol_run_and_sprint_rows':before['pistol_rows'],
            'shared_run_excludes_pistol':before['shared_run'],
            'shared_sprint_excludes_rifle_and_pistol':before['shared_sprint'],
            'animation_assets_modified':False,'play_rate':1.0,
            'presentation':'existing weapon-specific turn-start fallbacks; no new momentum-preserving pivot content'}
    (OUT/'verified.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    return report


def author():
    require(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None,'PIE active; no chooser writes')
    OUT.mkdir(exist_ok=True)
    require(MAIN not in [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()],
            'Main chooser has unsaved user edits')
    before=audit();base=before['rows']
    folder=OUT/('Backup_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f'));folder.mkdir()
    file=ROOT/'Content/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations.uasset'
    shutil.copy2(file,folder/file.name)
    before['backup']=str(folder/file.name);before['sha256']=hashlib.sha256(file.read_bytes()).hexdigest()
    (OUT/'before.json').write_text(json.dumps(before,indent=2),encoding='utf-8')
    for offset,source in enumerate(before['rifle_templates']):
        require(CU.duplicate_row_on_sub(MAIN,'',source)==base+offset,'Unexpected appended index')
    _,text,tr,_=helpers();asset=unreal.load_asset(MAIN)
    asset.set_editor_property('columns_structs',expected_columns(before,text,tr))
    require(CU.compile_and_save(MAIN),'Chooser compile/save failed')
    report=verify()
    (OUT/'chooser-after.txt').write_text('\n'.join(CU.dump_chooser_full_tree(MAIN)),encoding='utf-8')
    return report
