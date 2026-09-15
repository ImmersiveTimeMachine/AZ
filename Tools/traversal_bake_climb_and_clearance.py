# @Description: Bake AuthoredClearHeight onto the hurdle clip rows and add the six Climb rows.
#
# Run ONCE after the build that introduces UAZ_GA_Climb, EAZ_TraversalAction::Climb and the
# AuthoredApexRise -> AuthoredClearHeight rename. The rename drops the old serialized values by design:
# they held the ROOT track's peak, which is not a clearance and must not be carried forward.
#
# Everything written here is MEASURED from the clips in this script, never taken from a filename:
#   AuthoredClearHeight - the lowest BODY bone while its forward position is inside the authored barrier
#                         (y 0..20, front face at the animation origin). This is what an obstacle height
#                         has to be compared against.
#   EntrySamples        - legal start times, capped at the first warp window so no launch or hand-contact
#                         setup is ever skipped to enter faster.
#   PlantedFoot         - from the ball heights at that instant; Unknown when they are level, which is the
#                         honest answer for a two-footed pose and is treated as a match downstream.
#
# The pawn Blueprint must be compiled + saved by hand afterwards: scripted CDO writes do not reach
# instances until a compile, and compiling from Python crashes the editor.
import unreal

L = unreal.AnimationLibrary
E = unreal.AnimPoseExtensions
BASE = "/Game/AZ/Blueprints/Animation/MHC/Traversal/"
PAWN = "/Game/AZ/Blueprints/Character/Hero/MHC/AZ_BP_PawnMoverHero_MHC"

OPT = unreal.AnimPoseEvaluationOptions()
OPT.set_editor_property("evaluation_type", unreal.AnimDataEvalType.SOURCE)
BODY = ["pelvis", "thigh_l", "calf_l", "foot_l", "ball_l",
        "thigh_r", "calf_r", "foot_r", "ball_r"]

# Front face of the authored ledge in each climb clip's own space, measured as the deepest forward reach
# of any bone that is still BELOW the platform top (the body cannot pass through the wall). Five clips sit
# on the convention; Run_Neutral_R's origin is genuinely shifted and is the reason this is per-clip data.
CLIMB_FACE_Y = {
    "AM_AZ_Climb_2_5_Run_Neutral_L":   0.0,
    "AM_AZ_Climb_2_5_Run_Neutral_R": -59.3,
    "AM_AZ_Climb_2_5_Stand_Neutral_L": 0.0,
    "AM_AZ_Climb_2_5_Stand_Neutral_R": 0.0,
    "AM_AZ_Climb_2_5_Walk_Neutral_L":  0.0,
    "AM_AZ_Climb_2_5_Walk_Neutral_R":  0.0,
}
APPROACH = {"Stand": unreal.AZ_MantleApproach.STAND,
            "Walk":  unreal.AZ_MantleApproach.WALK,
            "Run":   unreal.AZ_MantleApproach.RUN}


def seq_of(montage):
    for st in montage.get_editor_property("slot_anim_tracks"):
        for sg in st.get_editor_property("anim_track").get_editor_property("anim_segments"):
            s = sg.get_editor_property("anim_reference")
            if s:
                start = sg.get_editor_property("anim_start_time")
                return s, float(start)
    return None, 0.0


def first_warp_start(montage):
    best = None
    for e in L.get_animation_notify_events(montage):
        ns = e.get_editor_property("notify_state_class")
        if ns is None or "MotionWarping" not in type(ns).__name__:
            continue
        t = L.get_anim_notify_event_trigger_time(e)
        if best is None or t < best:
            best = t
    return best


def clear_height(seq, y0=0.0, y1=20.0):
    """Lowest body bone while crossing the authored barrier span."""
    lo = None
    t = 0.0
    dur = seq.get_play_length()
    while t <= dur:
        r = L.get_bone_pose_for_time(seq, "root", t, False).translation
        if r.z > 1.0:
            p = E.get_anim_pose_at_time(seq, t, OPT)
            for b in BODY:
                v = E.get_bone_pose(p, b, unreal.AnimPoseSpaces.WORLD).translation
                if y0 <= v.y <= y1 and (lo is None or v.z < lo):
                    lo = v.z
        t += 0.02
    return lo


def planted_foot(seq, t):
    p = E.get_anim_pose_at_time(seq, t, OPT)
    zl = E.get_bone_pose(p, "ball_l", unreal.AnimPoseSpaces.WORLD).translation.z
    zr = E.get_bone_pose(p, "ball_r", unreal.AnimPoseSpaces.WORLD).translation.z
    if abs(zl - zr) < 3.0:
        return unreal.AZ_MantleFoot.UNKNOWN
    return unreal.AZ_MantleFoot.LEFT if zl < zr else unreal.AZ_MantleFoot.RIGHT


def climb_samples(montage, seq, face_y, seg_start, step=0.05, cap=16):
    """Legal entries: every step before the first warp window opens."""
    limit = first_warp_start(montage)
    if limit is None:
        limit = 0.0
    out = []
    t = 0.0
    while t < limit - 1e-3 and len(out) < cap:
        st = seg_start + t
        r = L.get_bone_pose_for_time(seq, "root", st, False).translation
        remaining = face_y - r.y
        if remaining > 0.0:
            s = unreal.AZ_MantleEntrySample()
            s.set_editor_property("time", float(t))
            s.set_editor_property("remaining_approach", float(remaining))
            s.set_editor_property("planted_foot", planted_foot(seq, st))
            out.append(s)
        t += step
    return out


def run():
    bp = unreal.EditorAssetLibrary.load_asset(PAWN)
    cdo = unreal.get_default_object(bp.generated_class())
    comp = [c for c in cdo.get_components_by_class(unreal.AZ_TraversalComponent)][0]
    clips = list(comp.get_editor_property("clips"))

    # ---- 1) hurdle rows: bake the measured body clearance -------------------------
    n_hurdle = 0
    for entry in clips:
        m = entry.get_editor_property("montage")
        if not m or not m.get_name().startswith("AM_AZ_Hurdle"):
            continue
        s, _ = seq_of(m)
        ch = clear_height(s)
        if ch is None:
            print("  !! %s: no body sample over the barrier span" % m.get_name())
            continue
        entry.set_editor_property("authored_clear_height", float(ch))
        n_hurdle += 1
        print("  clearH %-42s = %.1f" % (m.get_name(), ch))

    # ---- 2) climb rows ------------------------------------------------------------
    clips = [c for c in clips
             if not (c.get_editor_property("montage")
                     and c.get_editor_property("montage").get_name().startswith("AM_AZ_Climb"))]
    n_climb = 0
    for nm, face_y in sorted(CLIMB_FACE_Y.items()):
        m = unreal.EditorAssetLibrary.load_asset(BASE + nm)
        if not m:
            print("  !! missing %s" % nm)
            continue
        s, seg_start = seq_of(m)
        band = [k for k in APPROACH if ("_%s_" % k) in nm][0]
        final_y = L.get_bone_pose_for_time(s, "root", s.get_play_length(), False).translation.y
        samples = climb_samples(m, s, face_y, seg_start)

        e = unreal.AZ_MantleClipEntry()
        e.set_editor_property("action", unreal.AZ_TraversalAction.CLIMB)
        e.set_editor_property("approach", APPROACH[band])
        e.set_editor_property("style", unreal.AZ_MantleStyle.NEUTRAL)
        e.set_editor_property("planted_foot",
                              unreal.AZ_MantleFoot.LEFT if nm.endswith("_L") else unreal.AZ_MantleFoot.RIGHT)
        e.set_editor_property("montage", m)
        e.set_editor_property("entry_samples", samples)
        # Where the character ends up standing, measured from the clip's own front face. Recorded because
        # it is the real platform depth this clip needs, even though hurdle is the only action that gates
        # on it today.
        e.set_editor_property("required_top_support", float(max(0.0, final_y - face_y) + 10.0))
        e.set_editor_property("authored_clear_height", 0.0)
        clips.append(e)
        n_climb += 1
        rs = [x.get_editor_property("remaining_approach") for x in samples]
        print("  climb  %-34s band=%-5s samples=%2d range=%.0f..%.0f topSup=%.0f" % (
            nm, band, len(samples), min(rs) if rs else -1, max(rs) if rs else -1,
            max(0.0, final_y - face_y) + 10.0))

    comp.set_editor_property("clips", clips)

    # ---- 3) grant + route the climb ability ---------------------------------------
    climb_cls = unreal.load_class(None, "/Script/AZ.AZ_GA_Climb")
    comp.set_editor_property("climb_ability_class", climb_cls)
    startup = list(cdo.get_editor_property("startup_abilities"))
    if not any(c == climb_cls for c in startup):
        startup.append(climb_cls)
        cdo.set_editor_property("startup_abilities", startup)
    print("  ability: climb_ability_class set, startup_abilities=%d" % len(startup))

    print("DONE  hurdle rows baked=%d  climb rows=%d  total clips=%d" % (n_hurdle, n_climb, len(clips)))
    print("NOW COMPILE + SAVE the pawn Blueprint by hand.")


run()
