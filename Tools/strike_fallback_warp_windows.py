# @Description: warp windows on the trimmed strike montages so the UNPAIRED fallback (target present, pair did not fit)
# still aligns like the old warped heavy; with no target they are inert. Same shape as the jab/heavy windows.
import unreal
eal = unreal.EditorAssetLibrary; MU = unreal.AZ_MontageUtils
for path, contact in [('/Game/AZ/Blueprints/Animation/Montage/AM_Fists_Punch_Heavy_Strike', 0.50), ('/Game/AZ/Blueprints/Animation/Montage/AM_Fists_Kick_Strike', 0.20)]:
    m = unreal.load_asset(path); L = unreal.AnimationLibrary.get_sequence_length(m)
    MU.remove_motion_warping_notifies(m)
    # translation-only window to MeleeTarget, rotation-only twin to MeleeTarget_Facing starting +5ms (engine de-dups identical windows)
    assert MU.add_motion_warping_notify(m, unreal.Name('None'), 0.001 / L, contact / L, 'MeleeTarget', True, False, 1, 1, 180.0, 1.0, 2, 2.0)
    assert MU.add_motion_warping_notify(m, unreal.Name('None'), 0.006 / L, contact / L, 'MeleeTarget_Facing', False, True, 1, 1, 180.0, 1.0, 2, 0.0)
    print(path.split('/')[-1], 'saved', eal.save_loaded_asset(m, only_if_is_dirty=False))
    for s in MU.dump_montage_notifies(m): print('  ', s)
