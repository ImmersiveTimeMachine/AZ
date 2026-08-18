# AZ / CHALK - MetaHuman hero fixup, re-applied after ANY MetaHuman re-assembly.
#
# WHY THIS EXISTS
#   The MetaHuman assembly regenerates /Game/MetaHumans/Common/** and the assembled
#   meshes under /Game/AZ/Blueprints/Character/AZ_MHC_Hero/**. Everything we configure
#   on those assets is wiped when you edit MHC_Hero in MetaHuman Creator and re-assemble.
#   Symptom when it is lost: the hero silently stops animating (compatible skeleton gone)
#   and every socket-attached item -- weapons, grab anchor, grab IK -- attaches to nothing
#   with no error and no log line.
#
# WHAT IT RE-APPLIES
#   1. metahuman_base_skel.CompatibleSkeletons          += SKEL_SurvivalMan
#      metahuman_base_skel.bUseRetargetModesFromCompatibleSkeleton = True
#      (Both live on the TARGET skeleton: the list is not bi-directional, and the runtime
#       reads the flag off the target -- AnimationDecompression.cpp:72.)
#   2. All sockets from SKEL_SurvivalMan onto SKM_MHC_Hero_BodyMesh, transforms verified.
#      Sockets live on the Skeleton/Mesh, so compatible-skeletons does NOT carry them.
#
# HOW TO RUN
#   Unreal Editor -> Output Log -> cmd mode "Python" -> exec this file, or:
#   py "C:/UnrealEngine/Games/AZ/Tools/metahuman_fixup.py"
#
# Idempotent: safe to run repeatedly. Existing managed sockets are removed and re-created.

import unreal

SV_MESH = '/Game/SurvivalMan/Meshes/SKM_SurvivalMan_Mesh1'
SV_SKEL = '/Game/SurvivalMan/Meshes/SKEL_SurvivalMan'
MH_MESH = '/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh'
MH_SKEL = '/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel'


def _fail(msg):
    print('[MH-FIXUP] FAIL: %s' % msg)
    return False


def apply_compatible_skeleton(mh_skel, sv_skel):
    existing = list(mh_skel.get_editor_property('compatible_skeletons') or [])
    if not any(s and s.get_path_name() == sv_skel.get_path_name() for s in existing):
        existing.append(sv_skel)
        mh_skel.set_editor_property('compatible_skeletons', existing)
        print('[MH-FIXUP] added SKEL_SurvivalMan to CompatibleSkeletons')
    else:
        print('[MH-FIXUP] CompatibleSkeletons already correct')
    if not mh_skel.get_editor_property('use_retarget_modes_from_compatible_skeleton'):
        mh_skel.set_editor_property('use_retarget_modes_from_compatible_skeleton', True)
        print('[MH-FIXUP] enabled bUseRetargetModesFromCompatibleSkeleton')
    else:
        print('[MH-FIXUP] bUseRetargetModesFromCompatibleSkeleton already True')


def apply_sockets(sv_mesh, mh_mesh, mh_skel):
    bones = set(str(b) for b in unreal.AZ_SkeletonUtils.get_bone_names(mh_skel))
    src = []
    for i in range(sv_mesh.num_sockets()):
        s = sv_mesh.get_socket_by_index(i)
        src.append(dict(name=str(s.get_editor_property('socket_name')),
                        bone=str(s.get_editor_property('bone_name')),
                        loc=s.get_editor_property('relative_location'),
                        rot=s.get_editor_property('relative_rotation'),
                        scl=s.get_editor_property('relative_scale')))

    # wipe managed sockets first so the pass is idempotent
    for rec in src:
        guard = 0
        while mh_mesh.find_socket(rec['name']) and guard < 12:
            mh_mesh.remove_socket(rec['name'])
            guard += 1

    added, skipped, failed = [], [], []
    for rec in src:
        if rec['bone'] not in bones:
            skipped.append((rec['name'], rec['bone']))
            continue
        try:
            ns = unreal.new_object(unreal.SkeletalMeshSocket, outer=mh_mesh)
            mh_mesh.add_socket(ns, False)  # mesh-only; True duplicates onto the skeleton
            mh_mesh.rename_socket(ns.get_editor_property('socket_name'), rec['name'])
            ns.set_socket_parent(mh_mesh, rec['bone'])
            ns.set_socket_local_transform(unreal.Transform(rec['loc'], rec['rot'], rec['scl']))
            added.append(rec['name'])
        except Exception as e:
            failed.append((rec['name'], str(e)))

    ok = True
    for rec in src:
        if rec['bone'] not in bones:
            continue
        g = mh_mesh.find_socket(rec['name'])
        if not g:
            ok = False
            print('[MH-FIXUP]   MISSING %s' % rec['name'])
            continue
        gl = g.get_editor_property('relative_location')
        gr = g.get_editor_property('relative_rotation')
        if (str(g.get_editor_property('bone_name')) != rec['bone']
                or abs(gl.x - rec['loc'].x) > 0.01 or abs(gl.y - rec['loc'].y) > 0.01
                or abs(gl.z - rec['loc'].z) > 0.01
                or abs(gr.pitch - rec['rot'].pitch) > 0.01 or abs(gr.yaw - rec['rot'].yaw) > 0.01
                or abs(gr.roll - rec['rot'].roll) > 0.01):
            ok = False
            print('[MH-FIXUP]   MISMATCH %s' % rec['name'])

    print('[MH-FIXUP] sockets added: %d %s' % (len(added), added))
    if skipped:
        # weapon_r_muzzle is expected here: bone 'weapon_r' does not exist on the MetaHuman
        # skeleton, and nothing in Source/ references that socket.
        print('[MH-FIXUP] skipped (bone absent on MetaHuman): %s' % skipped)
    if failed:
        print('[MH-FIXUP] FAILED: %s' % failed)
    print('[MH-FIXUP] verify all-match: %s' % ok)
    return ok and not failed


def main():
    assets = {}
    for key, path in (('sv_mesh', SV_MESH), ('sv_skel', SV_SKEL),
                      ('mh_mesh', MH_MESH), ('mh_skel', MH_SKEL)):
        a = unreal.load_asset(path)
        if a is None:
            return _fail('could not load %s (%s)' % (key, path))
        assets[key] = a

    apply_compatible_skeleton(assets['mh_skel'], assets['sv_skel'])
    ok = apply_sockets(assets['sv_mesh'], assets['mh_mesh'], assets['mh_skel'])

    for a in (assets['mh_mesh'], assets['mh_skel']):
        unreal.EditorAssetLibrary.save_loaded_asset(a, only_if_is_dirty=False)
    print('[MH-FIXUP] saved. RESULT: %s' % ('OK' if ok else 'NEEDS ATTENTION'))
    return ok


if __name__ == '__main__':
    main()
