# Read-only export for the all-clip hand-on-gun scan. Per pack mesh: its gun geometry (gun bone space). Our models:
# Winchester + Remington (ProceduralMeshLibrary, no UObjects created). Per clip (every STEP frames): pack hands + gun bone,
# master hands + az_weapon_r (on SKM_AZ_Master). Output Saved/az_gun_scan.json
import json, os
import unreal

APE = unreal.AnimPoseExtensions
EAL = unreal.EditorAssetLibrary
WORLD = unreal.AnimPoseSpaces.WORLD
MH_ROOT, MH_PREFIX = "/Game/AZ/Assets/RifleMega/", "AZ_RTG_MH_"
PACK_ROOT = "/Game/RifleMega_MocapAnimPack/AnimationsFBX/"
DST_ROOT, DST_PREFIX = "/Game/AZ/Assets/Master/RifleMega/", "AZ_MST_"
MASTER_MESH = unreal.load_asset("/Game/AZ/Assets/Characters/Master/SKM_AZ_Master")
OUT = os.path.join(unreal.Paths.project_saved_dir(), "az_gun_scan.json")
STEP, MAXF = 8, 18
BONES = ["hand_r", "middle_01_r", "hand_l", "middle_01_l"]


def tr(t):
    q = t.rotation
    return [round(t.translation.x, 3), round(t.translation.y, 3), round(t.translation.z, 3), q.x, q.y, q.z, q.w]


def opts(mesh):
    o = unreal.AnimPoseEvaluationOptions()
    o.set_editor_property("evaluation_type", unreal.AnimDataEvalType.RAW)
    o.set_editor_property("should_retarget", True)
    o.set_editor_property("extract_root_motion", False)
    o.set_editor_property("optional_skeletal_mesh", mesh)
    return o


def pick(res, cls):
    items = res if isinstance(res, tuple) else (res,)
    return [x for x in items if isinstance(x, cls)]


def gun_bone_of(skel):
    ref = APE.get_reference_pose(skel)
    names = [str(n) for n in APE.get_bone_names(ref)]
    g = [n for n in names if n.endswith("Mesh")]
    return (g[0] if g else None), ref


def static_pts(path):
    sm = unreal.load_asset(path)
    pts = []
    for s in range(sm.get_num_sections(0)):
        r = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, s)
        verts = r[0]
        pts += [[round(v.x, 2), round(v.y, 2), round(v.z, 2)] for v in verts]
    return pts


data = {"models": {}, "sockets": {}, "pack_guns": {}, "clips": []}
data["models"]["winchester"] = static_pts("/Game/AZ/Assets/Weapons/Winchester_Rifle/SM_Winchester_Whole")
rem = [p for p in EAL.list_assets("/Game/AZ/Assets/Weapons/Remington870_Shotgun", recursive=False, include_folder=False) if "Whole" in p]
data["remington_path"] = rem[0].split(".")[0] if rem else None
if rem:
    data["models"]["remington"] = static_pts(rem[0].split(".")[0])
for key, sock in (("winchester", "RightHandWinchesterSocket"), ("remington", "RightHandShotgunSocket")):
    s = MASTER_MESH.find_socket(sock)
    data["sockets"][key] = None if not s else [s.relative_location.x, s.relative_location.y, s.relative_location.z] + \
        tr(unreal.Transform(s.relative_location, s.relative_rotation, s.relative_scale))[3:] + [str(s.bone_name)]

# pack meshes by skeleton
pack_mesh_by_skel = {}
for p in EAL.list_assets("/Game/RifleMega_MocapAnimPack/Demo/Models", recursive=True, include_folder=False):
    a = EAL.find_asset_data(p)
    if a.asset_class_path.asset_name == "SkeletalMesh":
        m = unreal.load_asset(p.split(".")[0])
        pack_mesh_by_skel.setdefault(m.skeleton.get_path_name(), m)
dm = unreal.DynamicMesh()          # ONE dynamic mesh, reused and kept referenced (no GC churn)
for skel_path, m in pack_mesh_by_skel.items():
    gname, ref = gun_bone_of(m.skeleton)
    dm.reset()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(m, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    gidx = [x for x in pick(unreal.GeometryScript_BoneWeights.get_bone_index(dm, gname), int) if not isinstance(x, bool)][0]
    pos = pick(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(dm, False), unreal.GeometryScriptVectorList)[0]
    vl = list(unreal.GeometryScript_List.convert_vector_list_to_array(pos))
    g_ref = APE.get_bone_pose(ref, gname, WORLD)
    pts = []
    for vid, v in enumerate(vl):
        bw = pick(unreal.GeometryScript_BoneWeights.get_largest_vertex_bone_weight(dm, vid), unreal.GeometryScriptBoneWeight)[0]
        if bw.bone_index == gidx and bw.weight > 0.5:
            lp = g_ref.inverse_transform_location(v)
            pts.append([round(lp.x, 2), round(lp.y, 2), round(lp.z, 2)])
    data["pack_guns"][skel_path] = {"mesh": m.get_name(), "gun_bone": gname, "pts": pts}
dm.reset()

mh_clips = sorted(p.split(".")[0] for p in EAL.list_assets(MH_ROOT, recursive=True, include_folder=False)
                  if p.rsplit("/", 1)[-1].startswith(MH_PREFIX))
for mh_path in mh_clips:
    sub, name = mh_path[len(MH_ROOT):].rsplit("/", 1)
    base = name[len(MH_PREFIX):]
    pack = unreal.load_asset(PACK_ROOT + sub + "/" + base)
    mst = unreal.load_asset(DST_ROOT + sub + "/" + DST_PREFIX + base)
    if not (pack and mst):
        continue
    skel_path = pack.get_editor_property("skeleton").get_path_name()
    pm = pack_mesh_by_skel.get(skel_path)
    if pm is None:
        data["clips"].append({"name": sub + "/" + base, "error": "no pack mesh for " + skel_path})
        continue
    gname = data["pack_guns"][skel_path]["gun_bone"]
    n = min(unreal.AnimationLibrary.get_num_keys(pack), unreal.AnimationLibrary.get_num_keys(mst))
    step = max(STEP, (n + MAXF - 1) // MAXF)
    frames = sorted(set(list(range(0, n, step)) + [n - 1]))
    op, om = opts(pm), opts(MASTER_MESH)
    rows = []
    for f in frames:
        pp = APE.get_anim_pose_at_frame(pack, f, op)
        pmst = APE.get_anim_pose_at_frame(mst, f, om)
        rows.append({"f": f,
                     "pack": {b: tr(APE.get_bone_pose(pp, b, WORLD)) for b in BONES + [gname]},
                     "mst": {b: tr(APE.get_bone_pose(pmst, b, WORLD)) for b in BONES + ["az_weapon_r"]}})
    data["clips"].append({"name": sub + "/" + base, "skel": skel_path, "frames": rows})
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(data, fh)
unreal.log("AZSCAN wrote %s: %d clips, pack guns %s, models %s, sockets %s"
           % (OUT, len(data["clips"]), {v["mesh"]: len(v["pts"]) for v in data["pack_guns"].values()},
              {k: len(v) for k, v in data["models"].items()}, {k: (v[-1] if v else None) for k, v in data["sockets"].items()}))
