"""RifleMega -> master skeleton: the gun path for az_weapon_r.

The pack animates the gun as a rigid bone (<Name>Mesh under ik_hand_gun). In reloads it moves up to ~20 cm relative
to hand_r. The master skeleton carries it as az_weapon_r (child of hand_r, identity at rest; weapon sockets sit on it).

Per frame, all in UE transform order (A*B = A local to B):
    R(t)     = gun relative to the pack's hand_r                 (make_relative(G, H))
    R_hold   = R at the clip's first frame (the hold the clip starts from - keeps idle->reload blends seamless)
    T(t)     = inv(R_hold) * R(t)                                 gun motion, in the pack hand's space
    K(t)     = rotation of the hero's hand_r relative to the pack's hand_r (the two hands' axes differ)
    A(t)     = K * T * inv(K)                                     the same motion in the hero hand's space
A(t) is the az_weapon_r local track. A == identity whenever the gun is in its hold.

Run in the editor (Python):   MODE = "analyze";  exec(open(r"C:/UnrealEngine/Games/AZ/Tools/az_master_riflemega.py").read())
Report: Saved/az_master_riflemega_report.txt
"""
import json
import math
import os
import unreal

MODE = globals().get("MODE", "analyze")
SRC_CLIP = globals().get("SRC_CLIP", "/Game/RifleMega_MocapAnimPack/AnimationsFBX/Rifle_ReloadingSet/Winchester/Rifle01_St_Reload_Winch")
TGT_CLIP = globals().get("TGT_CLIP", "/Game/AZ/Assets/RifleMega/Rifle_ReloadingSet/Winchester/AZ_RTG_MH_Rifle01_St_Reload_Winch")
HOLD_CLIP = globals().get("HOLD_CLIP", "/Game/RifleMega_MocapAnimPack/AnimationsFBX/Rifle_Styly01_St/Rifle01_IdleSet/Rifle01_St_Idle00")
REPORT = os.path.join(unreal.Paths.project_saved_dir(), "az_master_riflemega_report.txt")

APE = unreal.AnimPoseExtensions
ML = unreal.MathLibrary
EAL = unreal.EditorAssetLibrary
WORLD = unreal.AnimPoseSpaces.WORLD
lines = []


def log(msg):
    lines.append(msg)
    unreal.log("AZRIFLE " + msg)


def opts():
    o = unreal.AnimPoseEvaluationOptions()
    o.set_editor_property("evaluation_type", unreal.AnimDataEvalType.RAW)
    o.set_editor_property("should_retarget", False)
    o.set_editor_property("extract_root_motion", False)
    return o


def gun_bone(pose):
    names = [str(n) for n in APE.get_bone_names(pose)]
    guns = [n for n in names if n.endswith("Mesh")]
    return guns[0] if guns else None


def world(pose, bone):
    return APE.get_bone_pose(pose, bone, WORLD)


def rel(child, parent):
    return ML.make_relative_transform(child, parent)       # child * inv(parent)


def mul(a, b):
    return ML.compose_transforms(a, b)                      # a * b


def inv(t):
    return ML.invert_transform(t)


def rot_only(t):
    return unreal.Transform(unreal.Vector(0.0, 0.0, 0.0), t.rotation.rotator(), unreal.Vector(1.0, 1.0, 1.0))


def angle_deg(t):
    q = t.rotation
    return math.degrees(2.0 * math.acos(min(1.0, abs(q.w))))


def dist(a, b):
    return (a - b).length()


def gun_track(src, tgt, r_hold=None):
    """Per-frame A(t) (az_weapon_r local) plus the left-hand check. Frames of src and tgt must match.
    r_hold None = the clip's first frame; pass the pack-wide hold for clips that do not start in the hold."""
    o = opts()
    n = min(unreal.AnimationLibrary.get_num_keys(src), unreal.AnimationLibrary.get_num_keys(tgt))
    p0 = APE.get_anim_pose_at_frame(src, 0, o)
    gun = gun_bone(p0)
    if r_hold is None:
        r_hold = rel(world(p0, gun), world(p0, "hand_r"))
    k0 = None
    track, rows = [], []
    for f in range(n):
        ps = APE.get_anim_pose_at_frame(src, f, o)
        pt = APE.get_anim_pose_at_frame(tgt, f, o)
        hs, gs, ls = world(ps, "hand_r"), world(ps, gun), world(ps, "hand_l")
        ht, lt = world(pt, "hand_r"), world(pt, "hand_l")
        r = rel(gs, hs)
        k = rot_only(rel(ht, hs))
        if k0 is None:
            k0 = k
        a = mul(mul(k, mul(inv(r_hold), r)), inv(k))
        track.append(a)
        # left hand in gun space: pack vs hero (gun moved by A) vs hero (gun frozen in the hand, today's sockets)
        virt_hold = mul(r_hold, inv(k))                         # pack gun frame riding the hero hand
        l_src = rel(ls, gs).translation
        l_moved = rel(lt, mul(virt_hold, mul(a, ht))).translation
        l_frozen = rel(lt, mul(virt_hold, ht)).translation
        rows.append((f, a.translation.length(), angle_deg(a), dist(l_src, l_moved), dist(l_src, l_frozen),
                     angle_deg(rel(k, k0))))
    return gun, r_hold, track, rows


def analyze():
    src, tgt, hold = unreal.load_asset(SRC_CLIP), unreal.load_asset(TGT_CLIP), unreal.load_asset(HOLD_CLIP)
    log("clips: %s (%d keys) -> %s (%d keys)" % (src.get_name(), unreal.AnimationLibrary.get_num_keys(src), tgt.get_name(),
                                                 unreal.AnimationLibrary.get_num_keys(tgt)))
    gun, r_hold, track, rows = gun_track(src, tgt)
    o = opts()
    ph = APE.get_anim_pose_at_frame(hold, 0, o)
    gh = gun_bone(ph)
    r_idle = rel(world(ph, gh), world(ph, "hand_r"))
    d_hold = rel(r_hold, r_idle)
    log("gun bone %s | hold at reload frame 0 vs idle %s: %.2f cm, %.2f deg" % (gun, hold.get_name(), d_hold.translation.length(), angle_deg(d_hold)))
    log("pack hold (gun relative to hand_r): loc %s rot %s" % (r_hold.translation, r_hold.rotation.rotator()))
    peak_t = max(rows, key=lambda r: r[1])
    peak_a = max(rows, key=lambda r: r[2])
    log("az_weapon_r motion: max %.2f cm (frame %d), max %.2f deg (frame %d); last frame %.2f cm %.2f deg"
        % (peak_t[1], peak_t[0], peak_a[2], peak_a[0], rows[-1][1], rows[-1][2]))
    log("hero hand axes vs pack hand axes drift over the clip: max %.2f deg (0 = retarget kept the hand orientation)"
        % max(r[5] for r in rows))
    moved = [r[3] for r in rows]
    frozen = [r[4] for r in rows]
    log("left hand vs gun, error to the pack: WITH gun motion mean %.2f / max %.2f cm | gun FROZEN in hand mean %.2f / max %.2f cm"
        % (sum(moved) / len(moved), max(moved), sum(frozen) / len(frozen), max(frozen)))
    for r in rows[::10]:
        log("   f%3d  gun %6.2f cm %6.2f deg | left-hand error moved %5.2f  frozen %5.2f cm | hand-axis drift %.2f deg"
            % (r[0], r[1], r[2], r[3], r[4], r[5]))


MASTER_SKELETON = "/Game/AZ/Blueprints/Character/Master/SK_AZ_Master"
MASTER_MESH = "/Game/AZ/Assets/Characters/Master/SKM_AZ_Master"
MH_ROOT, MH_PREFIX = "/Game/AZ/Assets/RifleMega/", "AZ_RTG_MH_"
PACK_ROOT = "/Game/RifleMega_MocapAnimPack/AnimationsFBX/"
DST_ROOT, DST_PREFIX = "/Game/AZ/Assets/Master/RifleMega/", "AZ_MST_"
NAMES = globals().get("NAMES", [TGT_CLIP])          # MetaHuman clip paths to convert
CHUNK = globals().get("CHUNK", None)                 # index into CHUNKS_FILE instead of NAMES (batch run)
CHUNK_SIZE = 20
CHUNKS_FILE = os.path.join(unreal.Paths.project_saved_dir(), "az_master_riflemega_chunks.json")
WEAPON_BONE = "az_weapon_r"


def paths_for(mh_path):
    sub, name = mh_path[len(MH_ROOT):].rsplit("/", 1)
    base = name[len(MH_PREFIX):]
    return PACK_ROOT + sub + "/" + base, DST_ROOT + sub, DST_PREFIX + base


def pack_hold():
    ph = APE.get_anim_pose_at_frame(unreal.load_asset(HOLD_CLIP), 0, opts())
    return rel(world(ph, gun_bone(ph)), world(ph, "hand_r"))


def convert_one(mh_path, r_hold):
    """MetaHuman clip -> master clip: every bone track copied key-for-key, float curves and root-motion flags copied,
    az_weapon_r = the pack gun path. Never deletes (a delete in the same Python call froze the editor once)."""
    AL = unreal.AnimationLibrary
    mh = unreal.load_asset(mh_path)
    pack_path, dst_dir, dst_name = paths_for(mh_path)
    pack = unreal.load_asset(pack_path)
    if not (mh and pack):
        log("SKIP %s: mh %s pack %s" % (mh_path, bool(mh), bool(pack)))
        return False
    n_keys, n_frames = AL.get_num_keys(mh), AL.get_num_frames(mh)
    fps = int(round(n_frames / mh.get_play_length())) if mh.get_play_length() > 0 else 30
    tracks = [str(t) for t in AL.get_animation_track_names(mh)]
    o = opts()
    keys = {b: ([], [], []) for b in tracks}
    for f in range(n_keys):
        pose = APE.get_anim_pose_at_frame(mh, f, o)
        for b in tracks:
            t = APE.get_bone_pose(pose, b, unreal.AnimPoseSpaces.LOCAL)
            keys[b][0].append(t.translation)
            keys[b][1].append(t.rotation)
            keys[b][2].append(t.scale3d)
    gun, _, gun_path, rows = gun_track(pack, mh, r_hold)
    first_off = rows[0][1], rows[0][2]

    dst_path = dst_dir + "/" + dst_name
    dst = unreal.load_asset(dst_path) if EAL.does_asset_exist(dst_path) else None
    if dst is None:
        factory = unreal.AnimSequenceFactory()
        factory.set_editor_property("target_skeleton", unreal.load_asset(MASTER_SKELETON))
        dst = unreal.AssetToolsHelpers.get_asset_tools().create_asset(dst_name, dst_dir, unreal.AnimSequence, factory)
    c = dst.controller
    c.open_bracket("AZ master RifleMega convert", False)
    try:
        c.remove_all_bone_tracks(False)
        c.set_frame_rate(unreal.FrameRate(fps, 1), False)
        c.set_number_of_frames(unreal.FrameNumber(n_frames), False)
        for b in tracks:
            c.add_bone_curve(unreal.Name(b), False)
            if not c.set_bone_track_keys(unreal.Name(b), keys[b][0], keys[b][1], keys[b][2], False):
                raise RuntimeError("set_bone_track_keys failed on %s" % b)
        c.add_bone_curve(unreal.Name(WEAPON_BONE), False)
        if not c.set_bone_track_keys(unreal.Name(WEAPON_BONE), [a.translation for a in gun_path], [a.rotation for a in gun_path],
                                     [unreal.Vector(1.0, 1.0, 1.0) for _ in gun_path], False):
            raise RuntimeError("set_bone_track_keys failed on %s" % WEAPON_BONE)
    finally:
        c.close_bracket(False)
    n_curves = 0
    for cname in AL.get_animation_curve_names(mh, unreal.RawCurveTrackTypes.RCT_FLOAT):
        times, values = AL.get_float_keys(mh, cname)
        if not AL.does_curve_exist(dst, cname, unreal.RawCurveTrackTypes.RCT_FLOAT):
            AL.add_curve(dst, cname, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
        AL.add_float_curve_keys(dst, cname, times, values)
        n_curves += 1
    for p in ("enable_root_motion", "root_motion_root_lock", "force_root_lock", "use_normalized_root_motion_scale",
              "retarget_source"):
        try:
            dst.set_editor_property(p, mh.get_editor_property(p))
        except Exception:
            pass
    # 142 MetaHuman bones use AnimationRelative translation retargeting: length = anim + (mesh ref - retarget source
    # ref). The MetaHuman clips name the hero body as their source; without one the engine falls back to the skeleton's
    # template pose and the arms/fingers shift by up to 6.6 cm. SKM_AZ_Master has exactly the hero's ref pose.
    dst.set_editor_property("retarget_source_asset", unreal.load_asset(MASTER_MESH))
    notifies = len(AL.get_animation_notify_events(mh))
    EAL.save_asset(dst_path, only_if_is_dirty=False)

    # read back: every copied track exact, the weapon track equal to the computed path
    worst = 0.0
    for f in range(0, n_keys, 5):
        pa = APE.get_anim_pose_at_frame(mh, f, o)
        pb = APE.get_anim_pose_at_frame(dst, f, o)
        for b in tracks:
            ta, tb = APE.get_bone_pose(pa, b, unreal.AnimPoseSpaces.LOCAL), APE.get_bone_pose(pb, b, unreal.AnimPoseSpaces.LOCAL)
            worst = max(worst, (ta.translation - tb.translation).length(), angle_deg(rel(ta, tb)) / 57.2958)
        tw = APE.get_bone_pose(pb, WEAPON_BONE, unreal.AnimPoseSpaces.LOCAL)
        worst = max(worst, (tw.translation - gun_path[f].translation).length(), angle_deg(rel(tw, gun_path[f])) / 57.2958)
    moved = [r[3] for r in rows]
    frozen = [r[4] for r in rows]
    ok = worst < 1e-3
    log("%s %s -> %s | %d keys %d fps, %d tracks + %s, %d curves, %d notifies (not copied) | gun %s: max %.1f cm %.1f deg, "
        "frame0 off hold %.2f cm %.2f deg | left hand on gun: moved max %.2f cm, frozen max %.2f cm | read-back worst %.1e"
        % ("OK" if ok else "FAIL", mh.get_name(), dst_path, n_keys, fps, len(tracks), WEAPON_BONE, n_curves, notifies, gun,
           max(r[1] for r in rows), max(r[2] for r in rows), first_off[0], first_off[1], max(moved), max(frozen), worst))
    return ok


def all_mh_clips():
    return sorted(p.split(".")[0] for p in EAL.list_assets(MH_ROOT, recursive=True, include_folder=False)
                  if p.rsplit("/", 1)[-1].startswith(MH_PREFIX))


def survey(step=10):
    """Read-only: per pack clip, how far the gun leaves the pack-wide hold (sampled every `step` frames)."""
    hold = pack_hold()
    o = opts()
    moving, off_start, missing, still = [], [], [], 0
    for mh_path in all_mh_clips():
        pack_path = paths_for(mh_path)[0]
        pack = unreal.load_asset(pack_path)
        if not pack:
            missing.append(pack_path)
            continue
        n = unreal.AnimationLibrary.get_num_keys(pack)
        frames = sorted(set(list(range(0, n, step)) + [n - 1]))
        gun, peak, first = None, (0.0, 0.0), None
        for f in frames:
            p = APE.get_anim_pose_at_frame(pack, f, o)
            gun = gun or gun_bone(p)
            d = rel(rel(world(p, gun), world(p, "hand_r")), hold)
            cm, deg = d.translation.length(), angle_deg(d)
            if first is None:
                first = (cm, deg)
            if cm + deg / 10.0 > peak[0] + peak[1] / 10.0:
                peak = (cm, deg)
        name = mh_path.rsplit("/", 1)[-1][len(MH_PREFIX):]
        if first[0] > 0.5 or first[1] > 1.0:
            off_start.append("%s (start %.1f cm %.1f deg)" % (name, first[0], first[1]))
        if peak[0] > 1.0 or peak[1] > 2.0:
            moving.append("%s (max %.1f cm %.1f deg)" % (name, peak[0], peak[1]))
        else:
            still += 1
    log("survey: %d clips, gun stays in the hold in %d, moves in %d, starts off the hold in %d, pack clip missing %d"
        % (still + len(moving) + len(missing), still, len(moving), len(off_start), len(missing)))
    log("moving: " + "; ".join(moving))
    log("starting off the hold: " + "; ".join(off_start))
    if missing:
        log("missing pack clips: " + "; ".join(missing))


def check_all():
    """Read-only: every MetaHuman clip has its master copy, with all tracks + az_weapon_r, the same key count, the
    master mesh as retarget source and the master skeleton."""
    AL = unreal.AnimationLibrary
    master_mesh = MASTER_MESH + "." + MASTER_MESH.rsplit("/", 1)[-1]
    bad, ok = [], 0
    for mh_path in all_mh_clips():
        _, dst_dir, dst_name = paths_for(mh_path)
        dst_path = dst_dir + "/" + dst_name
        if not EAL.does_asset_exist(dst_path):
            bad.append("%s missing" % dst_name)
            continue
        mh, dst = unreal.load_asset(mh_path), unreal.load_asset(dst_path)
        tracks = set(str(t) for t in AL.get_animation_track_names(dst))
        want = set(str(t) for t in AL.get_animation_track_names(mh)) | {WEAPON_BONE}
        src_asset = dst.get_editor_property("retarget_source_asset")
        problems = []
        if tracks != want:
            problems.append("tracks %d/%d" % (len(tracks), len(want)))
        if AL.get_num_keys(dst) != AL.get_num_keys(mh):
            problems.append("keys %d/%d" % (AL.get_num_keys(dst), AL.get_num_keys(mh)))
        if not src_asset or src_asset.get_path_name() != master_mesh:
            problems.append("retarget source %s" % (src_asset.get_path_name() if src_asset else None))
        if dst.get_editor_property("skeleton").get_path_name().split(".")[0] != MASTER_SKELETON:
            problems.append("skeleton")
        if problems:
            bad.append("%s: %s" % (dst_name, ", ".join(problems)))
        else:
            ok += 1
    log("CHECK_ALL %d OK, %d bad%s" % (ok, len(bad), (": " + "; ".join(bad[:30])) if bad else ""))


lines.append("MODE %s" % MODE)
if MODE == "check_all":
    check_all()
elif MODE == "survey":
    survey()
elif MODE == "analyze":
    analyze()
elif MODE == "plan":
    # batches for the batch run; one clip is converted end to end (incl. save) before the next - big save batches
    # froze the editor during the original retarget
    clips = all_mh_clips()
    chunks = [clips[i:i + CHUNK_SIZE] for i in range(0, len(clips), CHUNK_SIZE)]
    with open(CHUNKS_FILE, "w", encoding="utf-8") as fh:
        json.dump(chunks, fh, indent=1)
    log("PLAN %d clips in %d chunks (0..%d) -> %s" % (len(clips), len(chunks), len(chunks) - 1, CHUNKS_FILE))
elif MODE == "convert":
    # hold = each clip's first frame: the pack holds the Auto and DB guns 1.7 / 2.7 cm differently from the others,
    # and every clip in which the gun moves starts in its own hold (survey 2026-09-25)
    # AFTER converting run Tools/az_master_left_grip.py: the copied MetaHuman clips release the left hand from the gun
    # while the hands are >50 cm apart (reloads, pickups) - that pass puts it back on the grip.
    if CHUNK is not None:
        with open(CHUNKS_FILE, encoding="utf-8") as fh:
            NAMES = json.load(fh)[CHUNK]
    results = [convert_one(p, None) for p in NAMES]
    log("CONVERT chunk %s: %d/%d OK" % (CHUNK, sum(1 for r in results if r), len(results)))
with open(REPORT, "w", encoding="utf-8") as fh:
    fh.write("\n".join(lines) + "\n")
import gc
gc.collect()
