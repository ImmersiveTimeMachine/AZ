"""One read-only snapshot in an existing user-run PIE session.

After lead review, execute this source once through the editor's Python command
tool while the user is playing. No observer, file output, evaluation request or
editor changes are installed. Output is one JSON line prefixed [FootPoseSnapshot].
Requested stack start time is NOT the playing phase. Hierarchy globals are rig
component space; mesh sockets are the published mesh component-space pose. The
curve getter joins existing parallel evaluation on the game thread but does not
guarantee the rig and postprocessed mesh buffers represent the same pose stage.
"""

import json
import unreal


FOOT_CLASS = "/Game/AZ/Blueprints/Animation/Procedural/CR_AZ_MHC_FootPlacement.CR_AZ_MHC_FootPlacement_C"
ANIM_CLASS = "/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC.AZ_ABP_MoverHero_MHC_C"


def _path(obj):
    return obj.get_path_name() if obj is not None else None


def _transform(value):
    pos = value.translation
    rot = value.rotation
    scale = value.scale3d
    return {"translation": [pos.x, pos.y, pos.z],
            "rotation_quaternion_xyzw": [rot.x, rot.y, rot.z, rot.w],
            "scale": [scale.x, scale.y, scale.z]}


def _value(value):
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, unreal.Object):
        return _path(value)
    return str(value)


def _read(obj, field, missing):
    try:
        return _value(obj.get_editor_property(field))
    except Exception as exc:
        missing[field] = str(exc)[:200]
        return None


def _has_outer(obj, expected):
    # AnimNode_ControlRig creates its instance with the owning mesh as Outer.
    for _ in range(16):
        if obj == expected:
            return True
        obj = obj.get_outer() if obj is not None else None
        if obj is None:
            return False
    return False


def _key_label(key):
    return {"name": str(key.name), "type": str(key.type)}


def capture():
    result = {"read_only": True, "missing": {}, "limits": [
        "Single game-thread invocation; no requested animation evaluation.",
        "GetCurveValue joins existing mesh animation work; published mesh buffers and last rig solve are different pose stages.",
        "Postprocess can change the published mesh after this rig; no same-evaluation serial is exposed by this snapshot.",
        "RequestedStartTime is a push request, not actual playing phase; crossing is not inferred from coordinates.",
        "Parentless animated nulls are candidate saved pre-solve targets, not proof of their write order or freshness."]}
    worlds = [world for world in unreal.EditorLevelLibrary.get_pie_worlds(False) if world]
    candidates = []
    for world in worlds:
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if pawn is not None:
            candidates.append((world, pawn))
    if len(candidates) != 1:
        result["error"] = "Expected exactly one PIE world with player pawn index 0"
        result["player_candidates"] = [_path(pawn) for _, pawn in candidates]
        return result
    world, pawn = candidates[0]
    result.update(world=_path(world), pawn=_path(pawn))
    mesh = pawn.get_mesh()
    if mesh is None or mesh.get_owner() != pawn:
        result["error"] = "Player pawn has no verified owned hero mesh"
        return result
    anim = mesh.get_anim_instance()
    if anim is None or _path(anim.get_class()) != ANIM_CLASS:
        result["error"] = "Player mesh is not using the expected active AnimBP"
        result["anim_instance"] = _path(anim)
        return result
    result.update(mesh=_path(mesh), anim_instance=_path(anim))
    result["frame_begin"] = unreal.SystemLibrary.get_frame_count()
    result["game_time_seconds"] = unreal.GameplayStatics.get_time_seconds(world)
    # FIRST pose read: engine GetProxyOnAnyThread blocks existing evaluation on GT
    # and permits PostAnimEvaluation. It does not force a new solve or new tick.
    result["anim_instance_contacts"] = {
        name: anim.get_curve_value(name) for name in ("contact_l", "contact_r")}
    result["mesh_world_transform"] = _transform(mesh.get_world_transform())
    ctx = anim.get_editor_property("ChooserContext")
    stack = anim.get_editor_property("BlendStackInputs")
    result["chooser"] = {field: _read(ctx, field, result["missing"])
                         for field in ("SMState", "bIsAiming", "bStrafe", "bIsMoving", "Speed2D",
                                       "MovementDirection", "MovementDirection8", "Gait", "Stance")}
    result["stack_request"] = {field: _read(stack, field, result["missing"])
                               for field in ("Anim", "StartTime", "bLoop", "BlendTime")}
    result["native_feet"] = {field: _read(anim, field, result["missing"])
                             for field in ("bProceduralFootPinning", "ProceduralFeetAlpha",
                                           "bProceduralFeetReset", "ProceduralFootPinReleaseSerial")}
    foot_class = unreal.load_class(None, FOOT_CLASS)
    if foot_class is None:
        result["error"] = "Foot rig generated class unavailable"
        return result
    rigs = list(unreal.ControlRig.find_control_rigs(mesh, foot_class))
    owned = [rig for rig in rigs if _has_outer(rig, mesh)]
    if len(owned) != 1:
        result["error"] = "Expected exactly one matching rig whose outer chain contains this mesh"
        result["rig_candidates"] = [_path(rig) for rig in rigs]
        return result
    rig = owned[0]
    result["rig"] = _path(rig)
    result["rig_variables"] = {field: _read(rig, field, result["missing"])
                               for field in ("Enable Foot Pinning", "LeftFootPinned", "RightFootPinned",
                                             "LeftPinWeight", "RightPinWeight", "PinReleaseSerial",
                                             "HandledPinReleaseSerial", "ForceReset", "PendingReset")}
    hierarchy = rig.get_hierarchy()
    keys = list(hierarchy.get_all_keys())
    requested = ((unreal.RigElementType.NULL, "animated_toe_target_l_null"),
                 (unreal.RigElementType.NULL, "animated_toe_target_r_null"),
                 (unreal.RigElementType.NULL, "animated_pelvis_null"))
    requested += tuple((unreal.RigElementType.BONE, name)
                       for name in ("root", "pelvis", "foot_l", "foot_r", "ball_l", "ball_r"))
    requested += tuple((unreal.RigElementType.CONTROL, name)
                       for name in ("toe_l_ctrl", "toe_r_ctrl", "pelvis_ctrl", "foot_l_ctrl", "foot_r_ctrl",
                                    "calculated_toe_target_l_ctrl", "calculated_toe_target_r_ctrl"))
    result["rig_component_pose"] = {}
    for kind, name in requested:
        matches = [key for key in keys if key.type == kind and str(key.name) == name]
        if len(matches) != 1:
            result["missing"]["hierarchy:" + name] = "Expected exactly one matching name and type"
            continue
        key = matches[0]
        parents = list(hierarchy.get_parents(key, True))
        entry = {"key": _key_label(key), "recursive_parents": [_key_label(parent) for parent in parents],
                 "transform": _transform(hierarchy.get_global_transform(key))}
        if kind == unreal.RigElementType.NULL:
            entry["parentless_saved_target_candidate"] = len(parents) == 0
        result["rig_component_pose"][name] = entry
    result["rig_hierarchy_contacts"] = {}
    for name in ("contact_l", "contact_r"):
        matches = [key for key in keys if key.type == unreal.RigElementType.CURVE and str(key.name) == name]
        if len(matches) == 1:
            result["rig_hierarchy_contacts"][name] = hierarchy.get_curve_value(matches[0])
        else:
            result["missing"]["hierarchy_curve:" + name] = "Expected exactly one curve key"
    result["published_mesh_component_pose"] = {}
    for name in ("root", "pelvis", "thigh_l", "thigh_r", "calf_l", "calf_r", "foot_l", "foot_r", "ball_l", "ball_r"):
        if mesh.does_socket_exist(name):
            result["published_mesh_component_pose"][name] = _transform(
                mesh.get_socket_transform(name, unreal.RelativeTransformSpace.RTS_COMPONENT))
        else:
            result["missing"]["mesh_bone:" + name] = "Bone/socket absent"
    result["frame_end"] = unreal.SystemLibrary.get_frame_count()
    return result


def main():
    try:
        result = capture()
    except Exception as exc:
        result = {"read_only": True, "error": str(exc), "error_type": type(exc).__name__}
    print("[FootPoseSnapshot] " + json.dumps(result, ensure_ascii=False, separators=(",", ":")))


if __name__ == "__main__":
    main()
