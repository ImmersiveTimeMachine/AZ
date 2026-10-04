# @Description: WGS 0.1 clip sampler - dumps body+weapon world transforms per clip to Saved/wgs/samples
import unreal
import json
import os

BONES = [
    "root", "pelvis",
    "spine_01", "spine_02", "spine_03", "spine_04", "spine_05",
    "neck_01", "neck_02", "head",
    "clavicle_l", "clavicle_r",
    "upperarm_l", "upperarm_r",
    "lowerarm_l", "lowerarm_r",
    "hand_l", "hand_r",
    "thigh_l", "thigh_r",
    "calf_l", "calf_r",
    "az_weapon_r",
]
for finger in ("thumb", "index", "middle", "ring", "pinky"):
    for idx in ("01", "02", "03"):
        for side in ("l", "r"):
            BONES.append(f"{finger}_{idx}_{side}")

BODY_MESH_PATH = "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"
SOCKET_NAME = "RightHandWinchesterSocket"
CURVE_NAMES = ["AZ_Grip_L", "AZ_Grip_R"]

OUT_DIR = "C:/UnrealEngine/Games/AZ/Saved/wgs/samples"


AL = unreal.AnimationLibrary


def curve_at(clip, name, t):
    # Copied from Tools/az_grip_apply.py curve_at().
    if not AL.does_curve_exist(clip, name, unreal.RawCurveTrackTypes.RCT_FLOAT):
        return 0.0
    times, values = AL.get_float_keys(clip, name)
    if not times:
        return 0.0
    for i in range(len(times) - 1):
        if times[i] <= t <= times[i + 1]:
            k = (t - times[i]) / max(1e-6, times[i + 1] - times[i])
            return values[i] + (values[i + 1] - values[i]) * k
    return values[0] if t < times[0] else values[-1]


def xform_to_list(t):
    loc = t.translation
    rot = t.rotation  # FQuat
    return [loc.x, loc.y, loc.z, rot.x, rot.y, rot.z, rot.w]


def get_bone_world(pose, bone_name, pose_bone_names):
    if pose_bone_names is not None and bone_name not in pose_bone_names:
        return None
    try:
        return pose.get_bone_pose(bone_name, unreal.AnimPoseSpaces.WORLD)
    except Exception:
        return None


def sample_clip(clip_path):
    seq = unreal.load_asset(clip_path)
    if seq is None:
        return {"clip": clip_path, "error": "load_asset returned None"}

    length = seq.get_play_length()

    n = int(length * 15.0)
    times = [k / 15.0 for k in range(0, n + 1)]
    if not times or abs(times[-1] - length) > 1e-6:
        times.append(length)

    body_mesh = unreal.load_asset(BODY_MESH_PATH)
    socket = None
    if body_mesh is not None:
        try:
            socket = body_mesh.find_socket(SOCKET_NAME)
        except Exception:
            socket = None

    missing_bones = set()
    bones_out = {b: [] for b in BONES}
    weapon_out = []
    curves_out = {c: [] for c in CURVE_NAMES}

    options = unreal.AnimPoseEvaluationOptions()

    for t in times:
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, t, options)
        try:
            pose_bone_names = set(str(n) for n in pose.get_bone_names())
        except Exception:
            pose_bone_names = None

        bone_world = {}
        for b in BONES:
            wt = get_bone_world(pose, b, pose_bone_names)
            if wt is None:
                missing_bones.add(b)
                bones_out[b].append(None)
            else:
                bone_world[b] = wt
                bones_out[b].append(xform_to_list(wt))

        # weapon world = compose(socket_local, az_weapon_r_world)
        if socket is not None and "az_weapon_r" in bone_world:
            socket_local = unreal.Transform(
                location=socket.get_editor_property("relative_location"),
                rotation=socket.get_editor_property("relative_rotation"),
                scale=socket.get_editor_property("relative_scale"),
            )
            weapon_world = unreal.MathLibrary.compose_transforms(socket_local, bone_world["az_weapon_r"])
            weapon_out.append(xform_to_list(weapon_world))
        else:
            weapon_out.append(None)

        for c in CURVE_NAMES:
            curves_out[c].append(curve_at(seq, c, t))

    clip_name = clip_path.rsplit("/", 1)[-1]
    result = {
        "clip": clip_name,
        "path": clip_path,
        "length": length,
        "times": times,
        "curves": curves_out,
        "weapon": weapon_out,
        "bones": bones_out,
        "missing_bones": sorted(missing_bones),
    }

    out_path = os.path.join(OUT_DIR, f"{clip_name}.json")
    with open(out_path, "w") as f:
        json.dump(result, f)

    return {
        "clip": clip_name,
        "path": clip_path,
        "length": length,
        "num_times": len(times),
        "missing_bones": sorted(missing_bones),
        "out_path": out_path,
    }


def run(clip_paths):
    os.makedirs(OUT_DIR, exist_ok=True)
    summary = []
    for cp in clip_paths:
        try:
            summary.append(sample_clip(cp))
        except Exception as e:
            summary.append({"clip": cp, "error": str(e)})
    return summary


if "CLIPS" in globals():
    _summary = run(CLIPS)
    print(json.dumps(_summary, indent=2))

import gc; gc.collect()
