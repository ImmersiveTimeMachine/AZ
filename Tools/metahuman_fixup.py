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
#   3. Master skeleton (Tools/az_master_skeleton.py): metahuman_base_skel.CompatibleSkeletons += SK_AZ_Master,
#      and the master's extra leaf bones (az_weapon_r, az_prop_r, az_prop_l) added to the hero body -- a track
#      for a bone the mesh lacks is dropped, so without them weapons have nothing to ride on.
#   4. Hero sockets from Tools/hero_sockets.json (in git). Every run FIRST merges the hero's current sockets into
#      the file (update/insert, never delete), so hand tuning is always captured; at the END every socket in the
#      file is put back on the hero (created or overwritten), so tuned values win over the SurvivalMan copies of
#      step 2 and hero-only sockets (shotgun, Winchester, pistol, grenade...) survive a re-assembly.
#      To drop a socket for good, delete it on the mesh AND from the json.
#
# HOW TO RUN
#   Unreal Editor -> Output Log -> cmd mode "Python" -> exec this file, or:
#   py "C:/UnrealEngine/Games/AZ/Tools/metahuman_fixup.py"
#
# Idempotent: safe to run repeatedly. Existing managed sockets are removed and re-created.

import json
import os

import unreal

SV_MESH = '/Game/SurvivalMan/Meshes/SKM_SurvivalMan_Mesh1'
SV_SKEL = '/Game/SurvivalMan/Meshes/SKEL_SurvivalMan'
MH_MESH = '/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh'
MH_SKEL = '/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel'
MASTER_SKEL = '/Game/AZ/Blueprints/Character/Master/SK_AZ_Master'
AZ_MASTER_SCRIPT = 'C:/UnrealEngine/Games/AZ/Tools/az_master_skeleton.py'
SOCKETS_JSON = 'C:/UnrealEngine/Games/AZ/Tools/hero_sockets.json'


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


def apply_master(mh_skel):
    master = unreal.load_asset(MASTER_SKEL)
    if master is None:
        return _fail('could not load %s' % MASTER_SKEL)
    existing = list(mh_skel.get_editor_property('compatible_skeletons') or [])
    if not any(s and s.get_path_name() == master.get_path_name() for s in existing):
        existing.append(master)
        mh_skel.set_editor_property('compatible_skeletons', existing)
        print('[MH-FIXUP] added SK_AZ_Master to CompatibleSkeletons')
    else:
        print('[MH-FIXUP] SK_AZ_Master already in CompatibleSkeletons')
    # adds only the missing extra bones (idempotent), saves mesh + skeleton, then verifies bone-for-bone vs the master
    ns = {'MODE': 'add_bones', 'TARGET_MESH': MH_MESH, '__name__': 'az_master_skeleton'}
    exec(open(AZ_MASTER_SCRIPT, encoding='utf-8').read(), ns)
    ok = any(line == 'VERIFY ALL PASS' for line in ns.get('lines', []))
    print('[MH-FIXUP] master bones: %s (report Saved/az_master_report.txt)' % ('OK' if ok else 'FAIL'))
    return ok


def _load_socket_file():
    if not os.path.exists(SOCKETS_JSON):
        return {}
    with open(SOCKETS_JSON, encoding='utf-8') as f:
        return json.load(f)


def snapshot_sockets(mh_mesh):
    data = _load_socket_file()
    seen = []
    for i in range(mh_mesh.num_sockets()):
        s = mh_mesh.get_socket_by_index(i)
        l = s.get_editor_property('relative_location')
        r = s.get_editor_property('relative_rotation')
        c = s.get_editor_property('relative_scale')
        name = str(s.get_editor_property('socket_name'))
        data[name] = {'bone': str(s.get_editor_property('bone_name')), 'loc': [l.x, l.y, l.z],
                      'rot_pitch_yaw_roll': [r.pitch, r.yaw, r.roll], 'scale': [c.x, c.y, c.z]}
        seen.append(name)
    with open(SOCKETS_JSON, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=1, sort_keys=True)
    only_file = sorted(n for n in data if n not in seen)
    print('[MH-FIXUP] socket snapshot: %d from the hero, %d only in the file %s' % (len(seen), len(only_file), only_file))
    return data


def _socket_matches(s, rec, tol):
    l = s.get_editor_property('relative_location')
    r = s.get_editor_property('relative_rotation')
    c = s.get_editor_property('relative_scale')
    got = [l.x, l.y, l.z, r.pitch, r.yaw, r.roll, c.x, c.y, c.z]
    want = list(rec['loc']) + list(rec['rot_pitch_yaw_roll']) + list(rec['scale'])
    return str(s.get_editor_property('bone_name')) == rec['bone'] and max(abs(a - b) for a, b in zip(got, want)) <= tol


def restore_sockets(mh_mesh, mh_skel, data):
    bones = set(str(b) for b in unreal.AZ_SkeletonUtils.get_bone_names(mh_skel))
    created, updated, unchanged, skipped, bad = [], [], [], [], []
    for name, rec in sorted(data.items()):
        if rec['bone'] not in bones:
            skipped.append((name, rec['bone']))
            continue
        p, y, r = rec['rot_pitch_yaw_roll']
        xf = unreal.Transform(unreal.Vector(*rec['loc']), unreal.Rotator(roll=r, pitch=p, yaw=y), unreal.Vector(*rec['scale']))
        s = mh_mesh.find_socket(name)
        if s and _socket_matches(s, rec, 1e-9):
            unchanged.append(name)   # never rewrite a matching socket: a rotator round trip would nudge tuned values
            continue
        if not s:
            s = unreal.new_object(unreal.SkeletalMeshSocket, outer=mh_mesh)
            mh_mesh.add_socket(s, False)  # mesh-only
            mh_mesh.rename_socket(s.get_editor_property('socket_name'), name)
            created.append(name)
        else:
            updated.append(name)
        s.set_socket_parent(mh_mesh, rec['bone'])
        s.set_socket_local_transform(xf)
    for name, rec in sorted(data.items()):
        if rec['bone'] not in bones:
            continue
        s = mh_mesh.find_socket(name)
        if not s or not _socket_matches(s, rec, 1e-3):
            bad.append(name)
    print('[MH-FIXUP] sockets from file: created %s, re-set %s, unchanged %d, skipped %s, mismatch %s'
          % (created, updated, len(unchanged), skipped, bad))
    return not bad


def apply_sockets(sv_mesh, mh_mesh, mh_skel, keep_names=()):
    bones = set(str(b) for b in unreal.AZ_SkeletonUtils.get_bone_names(mh_skel))
    src = []
    for i in range(sv_mesh.num_sockets()):
        s = sv_mesh.get_socket_by_index(i)
        src.append(dict(name=str(s.get_editor_property('socket_name')),
                        bone=str(s.get_editor_property('bone_name')),
                        loc=s.get_editor_property('relative_location'),
                        rot=s.get_editor_property('relative_rotation'),
                        scl=s.get_editor_property('relative_scale')))

    # The rifle hand sockets are TUNED PER SKELETON: the MetaHuman hand bone is not oriented like
    # SurvivalMan's, and since 2026-09-09 the rifle set plays native MetaHuman clips, so SurvivalMan's
    # socket values are wrong here by construction. Keep whatever is on the mesh; only create them
    # (from SurvivalMan, as a starting point) when they are missing after a re-assembly.
    # Sockets recorded in hero_sockets.json are the hero's own tuning: never overwrite them with SurvivalMan values.
    preserve = ('RightHandRifleSocketAim', 'RightHandRifleSocketRelaxed') + tuple(keep_names)
    preserved = [n for n in preserve if mh_mesh.find_socket(n)]

    # wipe managed sockets first so the pass is idempotent
    for rec in src:
        if rec['name'] in preserved:
            continue
        guard = 0
        while mh_mesh.find_socket(rec['name']) and guard < 12:
            mh_mesh.remove_socket(rec['name'])
            guard += 1

    added, skipped, failed = [], [], []
    for rec in src:
        if rec['name'] in preserved:
            continue
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
        if rec['bone'] not in bones or rec['name'] in preserved:
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
    if preserved:
        print('[MH-FIXUP] preserved (tuned for the MetaHuman hand, not copied): %s' % preserved)
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

    socket_data = snapshot_sockets(assets['mh_mesh'])
    apply_compatible_skeleton(assets['mh_skel'], assets['sv_skel'])
    ok_master = apply_master(assets['mh_skel'])
    ok = apply_sockets(assets['sv_mesh'], assets['mh_mesh'], assets['mh_skel'], tuple(socket_data)) and ok_master
    ok = restore_sockets(assets['mh_mesh'], assets['mh_skel'], socket_data) and ok

    for a in (assets['mh_mesh'], assets['mh_skel']):
        unreal.EditorAssetLibrary.save_loaded_asset(a, only_if_is_dirty=False)
    print('[MH-FIXUP] saved. RESULT: %s' % ('OK' if ok else 'NEEDS ATTENTION'))
    return ok


if __name__ == '__main__':
    main()
