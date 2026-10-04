# @Description: Live PIE probe - are the hero's finger rotations the grip pose's or the clip's
# exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/probe_live_fingers.py").read())
# For every finger bone: angle (deg) between the LIVE local rotation on the hero mesh and (a) the grip pose, (b) the
# playing clip at the same time (from the anim instance's last pushed clip). Grip applied => (a) ~0.
import unreal, math

APE = unreal.AnimPoseExtensions
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not w:
    print("NO PIE")
else:
    hero = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Pawn) if "Hero" in a.get_class().get_name()][0]
    mesh = [m for m in hero.get_components_by_class(unreal.SkeletalMeshComponent) if m.get_name() == "Mesh"][0]
    ai = mesh.get_anim_instance()
    grip = APE.get_anim_pose_at_time(unreal.load_asset("/Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_Winchester"), 0.0,
                                     unreal.AnimPoseEvaluationOptions())
    clip = unreal.load_asset("/Game/AZ/Assets/Master/RifleMega/Rifle_Styly02_St/Rifle02_IdleSet/AZ_MST_Rifle02_St_Idle00")
    cpose = APE.get_anim_pose_at_time(clip, 1.0, unreal.AnimPoseEvaluationOptions())

    def ang(q1, q2):
        d = abs(q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w)
        return math.degrees(2 * math.acos(min(1.0, d)))

    print("alpha", round(ai.get_editor_property("weapon_grip_alpha"), 2), "pose", ai.get_editor_property("weapon_grip_pose"),
          "curves L/R", round(ai.get_curve_value("AZ_Grip_L"), 2), round(ai.get_curve_value("AZ_Grip_R"), 2),
          "LOD", mesh.get_predicted_lod_level() if hasattr(mesh, "get_predicted_lod_level") else "?")
    for side in ("l", "r"):
        rows = []
        for f in ("thumb", "index", "middle", "ring", "pinky"):
            for i in (1, 2, 3):
                b = "%s_0%d_%s" % (f, i, side)
                live = mesh.get_socket_transform(b, unreal.RelativeTransformSpace.RTS_PARENT_BONE_SPACE).rotation
                g = APE.get_bone_pose(grip, b, unreal.AnimPoseSpaces.LOCAL).rotation
                c = APE.get_bone_pose(cpose, b, unreal.AnimPoseSpaces.LOCAL).rotation
                rows.append("%s%d live-grip %4.1f live-clip %4.1f" % (f[:2], i, ang(live, g), ang(live, c)))
        print(side, "\n  " + "\n  ".join(rows))
import gc; gc.collect()
