# @Description: PSIA heavy KICK variant on R (random with the punch). Run AFTER the StrikeVariants build, editor open.
# Measured 2026-09-03: RTG_RM_Fists_Kick_Front_Move_R 1.5s, 165cm walk-off; ball_r peaks 105.6cm reach @0.45 (z 115,
# x -4), root advanced 89.5 by then. Contact 0.45; montage [0, 0.85]; hit window 0.38-0.52 on foot_r/ball_r.
import unreal
eal = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary
MU = unreal.AZ_MontageUtils
PU = unreal.AZ_PoseSearchUtils
NONE = unreal.Name('None')
tools = unreal.AssetToolsHelpers.get_asset_tools()
PSI = '/Game/AZ/Blueprints/Animation/PSI'
STRIKE = '/Game/AZ/Blueprints/Animation/Strike'
# Montage starts 0.20s INTO the clip (a third of the step-in): contact 0.45 -> 0.25 on the montage timeline,
# root advance segment-start->contact = 89.5 - 33.3. Full wind-up put the pair at 232cm (fires from ~180cm) —
# the punch pairs from ~125, so R was always the punch at fighting range (PIE 03:01: kick tried twice, rejected).
# 2nd trim (PIE 03:04): at 0.20 the kick still needed 67-99cm of close-in at 96-132cm (the punch fit) and its ONE
# pair (221cm) whiffed — root-root 167 at contact, hero travelled 26 of 56: the 0.25s blend-in scales root motion,
# and contact was 0.25. Start at 0.25 (contact 0.20, root adv 89.5-46.4) with a 0.10 blend-in -> window ~127-247.
KICK_START = 0.25
CONTACT, REACH, ROOT_ADV = 0.20, 105.6, 89.5 - 46.4
KICK_BLEND_IN = 0.10
CHEST, SINK, WALK_SPEED, WALK_END = 13.0, 8.0, 35.0, 3.067

# ---- A. hero half: kick [0, 0.85]
kick_seq = unreal.load_asset('/Game/Assets/RM_Movement/RTG_RM_Fists_Kick_Front_Move_R')
ref = unreal.load_asset('/Game/AZ/Blueprints/Animation/Montage/AM_Fists_Punch_Heavy_Strike')
bi = ref.get_editor_property('blend_in').get_editor_property('blend_time'); bo = ref.get_editor_property('blend_out').get_editor_property('blend_time')
HERO_M = '/Game/AZ/Blueprints/Animation/Montage/AM_Fists_Kick_Strike'
hm = MU.build_sectioned_montage_ranged(HERO_M, 'FullBody', ['Default'], [kick_seq], [NONE], [], [KICK_START], [0.85], KICK_BLEND_IN, bo)
assert hm, 'kick montage build failed'
MU.remove_motion_warping_notifies(hm); MU.remove_melee_window_notify_states(hm)
assert MU.add_melee_window_notify_state(hm, 0.38 - KICK_START, 0.14)
assert MU.add_melee_window_notify_state(hm, 0.60 - KICK_START, 0.25, '1', True, 'Event.Combat.CancelOpen', 'Event.Combat.CancelClose')
assert eal.save_loaded_asset(hm, only_if_is_dirty=False)
print('HERO', HERO_M, 'len=%.3f' % AL.get_sequence_length(hm), list(MU.dump_montage_sections(hm)), list(MU.dump_montage_notifies(hm)))

# ---- B. victim half: [WalkIn 0.45 | React = knockback to root rest]
walk = unreal.load_asset(STRIKE + '/AS_Zombie_Walk_F_5_Loop_RM'); kb = unreal.load_asset(STRIKE + '/AS_Zombie_Walk_F_5_KnockBack_RM')
VICTIM_M = STRIKE + '/AM_Zombie_Strike_Kick_F_5'
vm = MU.build_sectioned_montage_ranged(VICTIM_M, 'DefaultSlot', ['WalkIn', 'React'], [walk, kb], ['React', NONE],
                                       [], [WALK_END - CONTACT, 0.0], [WALK_END, 2.767], 0.25, 0.4)
assert vm, 'victim montage build failed'
MU.remove_gameplay_event_notifies(vm, 'Event.Combat.BeatEnd')
assert MU.add_gameplay_event_notify(vm, 'Event.Combat.BeatEnd', CONTACT + 2.70)
assert eal.save_loaded_asset(vm, only_if_is_dirty=False)
print('VICTIM', VICTIM_M, 'len=%.3f' % AL.get_sequence_length(vm), list(MU.dump_montage_sections(vm)), list(MU.dump_montage_notifies(vm)))

# ---- C. PSIA: victim origin = reach + sink + chest + walk-in travel + hero root advance at contact
HERO_TRAVEL_LOSS = 12.0  # measured 03:08/03:13 (origin 176.7): hero moved 30-37 of the authored 43 by contact (blend-in + first drive
                         # tick), root-root 143-153 vs the modelled 137 -> 3/6 kicks landed. 12 puts contact at ~131-141, inside the sweep.
y0 = REACH + SINK + CHEST + WALK_SPEED * CONTACT + ROOT_ADV - HERO_TRAVEL_LOSS
ppath = PSI + '/AZ_Strike_Kick_F5'
psia = unreal.load_asset(ppath) if eal.does_asset_exist(ppath) else tools.create_asset('AZ_Strike_Kick_F5', PSI, unreal.PoseSearchInteractionAsset, unreal.PoseSearchInteractionAssetFactory())
assert psia
a = unreal.PoseSearchInteractionAssetItem(); a.set_editor_property('role', 'Attacker'); a.set_editor_property('animation', hm)
a.set_editor_property('warping_weight_translation', 1.0); a.set_editor_property('warping_weight_rotation', 1.0); a.set_editor_property('origin', unreal.Transform())
v = unreal.PoseSearchInteractionAssetItem(); v.set_editor_property('role', 'Victim'); v.set_editor_property('animation', vm)
v.set_editor_property('warping_weight_translation', 0.0); v.set_editor_property('warping_weight_rotation', 0.0)
v.set_editor_property('origin', unreal.Transform(location=unreal.Vector(0.0, y0, 0.0), rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0), scale=unreal.Vector(1, 1, 1)))
psia.set_editor_property('items', [a, v])
assert eal.save_loaded_asset(psia, only_if_is_dirty=False)
print('PSIA', ppath, 'victim origin Y=%.1f' % y0)

# ---- D. its own database
schema = unreal.load_asset(PSI + '/PSS_AZ_Strike')
dpath = PSI + '/PSD_AZ_Strike_Kick'
db = unreal.load_asset(dpath) if eal.does_asset_exist(dpath) else tools.create_asset('PSD_AZ_Strike_Kick', PSI, unreal.PoseSearchDatabase, None)
assert db
db.set_editor_property('schema', schema); db.set_editor_property('pose_search_mode', unreal.PoseSearchMode.BRUTE_FORCE)
PU.clear_database(db); assert PU.add_anim_asset_to_database(db, psia); assert PU.set_sampling_range_on_entry(db, 0, 0.0, 0.15)
assert eal.save_loaded_asset(db, only_if_is_dirty=False)

# ---- E. BP_GA_HeavyStrike: two variants, punch 0.2 / kick 0.8
bp = unreal.load_asset('/Game/AZ/Blueprints/AbilitySystem/Hero/Abilities/BP_GA_HeavyStrike')
cdo = unreal.get_default_object(bp.generated_class())
# Weights are the USER's tuning (edited in the BP editor, 2026-09-03: punch 0.6 / kick 0.4) — keep whatever is there.
existing = {x.get_editor_property('montage').get_name(): x.get_editor_property('weight') for x in cdo.get_editor_property('strike_variants') if x.get_editor_property('montage')}
punch = unreal.AZ_StrikeVariant()
punch.set_editor_property('database', unreal.load_asset(PSI + '/PSD_AZ_Strike'))
punch.set_editor_property('montage', unreal.load_asset('/Game/AZ/Blueprints/Animation/Montage/AM_Fists_Punch_Heavy_Strike'))
punch.set_editor_property('strike_sockets', []); punch.set_editor_property('weight', existing.get('AM_Fists_Punch_Heavy_Strike', 0.5))
kick = unreal.AZ_StrikeVariant()
kick.set_editor_property('database', db); kick.set_editor_property('montage', hm)
kick.set_editor_property('strike_sockets', [unreal.Name('ball_r'), unreal.Name('foot_r')]); kick.set_editor_property('weight', existing.get('AM_Fists_Kick_Strike', 0.5))
cdo.set_editor_property('strike_variants', [punch, kick])
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert eal.save_loaded_asset(bp, only_if_is_dirty=False)
cdo = unreal.get_default_object(bp.generated_class())
print('BP_GA_HeavyStrike variants:', [(x.get_editor_property('database').get_name(), x.get_editor_property('montage').get_name(), [str(n) for n in x.get_editor_property('strike_sockets')], x.get_editor_property('weight')) for x in cdo.get_editor_property('strike_variants')])

# ---- F. index build (ACCESS) — close the tab before PIE
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([db])
print('DONE - watch for "PSD_AZ_Strike_Kick BuildIndex Succeeded"')
