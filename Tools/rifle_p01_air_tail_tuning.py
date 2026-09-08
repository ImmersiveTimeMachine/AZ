# @Description: Measure and optionally create unassigned P01 airborne-motion tuning drafts.
"""Audit by default. prepare=True writes ONLY DerivedJump/Tuning draft sequences.

No active takeoff/landing, chooser, DB, AnimBP, native code or IK changes. The
existing two-second Air clip supplies bounded relative motion around each
takeoff's own RU/LU held pose. Root keys and the complete pre-hold prefix stay
unchanged. Drafts are not activation or runtime visual acceptance.
"""
import gc
import hashlib
import json
import math
from pathlib import Path
import runpy

import unreal

PROJECT = Path('C:/UnrealEngine/Games/AZ')
CONTENT_RECEIPT = PROJECT / 'Saved/RifleAnimationContent/p01-jump-prepare.json'
TUNING = '/Game/AZ/Assets/RTG/Riffle_P01/DerivedJump/Tuning'
BODY = '/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh'
RAW_MESH = '/Game/SurvivalMan/Meshes/SKM_SurvivalMan_Mesh1'
OWNER = 'rifle_p01_air_tail_tuning:v1'
OWNER_KEY, MANIFEST_KEY, STATE_KEY = 'AZ.P01AirDraft.Owner', 'AZ.P01AirDraft.Manifest', 'AZ.P01AirDraft.State'
AMPLITUDES = (1.0, 0.75, 0.5, 0.35, 0.25, 0.15, 0.10)
# Explicit draft tolerances, not claims of final visual acceptance.
DEFAULT_LIMITS = dict(grip_anchor_drift_cm=2.0, grip_span_change_cm=1.0,
                      landing_nearest_rms_cm=2.5, max_local_delta_degrees=10.0)
JOINTS = ('pelvis', 'thigh_l', 'calf_l', 'foot_l', 'thigh_r', 'calf_r', 'foot_r',
          'spine_02', 'hand_l', 'hand_r')
EAL, AL, E = unreal.EditorAssetLibrary, unreal.AnimationLibrary, unreal.AnimPoseExtensions


def require(value, message):
    if not value: raise RuntimeError(message)


def package(asset): return asset.get_path_name().split('.')[0]


def load(path):
    asset = unreal.load_asset(path)
    require(asset is not None, 'Missing asset: ' + path)
    return asset


def metadata(asset):
    # Hydrates metadata before assertions; first scalar GetMetaDataTag can otherwise read empty.
    return {str(k): str(v) for k, v in EAL.get_metadata_tag_values(asset).items()}


def game_world():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    return world.get_path_name() if world else None


def digest(value): return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def source_hash(path):
    file = PROJECT / 'Content' / (path[len('/Game/'):] + '.uasset')
    require(file.is_file(), 'Missing source disk package: ' + path)
    return hashlib.sha256(file.read_bytes()).hexdigest()


def vec(v): return [float(v.x), float(v.y), float(v.z)]
def quat(q): return [float(q.x), float(q.y), float(q.z), float(q.w)]
def normalize(q):
    length = math.sqrt(sum(v*v for v in q))
    require(length > 1e-8, 'Invalid quaternion')
    return [v/length for v in q]
def inverse(q): return [-q[0], -q[1], -q[2], q[3]]
def multiply(a, b):
    x,y,z,w=a; X,Y,Z,W=b
    return [w*X+x*W+y*Z-z*Y, w*Y-x*Z+y*W+z*X, w*Z+x*Y-y*X+z*W, w*W-x*X-y*Y-z*Z]
def rotate(q, p): return multiply(multiply(q, p+[0.0]), inverse(q))[:3]
def angle(a,b): return math.degrees(2*math.acos(min(1.0, abs(sum(x*y for x,y in zip(normalize(a),normalize(b)))))))


def scaled_rotation(delta, amplitude):
    delta=normalize(delta)
    if delta[3] < 0: delta=[-v for v in delta]
    half=math.acos(min(1.0, max(-1.0, delta[3])))
    sine=math.sin(half)
    if sine < 1e-8: return [0.0,0.0,0.0,1.0]
    factor=math.sin(half*amplitude)/sine
    return normalize([delta[i]*factor for i in range(3)]+[math.cos(half*amplitude)])


def transform_data(t): return (vec(t.translation), normalize(quat(t.rotation)), vec(t.scale3d))
def as_transform(data):
    p,q,s=data; value=unreal.Transform()
    value.translation=unreal.Vector(*p); value.rotation=unreal.Quat(*q); value.scale3d=unreal.Vector(*s)
    return value


def compose(base, air, zero, amplitude):
    p,q,s=base
    delta=scaled_rotation(multiply(air[1], inverse(zero[1])), amplitude)
    return ([p[i]+amplitude*(air[0][i]-zero[0][i]) for i in range(3)],
            normalize(multiply(delta,q)), list(s))


def options(mesh, retarget):
    value=unreal.AnimPoseEvaluationOptions()
    for key, setting in dict(evaluation_type=unreal.AnimDataEvalType.RAW, should_retarget=retarget,
                             extract_root_motion=False, incorporate_root_motion_into_pose=True,
                             evaluate_curves=False, optional_skeletal_mesh=mesh).items(): value.set_editor_property(key,setting)
    return value


def pose(asset,time,opt):
    value=E.get_anim_pose_at_time(asset,time,opt)
    require(E.is_valid(value), 'Pose evaluation failed: '+package(asset))
    return value


def locals_for(value,bones): return {bone:transform_data(E.get_bone_pose(value,bone,unreal.AnimPoseSpaces.LOCAL)) for bone in bones}


def pose_metric(value):
    root=E.get_bone_pose(value,'root',unreal.AnimPoseSpaces.WORLD).translation
    positions={bone:vec(E.get_bone_pose(value,bone,unreal.AnimPoseSpaces.WORLD).translation) for bone in JOINTS}
    right=E.get_bone_pose(value,'hand_r',unreal.AnimPoseSpaces.WORLD)
    span=[positions['hand_l'][i]-positions['hand_r'][i] for i in range(3)]
    anchor=rotate(inverse(normalize(quat(right.rotation))),span)
    return {'joints':[[positions[bone][i]-vec(root)[i] for i in range(3)] for bone in JOINTS],
            'anchor':anchor,'span':math.sqrt(sum(v*v for v in span)),
            'feet':[positions['foot_l'],positions['foot_r']]}


def rms(a,b): return math.sqrt(sum(math.dist(x,y)**2 for x,y in zip(a,b))/len(a))


def air_path(record):
    if not record['moving']:
        suffix='Stand/Jump/IPC/Riffle_P_W2_Stand_'+('Aim' if record['aim'] else 'Relaxed')+'_Jump_Air_IPC'
    else:
        family='Walk' if record['gait']=='Walk' else 'Jog'
        suffix=family+'/Jump/IPC/Riffle_P_W2_'+family+('_Aim' if record['aim'] else '')+'_F_Jump_LU_Air_IPC'
    return '/Game/AZ/Assets/RTG/Riffle_P01/'+suffix


def context(record):
    source,air,land=load(record['takeoff']),load(air_path(record)),load(record['land'])
    source_meta=metadata(source)
    require(source_meta.get('AZ.P01Jump.Owner')=='rifle_p01_jump_setup:v1'
            and source_meta.get('AZ.P01Jump.State')=='complete','Active takeoff ownership is not recognized')
    model=source.get_editor_property('data_model_interface'); rate=model.get_frame_rate()
    fps=float(rate.numerator)/float(rate.denominator)
    hold_seconds=(record['measurement']['hold_frame']-record['measurement']['source_entry_frame'])/30
    hold_key=math.ceil(hold_seconds*fps); hold_time=hold_key/fps
    raw_opt=options(load(RAW_MESH),False); metric_opt=options(load(BODY),True)
    bones=[str(b) for b in model.get_bone_track_names()]
    require('root' in bones and all(b in bones for b in JOINTS),'Required tracked body bones missing')
    raw_held=locals_for(pose(source,hold_time,raw_opt),bones)
    air_zero=locals_for(pose(air,0.0,raw_opt),bones)
    held_metric_pose=pose(source,hold_time,metric_opt)
    metric_bones={str(b) for b in E.get_bone_names(held_metric_pose)}
    common=[b for b in bones if b in metric_bones and b!='root']
    held_metric_local=locals_for(held_metric_pose,common)
    require(abs(float(AL.get_sequence_length(air))-2.0)<1e-5,'Expected existing two-second Air cycle')
    landing=[pose_metric(pose(land,t,metric_opt)) for t in (0.0,1/30,2/30,3/30,4/30)]
    return dict(source=source,air=air,land=land,bones=bones,common=common,fps=fps,hold_key=hold_key,hold_time=hold_time,
                raw_opt=raw_opt,metric_opt=metric_opt,raw_held=raw_held,air_zero=air_zero,
                held_metric_local=held_metric_local,held_metric=pose_metric(held_metric_pose),landing=landing,air_cache={})


def air_locals(ctx,phase):
    phase=phase%2.0
    if phase not in ctx['air_cache']: ctx['air_cache'][phase]=locals_for(pose(ctx['air'],phase,ctx['raw_opt']),ctx['bones'])
    return ctx['air_cache'][phase]


def candidate_pose(ctx,phase,amplitude):
    value=pose(ctx['source'],ctx['hold_time'],ctx['metric_opt'])
    air=air_locals(ctx,phase)
    for bone in ctx['common']:
        data=compose(ctx['held_metric_local'][bone],air[bone],ctx['air_zero'][bone],amplitude)
        value=E.set_bone_pose(value,as_transform(data),bone,unreal.AnimPoseSpaces.LOCAL)
    return value


def measure(ctx,amplitude,samples=13,draft=None):
    result=dict(amplitude=amplitude,grip_anchor_drift_cm=0.0,grip_span_change_cm=0.0,
                landing_nearest_rms_cm=0.0,max_local_delta_degrees=0.0,foot_variation_cm=0.0,samples=samples)
    per_phase=[]
    for i in range(samples):
        phase=2.0*i/(samples-1)
        value=pose(draft,ctx['hold_time']+phase,ctx['metric_opt']) if draft else candidate_pose(ctx,phase,amplitude)
        metric=pose_metric(value)
        grip=math.dist(metric['anchor'],ctx['held_metric']['anchor'])
        span=abs(metric['span']-ctx['held_metric']['span'])
        nearest=min(rms(metric['joints'],landing['joints']) for landing in ctx['landing'])
        delta=max(angle(transform_data(E.get_bone_pose(value,b,unreal.AnimPoseSpaces.LOCAL))[1],ctx['held_metric_local'][b][1]) for b in ctx['common'])
        foot=max(math.dist(a,b) for a,b in zip(metric['feet'],ctx['held_metric']['feet']))
        for key,number in [('grip_anchor_drift_cm',grip),('grip_span_change_cm',span),
                           ('landing_nearest_rms_cm',nearest),('max_local_delta_degrees',delta),('foot_variation_cm',foot)]:
            result[key]=max(result[key],number)
        per_phase.append(dict(phase=phase,grip_anchor_drift_cm=grip,landing_nearest_rms_cm=nearest))
    first=candidate_pose(ctx,0,amplitude) if not draft else pose(draft,ctx['hold_time'],ctx['metric_opt'])
    last=candidate_pose(ctx,2,amplitude) if not draft else pose(draft,ctx['hold_time']+2,ctx['metric_opt'])
    result['loop_seam_rms_cm']=rms(pose_metric(first)['joints'],pose_metric(last)['joints'])
    result['phase_details']=per_phase
    return result


def within(measurement,limits): return all(measurement[key]<=value for key,value in limits.items())


def audit(limits=None):
    require(game_world() is None, 'Game world is active; defer editor-only metadata/asset audit')
    limits=dict(DEFAULT_LIMITS if limits is None else limits)
    require(set(limits)==set(DEFAULT_LIMITS) and all(v>0 for v in limits.values()),'Provide all explicit positive draft limits')
    content=json.loads(CONTENT_RECEIPT.read_text(encoding='utf-8'))
    report=dict(status='auditing',game_world=game_world(),limits=limits,records=[],assignment='NONE; draft candidates only')
    for record in content['records']:
        ctx=context(record); attempts=[]; chosen=None
        for amplitude in AMPLITUDES:
            trial=measure(ctx,amplitude)
            attempts.append(trial)
            if within(trial,{key:value*0.9 for key,value in limits.items()}):
                dense=measure(ctx,amplitude,61)
                attempts.append(dense)
                if within(dense,limits): chosen=dense;break
        require(chosen is not None,'No bounded animated candidate for '+record['key'])
        manifest=dict(owner=OWNER,source=record['takeoff'],air=air_path(record),land=record['land'],
                      source_hash=source_hash(record['takeoff']),air_hash=source_hash(air_path(record)),
                      hold_key=ctx['hold_key'],fps=ctx['fps'],amplitude=chosen['amplitude'],limits=limits)
        target=TUNING+'/AS_P01_Jump_'+record['key']+'_AirDelta_'+str(round(chosen['amplitude']*100)).zfill(3)
        if EAL.does_asset_exist(target):
            meta=metadata(load(target)); require(meta.get(OWNER_KEY)==OWNER and meta.get(MANIFEST_KEY)==json.dumps(manifest,sort_keys=True),
                                               'Unknown or changed-source tuning output: '+target)
        report['records'].append(dict(key=record['key'],target=target,manifest=manifest,predicted=chosen,attempts=attempts))
        print('[P01AirDraft] audited '+record['key']+' amplitude='+str(chosen['amplitude']))
    report['status']='audit_complete'
    return report


def bake(ctx,draft,amplitude):
    require(game_world() is None,'Game world appeared; stop draft writes')
    model=ctx['source'].get_editor_property('data_model_interface'); count=model.get_number_of_keys()
    times=[i/ctx['fps'] for i in range(count)]
    original={i:locals_for(pose(ctx['source'],time,ctx['raw_opt']),ctx['bones']) for i,time in enumerate(times)}
    controller=draft.get_editor_property('controller'); target_model=draft.get_editor_property('data_model_interface')
    if controller is None or controller.get_model_interface()!=target_model:
        controller=type(ctx['source'].get_editor_property('controller'))(outer=draft);controller.set_model(target_model)
    controller.open_bracket('Author unassigned bounded P01 air-motion draft',False)
    try:
        for bone in ctx['bones']:
            if bone=='root':continue  # Root track is not rewritten, resampled, resized or retimed.
            values=[]
            for i,time in enumerate(times):
                value=original[i][bone] if i<=ctx['hold_key'] else compose(ctx['raw_held'][bone],air_locals(ctx,time-ctx['hold_time'])[bone],ctx['air_zero'][bone],amplitude)
                values.append(value)
            require(controller.set_bone_track_keys(bone,[unreal.Vector(*x[0]) for x in values],
                    [unreal.Quat(*x[1]) for x in values],[unreal.Vector(*x[2]) for x in values],False),'Draft bone write failed: '+bone)
    finally:controller.close_bracket(False)
    require(target_model.get_number_of_keys()==count and abs(float(AL.get_sequence_length(draft))-float(AL.get_sequence_length(ctx['source'])))<1e-6,
            'Draft changed the existing animation clock')
    root_error=root_angle_error=root_scale_error=prefix_position_error=prefix_angle_error=0.0
    for i,time in enumerate(times):
        value=pose(draft,time,ctx['raw_opt'])
        ar=AL.extract_root_track_transform(draft,time);br=AL.extract_root_track_transform(ctx['source'],time)
        a=vec(ar.translation);b=vec(br.translation)
        root_error=max(root_error,math.dist(a,b))
        root_angle_error=max(root_angle_error,angle(quat(ar.rotation),quat(br.rotation)))
        root_scale_error=max(root_scale_error,math.dist(vec(ar.scale3d),vec(br.scale3d)))
        if i<=ctx['hold_key']:
            for bone in ctx['bones']:
                current=transform_data(E.get_bone_pose(value,bone,unreal.AnimPoseSpaces.LOCAL));before=original[i][bone]
                prefix_position_error=max(prefix_position_error,math.dist(current[0],before[0]))
                prefix_angle_error=max(prefix_angle_error,angle(current[1],before[1]))
    require(root_error<1e-4 and root_angle_error<0.001 and root_scale_error<1e-6
            and prefix_position_error<0.001 and prefix_angle_error<0.01,'Root/prefix preservation failed')
    return dict(root_max_delta_error_cm=root_error,root_max_rotation_error_deg=root_angle_error,
                root_max_scale_error=root_scale_error,prefix_max_position_error_cm=prefix_position_error,
                prefix_max_rotation_error_deg=prefix_angle_error,number_of_keys=count)


def main(prepare=False,limits=None,receipt_path=None):
    report=None
    try:
        current_world=game_world()
        if current_world is not None:
            report=dict(status='drafts_not_written_game_world_active',game_world=current_world,
                        assignment='NONE; draft candidates only',records=[],
                        note='Static read-only fitting receipt remains available; no editor asset APIs were called')
            return report
        report=audit(limits)
        if not prepare:return report
        if game_world() is not None:
            report['status']='drafts_not_written_game_world_active';return report
        setup=runpy.run_path(str(PROJECT/'Tools/rifle_p01_setup.py'),run_name='p01_air_draft_backup')
        sources=sorted(set(r['manifest']['source'] for r in report['records']))
        report['backup']=setup['backup_existing'](sources)
        content={r['key']:r for r in json.loads(CONTENT_RECEIPT.read_text(encoding='utf-8'))['records']}
        for record in report['records']:
            require(game_world() is None,'Game world appeared; no further draft writes')
            manifest=record['manifest'];ctx=context(content[record['key']])
            require(source_hash(manifest['source'])==manifest['source_hash'] and source_hash(manifest['air'])==manifest['air_hash'],'Source changed after draft audit')
            draft=load(record['target']) if EAL.does_asset_exist(record['target']) else EAL.duplicate_asset(manifest['source'],record['target'])
            require(draft is not None,'Draft duplicate failed')
            EAL.set_metadata_tag(draft,OWNER_KEY,OWNER);EAL.set_metadata_tag(draft,MANIFEST_KEY,json.dumps(manifest,sort_keys=True));EAL.set_metadata_tag(draft,STATE_KEY,'preparing')
            record['preservation']=bake(ctx,draft,manifest['amplitude'])
            record['actual']=measure(ctx,manifest['amplitude'],61,draft)
            require(within(record['actual'],report['limits']) and record['actual']['loop_seam_rms_cm']<0.1,'Actual draft exceeds reviewed geometric bounds')
            require(record['actual']['foot_variation_cm']>0.25,'Draft motion is too small to distinguish from a static hold')
            EAL.set_metadata_tag(draft,STATE_KEY,'draft_complete_unassigned')
            require(game_world() is None and EAL.save_loaded_asset(draft,only_if_is_dirty=False),'Draft save failed or game world appeared')
            print('[P01AirDraft] saved unassigned '+record['key'])
        report['status']='drafts_prepared_unassigned'
        return report
    except Exception as error:
        if report is not None:report.update(status='failed',error=str(error))
        raise
    finally:
        if report is not None and receipt_path:
            path=Path(receipt_path);path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(report,indent=2),encoding='utf-8')
        if report is not None:print('[P01AirDraft] '+json.dumps({k:report.get(k) for k in ('status','game_world','assignment','error')}))
        gc.collect()


if __name__=='__main__':main()
