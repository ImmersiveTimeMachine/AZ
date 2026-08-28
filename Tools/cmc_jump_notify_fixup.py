# CMC jump — PoseSearch notify layout (RECOVERY SCRIPT, do not run casually)
#
# WHY THIS FILE EXISTS
# The jump source clips live under /Content/Assets/RTG_AZ/ which .gitignore excludes
# (LFS budget, rule at .gitignore:65). Their notify layout is load-bearing for jump
# selection but is NOT in version control. This script re-applies it after a fresh
# clone / asset-pack reimport.
#
# The engine exposes no read path for notify windows (FAnimNotifyEvent's Duration and
# TriggerTime are protected, and there are no Python getters), so this file is the only
# written record of the values. Keep it in sync by hand when a window changes.
#
# ★ DO NOT RUN against a working project to "verify" — it REMOVES all PoseSearch notifies
# first and rewrites them. Run it only when the clips have lost their notifies.
#
# Usage: mcp__rider__ue_execute_python with this file's contents.

import unreal

PACK = '/Game/Assets/RTG_AZ/MovementAnimsetPro/'

# THE MECHANISM (see memory/feedback_posesearch_mm_mechanism_rules.md):
#   BranchIn        = the INDEXED range -> also owns database membership (PreSaveRoot sync).
#   BlockTransition = poses that may NOT be ENTERED (it can never hold a playing clip).
# Together they define an ENTRY WINDOW: BranchIn opens indexing, BlockTransition closes
# entry after the window, so MM can only enter the clip where it is meant to.
#
# branch_in: (start, duration)  duration 0 = to clip end. None = leave clip out of BranchIn
#            indexing (clip is an explicit database member instead).
# block:     (start, duration)  None = no entry restriction.
LAYOUT = {
    # Takeoff: enterable ONLY in the launch window [0.175, 0.325].
    # 0.175 = the measured launch frame; blocking the remaining 3.208s stops MM entering
    # the rise/apex/fall portion, which used to read as the falling loop.
    'AnimPro_JumpIdleStart': {'branch_in': (0.175, 0.0), 'block': (0.325, 3.208)},

    # Land: explicit database member (no BranchIn), enterable ONLY at impact [0, 0.15].
    # Without the block, MM entered the land ~65% in (measured t=0.67/1.03) and the
    # impact was never seen. Verified 2026-08-28.
    'AnimPro_JumpIdleLand':  {'branch_in': None,         'block': (0.150, 0.883)},
}

# NOT YET RECORDED — the walk/run takeoffs and lands and the falling loop were given
# BranchIn notifies earlier in the spike but their exact windows were never read back
# (no engine read path) and are NOT reproduced here. Re-measure before adding them:
# writing a guessed window here would silently corrupt a working layout.
UNRECORDED = [
    'AnimPro_JumpWalkStart', 'AnimPro_JumpRunStart_LU', 'AnimPro_JumpRunStart_RU',
    'AnimPro_FallingLoop', 'AnimPro_JumpWalkLand', 'AnimPro_JumpRunLand',
]


def apply_layout():
    for name, spec in LAYOUT.items():
        path = PACK + name
        seq = unreal.EditorAssetLibrary.load_asset(path)
        if not seq:
            print('MISSING: ' + path)
            continue

        unreal.AZ_PoseSearchUtils.remove_all_pose_search_notifies(seq)

        if spec['branch_in']:
            start, dur = spec['branch_in']
            unreal.AZ_PoseSearchUtils.add_branch_in_notify(seq, start, dur)
        if spec['block']:
            start, dur = spec['block']
            unreal.AZ_PoseSearchUtils.add_block_transition_notify(seq, start, dur)

        count = len(unreal.AnimationLibrary.get_animation_notify_events(seq))
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        print('%s: %d notify state(s)' % (name, count))

    print('Unrecorded (windows must be re-measured): ' + ', '.join(UNRECORDED))


apply_layout()
