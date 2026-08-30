# CMC jump — clip authoring layout (RECOVERY SCRIPT, do not run casually)
#
# WHY THIS FILE EXISTS
# The jump source clips live under /Content/Assets/RTG_AZ/ which .gitignore excludes
# (LFS budget, rule at .gitignore:65). Their notify layout, root-lock flags and gameplay
# event beats are load-bearing for jump selection AND for the jump ability's lifetime,
# but none of it is in version control. This script re-applies all of it after a fresh
# clone / asset-pack reimport.
#
# The PoseSearch databases under /Game/AZ/.../MotionMatching/CMC/ ARE tracked, so their
# membership comes back on its own — but membership is OWNED by each clip's BranchIn
# notify (PreSaveRoot -> SynchronizeWithExternalDependencies), so re-pointing a BranchIn
# here silently moves a clip between databases on the next save. Change with care.
#
# ★ DO NOT RUN against a working project to "verify" — it REMOVES all PoseSearch notifies
# first and rewrites them. Run it only when the clips have lost their authoring.
#
# Usage: mcp__rider__ue_execute_python with this file's contents.

import unreal

PACK = '/Game/Assets/RTG_AZ/MovementAnimsetPro/'
DBS = '/Game/AZ/Blueprints/Animation/MotionMatching/CMC/'
LAND_COMPLETE_TAG = 'Event.Movement.LandComplete'

# ---------------------------------------------------------------------------------------
# THE MECHANISM (see memory/feedback_posesearch_mm_mechanism_rules.md R4/R10):
#   BranchIn        = the INDEXED range -> also owns database membership.
#   BlockTransition = poses that may NOT be ENTERED (it can never hold a playing clip).
# Together they define an ENTRY WINDOW: BranchIn opens indexing at the moment the clip is
# meant to be entered, BlockTransition closes entry after it. Without the BlockTransition
# half, MM entered takeoffs ~1.5-2.0s into a 4.0s clip (deep in the fall, no push-off) and
# re-entered land clips mid-play, which showed up as landing clips flip-flopping LU<->RU.
#
# ForceRootLock MUST be True on every clip here. The ABP runs RootMotionFromMontagesOnly,
# so the capsule is driven by CMC and any root translation left in an MM-played clip
# displaces the MESH out of the capsule instead (measured +80cm / +86cm on the walk lands).
#
# ---------------------------------------------------------------------------------------
# THE LAND DATABASE SPLIT (2026-08-30). Stop-lands and continue-lands used to share one
# database per gait, so which one played was decided by a cost contest — and rule R2 says
# a clip that must play at a MOMENT can never win one. Measured: a *_Land (land-to-stop)
# clip selected while the capsule was still moving at 173, with rate=1.50 (the MM play-rate
# CEILING) as the search tried to make a decelerating clip keep up.
# Split into stop/move families, selected by SM state — a fact the state machine ALREADY
# owns (AZ_LocomotionStateMachine.cpp: touchdown -> TransitionToLocomotion if moving, else
# TransitionToIdle) and the ABP's DatabaseGates rows already key on. No cost tuning.
#
#   database                          tags                                  gate row (AZ_ABP_CmcAnimInstance)
#   PSD_AZ_Stand_Idle_Lands           JumpLands, JumpLandsStop              LandIdle (SM=TransitionToIdle)
#   PSD_AZ_Stand_Idle_Lands_Move      JumpLands, JumpLandsMove              (unreferenced — see NOTE)
#   PSD_AZ_Stand_Walk_Lands           JumpLands, JumpLandsStop              (unreferenced — see NOTE)
#   PSD_AZ_Stand_Walk_Lands_Move_LU   JumpLands, JumpLandsMove, JumpLandsLU LandWalk (SM=TransitionToLocomotion, Walk)
#   PSD_AZ_Stand_Walk_Lands_Move_RU   JumpLands, JumpLandsMove, JumpLandsRU LandWalk
#   PSD_AZ_Stand_Run_Lands            JumpLands, JumpLandsStop              (unreferenced — see NOTE)
#   PSD_AZ_Stand_Run_Lands_Move_LU    JumpLands, JumpLandsMove, JumpLandsLU LandRun  (SM=TransitionToLocomotion, Run/Sprint)
#   PSD_AZ_Stand_Run_Lands_Move_RU    JumpLands, JumpLandsMove, JumpLandsRU LandRun
#
# ---------------------------------------------------------------------------------------
# THE FOOT SPLIT (2026-08-30) — Mover parity. Which foot a land is authored for is DATA, not
# a cost question. CHT_v2_CharacterAnimations decides it on bLeftFootDown alone and pushes the
# clip with bUseMM=False, so on Mover the foot is never searched. Mapping read off that table
# and cross-checked against its 18 stop rows, all of which agree:
#
#       bLeftFootDown == True  -> _RU clip        bLeftFootDown == False -> _LU clip
#
# The live CMC graph has no direct-play path, so the equivalent guarantee is a pool of ONE:
# each foot owns a database, tagged JumpLandsLU / JumpLandsRU, and Get_DatabasesToSearch drops
# the database carrying the wrong foot's tag. Before this, the foot fell out of a cost contest
# between two near-identical impact poses — JumpWalkStart_LU landed on RU_Land2Walk (+2.67) and
# was taken over by LU_Land2Walk (+3.28) 0.28s later, so every touchdown played two lands.
#
# bLeftFootDown survives the flight by itself: Update_MovementDirection only writes it while a
# foot-contact curve is above threshold, and NONE of the 428 RTG_AZ clips carries a curve on a
# jump — so at touchdown it still holds the last grounded planted foot, and it cannot flip
# mid-land and strip the database holding the clip that is playing.
# ★ The curve NAMES live on the ABP CDO (contact_l / contact_r, threshold 0.5), NOT on the C++
# defaults, which still read FootSpeed_L / FootSpeed_R — names no clip in the project has. Read
# the CDO, never the C++ default, before concluding the foot signal is dead.
#
# ★ THE RU MIRROR SWAP (2026-08-30 evening). The AUTHORED RU moving-land clips are retired from
# every index: they measured entry cost +123 (walk) / +709 (run) against the LU clips' +4 and
# thrashed the search (the 0.24->0.10->0.07->0.03->0.00 re-entry loop that survived BOTH a -10
# continuing-pose bias and PoseJumpThresholdTime (-1,1) — the clips are simply bad pose matches
# for their own landing). The *_RU databases instead hold the LU clip as an EXPLICIT entry with
# MirrorOption=MirroredOnly (schema mirror table AZ_Hero_MDT):
#     PSD_AZ_Stand_Walk_Lands_Move_RU  = AnimPro_JumpWalk_LU_Land2Walk  (MirroredOnly, explicit)
#     PSD_AZ_Stand_Run_Lands_Move_RU   = AnimPro_JumpRun_LU_Land2Run    (MirroredOnly, explicit)
# apply_layout() CANNOT rebuild those two entries: AddSequenceToDatabase has no mirror parameter
# (the entry array is not script-reflected; the flag was applied via a temporary, since-reverted
# body edit). After a wipe, re-add them by hand in the PSD editor: add the LU clip, set
# Mirror Option = Mirrored Only. The RU source clips keep ForceRootLock + their LandComplete beat
# but carry NO PoseSearch notifies — they are in no database, and their BranchIn must NOT come
# back (it would re-add the bad clip alongside the mirror).
# Related engine knob discovered on the entry struct, not yet used: bDisableReselection ("poses
# from the same asset cannot be reselected") — the purpose-built guard for the snap class if it
# ever reappears on other one-shots.
#
# Every land DB keeps the JumpLands tag: the C++ entry/hold gate in AZ_CmcAnimInstance.cpp
# and KeepPlayingOneShotSearchable both key on it and are unchanged by the split.
#
# NOTE — the four *_Land (land-to-stop) clips and JumpIdleLand2Walk are currently reachable
# by no gate row. Routing them needs a discriminator FAZ_DatabaseGate cannot express today:
# "what gait was the jump launched from", as distinct from the commanded gait at touchdown.
# They are kept indexed and authored so that adding that input is a gate-row change only.
#
# ---------------------------------------------------------------------------------------
# TAKEOFF WINDOWS. BranchIn sits ON the measured launch frame; entry closes 0.175s later.
# Launch frames measured from the root track (first frame with vertical velocity > 30 cm/s),
# and the apex heights they produce match the content-derived constants in AZ::CmcJump.
#   clip                       launch   apex t / height
#   JumpIdleStart              0.150    0.467 / 100.1cm
#   JumpWalkStart_LU/RU        0.150    0.367 /  60.0cm
#   JumpRunStart_LU            0.217    0.433 /  70.1cm
#   JumpRunStart_RU            0.117    0.333 /  70.1cm
#
# LAND WINDOWS. Every land clip is authored IN PLACE (root Z flat across the whole clip),
# so touchdown is t=0 for all of them and the impact window is [0, 0.15].
#
# ★ LAND-COMPLETE BEAT RULE. Each land clip carries an AZ_AnimNotify_SendGameplayEvent
# firing Event.Movement.LandComplete, which is what ends UAZ_GA_PawnJump (events drive,
# the 2.5s watchdog only guards). The beat MUST sit inside the exclusive hold window —
# LandExclusiveHoldFraction (0.60) x length — because that is the only interval where the
# clip is guaranteed to still be the NEWEST blend-stack player: a blending-out player is
# updated with an inactive context (AnimNode_BlendStack.cpp:893-898) and fires nothing.
# Past the release the pool reopens and locomotion can take the clip over, eating the event
# and leaving the ability to time out on the watchdog — which blocks the next jump for 2.5s
# (Movement.Jumping is in the ability's own ActivationBlockedTags). 50% of length satisfies
# the rule for every clip below; assert_beats() re-checks it before writing anything.
# ---------------------------------------------------------------------------------------

# ★ COMMITTING THE ONE-SHOT. Forcing a lands-only pool makes the land the ONLY legal answer
# but does not make it a CHEAP one: measured entry cost +2.67 / +3.28 for the Land2Walk clips
# against +0.01 for AnimPro_WalkFwdLoop, because the clips are authored at 222 cm/s while the
# capsule walks at 173 and the takeoff leaves the body in a falling pose. With every candidate
# in the pool expensive, the search never settled — it re-entered the playing clip's OWN entry
# window ("[CmcSnap] AnimPro_JumpWalk_RU_Land2Walk 0.23 -> 0.10") and then jumped to the other
# foot, so one touchdown played three stabs. Rule R1: the continuing-pose exemption keeps a clip
# SEARCHABLE, not SELECTED — pool narrowing alone cannot hold a one-shot.
# OverrideContinuingPoseCostBias is the lever that can (the same one the turn-in-place and
# reface-start clips use). Applied over the exclusive hold window, so the bias expires exactly
# when the pool reopens and the hand-back to locomotion is unaffected. The magnitude only has
# to exceed the competitor costs above — it is a commit, not a tuning knob.
LAND_CONTINUING_BIAS = -10.0

LAND_HOLD_FRACTION = 0.60  # keep in sync with UAZ_CmcAnimInstance::LandExclusiveHoldFraction

# name: (database | None, branch_in_start | None, block_start | None, land_complete | None)
#   branch_in None = not a BranchIn member (explicit database entry instead)
#   block     None = no entry restriction
# All ranges run to the end of the clip; all clips get ForceRootLock = True.
LAYOUT = {
    # -- takeoffs --------------------------------------------------------------------
    'AnimPro_JumpIdleStart':         ('PSD_AZ_Stand_Idle_Jump', 0.175, 0.325, None),
    'AnimPro_JumpWalkStart_LU':      ('PSD_AZ_Stand_WalkAndRun_Jump', 0.150, 0.325, None),
    'AnimPro_JumpWalkStart_RU':      ('PSD_AZ_Stand_WalkAndRun_Jump', 0.150, 0.325, None),
    'AnimPro_JumpRunStart_LU':       ('PSD_AZ_Stand_WalkAndRun_Jump', 0.217, 0.392, None),
    'AnimPro_JumpRunStart_RU':       ('PSD_AZ_Stand_WalkAndRun_Jump', 0.117, 0.292, None),
    # -- air -------------------------------------------------------------------------
    'AnimPro_FallingLoop':           ('PSD_AZ_Stand_InAir', 0.000, None, None),
    # -- lands: STOP family (land and come to rest) ----------------------------------
    'AnimPro_JumpIdleLand':          (None, None, 0.150, 0.517),
    'AnimPro_JumpWalk_LU_Land':      ('PSD_AZ_Stand_Walk_Lands', 0.000, 0.150, 0.633),
    'AnimPro_JumpWalk_RU_Land':      ('PSD_AZ_Stand_Walk_Lands', 0.000, 0.150, 0.700),
    'AnimPro_JumpRun_LU_Land':       ('PSD_AZ_Stand_Run_Lands', 0.000, 0.150, 0.633),
    'AnimPro_JumpRun_RU_Land':       ('PSD_AZ_Stand_Run_Lands', 0.000, 0.150, 0.700),
    # -- lands: MOVE family (land and carry on moving) -------------------------------
    'AnimPro_JumpIdleLand2Walk':     ('PSD_AZ_Stand_Idle_Lands_Move', 0.000, 0.150, 0.650),
    'AnimPro_JumpWalk_LU_Land2Walk': ('PSD_AZ_Stand_Walk_Lands_Move_LU', 0.000, 0.150, 0.456),
    'AnimPro_JumpRun_LU_Land2Run':   ('PSD_AZ_Stand_Run_Lands_Move_LU', 0.000, 0.150, 0.450),
    # RETIRED from indexing (see THE RU MIRROR SWAP above): no database, no PoseSearch notifies.
    # The *_RU databases play the LU clips MIRRORED instead — a manual PSD-editor entry.
    'AnimPro_JumpWalk_RU_Land2Walk': (None, None, None, 0.283),
    'AnimPro_JumpRun_RU_Land2Run':   (None, None, None, 0.217),
}

# AnimPro_JumpIdleLandHard is deliberately OUT of every database (no BranchIn, no explicit
# entry): a severity variant with no gate wins the common case — it played on every ordinary
# jump. Re-add it only once an impact-speed rule exists to select it. See rule R8.
# It still needs ForceRootLock, so it is listed here.
ROOT_LOCK_ONLY = ['AnimPro_JumpIdleLandHard']


def _land_complete_tag():
    tag = unreal.GameplayTag()
    tag.import_text('(TagName="%s")' % LAND_COMPLETE_TAG)
    if not unreal.GameplayTagLibrary.is_gameplay_tag_valid(tag):
        raise RuntimeError('%s is not a registered tag - check AZ_GameplayTags.cpp' % LAND_COMPLETE_TAG)
    return tag


def assert_beats():
    """Fail BEFORE writing anything if a land-complete beat sits outside the hold window."""
    bad = []
    for name, (_db, _bi, _bl, beat) in LAYOUT.items():
        if beat is None:
            continue
        seq = unreal.EditorAssetLibrary.load_asset(PACK + name)
        if not seq:
            continue
        limit = LAND_HOLD_FRACTION * seq.get_play_length()
        if beat >= limit:
            bad.append('%s beat=%.3f must be < %.3f' % (name, beat, limit))
    if bad:
        raise RuntimeError('land-complete beat rule violated:\n  ' + '\n  '.join(bad))


def apply_layout():
    assert_beats()
    tag = _land_complete_tag()

    for name in ROOT_LOCK_ONLY:
        seq = unreal.EditorAssetLibrary.load_asset(PACK + name)
        if seq:
            seq.set_editor_property('force_root_lock', True)
            unreal.EditorAssetLibrary.save_asset(PACK + name, only_if_is_dirty=False)
            print('%-32s root-lock only' % name)

    for name, (dbname, branch_in, block, beat) in LAYOUT.items():
        path = PACK + name
        seq = unreal.EditorAssetLibrary.load_asset(path)
        if not seq:
            print('MISSING: ' + path)
            continue
        length = seq.get_play_length()

        seq.set_editor_property('force_root_lock', True)
        unreal.AZ_PoseSearchUtils.remove_all_pose_search_notifies(seq)

        if branch_in is not None:
            db = unreal.EditorAssetLibrary.load_asset(DBS + dbname)
            if not db:
                print('MISSING DB: ' + DBS + dbname)
                continue
            # AddBranchInNotify(Sequence, Database, StartTime, Duration) - the Database is
            # REQUIRED: a null one is rejected by the engine and MM then ignores the notify.
            unreal.AZ_PoseSearchUtils.add_branch_in_notify(seq, db, branch_in, 0.0)
        if block is not None:
            # AddBlockTransitionNotify rejects Duration <= 0, so "to the end" must be explicit.
            unreal.AZ_PoseSearchUtils.add_block_transition_notify(seq, block, length - block)

        if beat is not None:
            # A land clip: commit it for the hold window so nothing re-enters it or its sibling.
            # Skip the PoseSearch bias on retired clips (dbname None) — they are in no index and
            # must stay free of PoseSearch notifies so BranchIn sync can never resurrect them.
            if dbname is not None:
                unreal.AZ_PoseSearchUtils.add_override_continuing_pose_cost_bias_notify(
                    seq, 0.0, LAND_HOLD_FRACTION * length, LAND_CONTINUING_BIAS)
            for e in unreal.AnimationLibrary.get_animation_notify_events(seq):
                n = e.notify
                if n and isinstance(n, unreal.AZ_AnimNotify_SendGameplayEvent):
                    unreal.AnimationLibrary.remove_animation_notify_events_by_name(seq, e.notify_name)
            n = unreal.AnimationLibrary.add_animation_notify_event(
                seq, '1', beat, unreal.AZ_AnimNotify_SendGameplayEvent)
            n.set_editor_property('event_tag', tag)

        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        print('%-32s len=%.3f  branchIn=%s block=%s beat=%s  db=%s' % (
            name, length, branch_in, block, beat, dbname))


apply_layout()
