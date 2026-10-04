# @Description: WGS 0.2 weapon parts dump - verts and triangle indices per Winchester static mesh part to JSON
import unreal
import json
import os

PARTS = {
    "BasePart": "/Game/AZ/Assets/Weapons/Winchester_Rifle/Meshes/SM_Winchester_BasePart",
    "Winchestere_ElitBase": "/Game/AZ/Assets/Weapons/Winchester_Rifle/Meshes/SM_Winchestere_ElitBase",
    "ChargerBase": "/Game/AZ/Assets/Weapons/Winchester_Rifle/Meshes/SM_Winchester_ChargerBase",
    "MazzleBase": "/Game/AZ/Assets/Weapons/Winchester_Rifle/Meshes/SM_Winchester_MazzleBase",
    "Shutter": "/Game/AZ/Assets/Weapons/Winchester_Rifle/Meshes/SM_Winchester_Shutter",
    "ShutterDet": "/Game/AZ/Assets/Weapons/Winchester_Rifle/Meshes/SM_Winchester_ShutterDet",
    "SightPlank": "/Game/AZ/Assets/Weapons/Winchester_Rifle/Meshes/SM_Winchester_SightPlank",
    "StockBase": "/Game/AZ/Assets/Weapons/Winchester_Rifle/Meshes/SM_Winchester_StockBase",
}

OUT_PATH = "C:/UnrealEngine/Games/AZ/Saved/wgs/weapons/winchester_parts.json"


def dump_part(sm):
    try:
        num_sections = sm.get_num_sections(0)
    except Exception:
        num_sections = 1
    verts_out = []
    tris_out = []
    vert_offset = 0
    for s in range(num_sections):
        verts, tris, normals, uvs, tangents = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, s)
        for v in verts:
            verts_out.append([v.x, v.y, v.z])
        for idx in tris:
            tris_out.append(int(idx) + vert_offset)
        vert_offset += len(verts)
    return verts_out, tris_out


def run():
    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    result = {}
    summary = {}
    total_tris = 0
    for name, path in PARTS.items():
        sm = unreal.load_asset(path)
        if sm is None:
            summary[name] = "LOAD FAILED"
            continue
        verts, tris = dump_part(sm)
        num_tris = len(tris) // 3
        result[name] = {"path": path, "verts": verts, "tris": tris}
        summary[name] = {"verts": len(verts), "tris": num_tris}
        total_tris += num_tris

    with open(OUT_PATH, "w") as f:
        json.dump(result, f)

    summary["total_tris"] = total_tris
    summary["out_path"] = OUT_PATH
    print(json.dumps(summary, indent=2))


run()
import gc; gc.collect()
