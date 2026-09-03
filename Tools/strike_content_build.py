# @Description: PSIA heavy strike — content build (run AFTER the closed CLI build, editor reopened).
# Builds: hero strike montage, victim [WalkIn|React] montage, PSIA AZ_Strike_Heavy_F5, PSD_AZ_Strike,
# BP_GA_HeavyStrike (child of AZ_GA_StrikeInteraction), fist-slot grant, InputConfig row.
import unreal
eal = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary
MU = unreal.AZ_MontageUtils
PU = unreal.AZ_PoseSearchUtils
NONE = unreal.Name('None')

def tag(name):
    t = unreal.GameplayTag(); t.import_text(name); return t   # tag_name is read-only from Python; ImportText accepts a bare tag name

# ---------------- A. hero half: Heavy2Idle [0, 0.9] (right hook lands at 0.50) ----------------
heavy_seq = unreal.load_asset('/Game/Assets/RM_Movement/RTG_RM_Fists_Punch_Heavy2Idle')
ref = unreal.load_asset('/Game/AZ/Blueprints/Animation/Montage/AM_Fists_Punch_Heavy_L')
bi = ref.get_editor_property('blend_in').get_editor_property('blend_time')
bo = ref.get_editor_property('blend_out').get_editor_property('blend_time')
HERO_M = '/Game/AZ/Blueprints/Animation/Montage/AM_Fists_Punch_Heavy_Strike'
hm = MU.build_sectioned_montage_ranged(HERO_M, 'FullBody', ['Default'], [heavy_seq], [NONE], [], [0.0], [0.9], bi, bo)
assert hm, 'hero montage build failed'
MU.remove_motion_warping_notifies(hm)          # the pair owns the geometry — no warp windows on this clip
MU.remove_melee_window_notify_states(hm)
assert MU.add_melee_window_notify_state(hm, 0.44, 0.12)                     # the hook only; the 0.34 jab is a feint
assert MU.add_melee_window_notify_state(hm, 0.60, 0.30, '1', True, 'Event.Combat.CancelOpen', 'Event.Combat.CancelClose')
print('HERO', HERO_M, 'len=%.3f blend=%.2f/%.2f' % (AL.get_sequence_length(hm), bi, bo))
for s in MU.dump_montage_sections(hm): print('  SEC', s)
for s in MU.dump_montage_notifies(hm): print('  NOT', s)
assert eal.save_loaded_asset(hm)

# ---------------- B. victim half: [WalkIn 0.5s of the walk loop | React = knockback to root rest] ----------------
walk = unreal.load_asset('/Game/AZ/Blueprints/Animation/Strike/AS_Zombie_Walk_F_5_Loop_RM')
kb = unreal.load_asset('/Game/AZ/Blueprints/Animation/Strike/AS_Zombie_Walk_F_5_KnockBack_RM')
VICTIM_M = '/Game/AZ/Blueprints/Animation/Strike/AM_Zombie_Strike_Walk_F_5'
vm = MU.build_sectioned_montage_ranged(VICTIM_M, 'DefaultSlot', ['WalkIn', 'React'], [walk, kb], ['React', NONE],
                                       [], [2.567, 0.0], [3.067, 2.767], 0.25, 0.4)
assert vm, 'victim montage build failed'
MU.remove_gameplay_event_notifies(vm, 'Event.Combat.BeatEnd')
assert MU.add_gameplay_event_notify(vm, 'Event.Combat.BeatEnd', 3.20)
print('VICTIM', VICTIM_M, 'len=%.3f' % AL.get_sequence_length(vm))
for s in MU.dump_montage_sections(vm): print('  SEC', s)
for s in MU.dump_montage_notifies(vm): print('  NOT', s)
assert eal.save_loaded_asset(vm)

# ---------------- C. PSIA: hero anchors (1/1), victim placed 183.6cm ahead facing him ----------------
tools = unreal.AssetToolsHelpers.get_asset_tools()
PSI = '/Game/AZ/Blueprints/Animation/PSI'
psia_path = PSI + '/AZ_Strike_Heavy_F5'
psia = unreal.load_asset(psia_path) if eal.does_asset_exist(psia_path) else tools.create_asset('AZ_Strike_Heavy_F5', PSI, unreal.PoseSearchInteractionAsset, unreal.PoseSearchInteractionAssetFactory())
assert psia, 'PSIA create failed'
a = unreal.PoseSearchInteractionAssetItem()
a.set_editor_property('role', 'Attacker'); a.set_editor_property('animation', hm)
a.set_editor_property('warping_weight_translation', 1.0); a.set_editor_property('warping_weight_rotation', 1.0)
a.set_editor_property('origin', unreal.Transform())
v = unreal.PoseSearchInteractionAssetItem()
v.set_editor_property('role', 'Victim'); v.set_editor_property('animation', vm)
v.set_editor_property('warping_weight_translation', 0.0); v.set_editor_property('warping_weight_rotation', 0.0)
v.set_editor_property('origin', unreal.Transform(location=unreal.Vector(0.0, 183.6, 0.0), rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0), scale=unreal.Vector(1, 1, 1)))
psia.set_editor_property('items', [a, v])
assert eal.save_loaded_asset(psia)
print('PSIA', psia_path, psia.get_editor_property('items'))

# ---------------- D. database over the strike schema, entry window = the wind-up ----------------
schema = unreal.load_asset(PSI + '/PSS_AZ_Strike')
db_path = PSI + '/PSD_AZ_Strike'
db = unreal.load_asset(db_path) if eal.does_asset_exist(db_path) else tools.create_asset('PSD_AZ_Strike', PSI, unreal.PoseSearchDatabase, None)   # the factory returns None headless (schema picker); None works (memory recipe)
assert db, 'DB create failed'
db.set_editor_property('schema', schema)
db.set_editor_property('pose_search_mode', unreal.PoseSearchMode.BRUTE_FORCE)
PU.clear_database(db)
assert PU.add_anim_asset_to_database(db, psia)
assert PU.set_sampling_range_on_entry(db, 0, 0.0, 0.15)
assert eal.save_loaded_asset(db)
print('DB', db_path, 'schema=', db.get_editor_property('schema').get_name())

# ---------------- E. BP_GA_HeavyStrike (child of AZ_GA_StrikeInteraction) ----------------
GA_DIR = '/Game/AZ/Blueprints/AbilitySystem/Hero/Abilities'
bp_path = GA_DIR + '/BP_GA_HeavyStrike'
if eal.does_asset_exist(bp_path):
    bp = unreal.load_asset(bp_path)
else:
    f = unreal.BlueprintFactory(); f.set_editor_property('parent_class', unreal.AZ_GA_StrikeInteraction)
    bp = tools.create_asset('BP_GA_HeavyStrike', GA_DIR, unreal.Blueprint, f)
assert bp, 'BP create failed'
cls = bp.generated_class(); cdo = unreal.get_default_object(cls)
heavy_warped = unreal.load_asset('/Game/AZ/Blueprints/Animation/Montage/AM_Fists_Punch_Heavy_L')
ge_ready = unreal.load_object(None, '/Game/AZ/Blueprints/AbilitySystem/Hero/Effects/GE_CombatReady.GE_CombatReady_C')
props = {
    'input_tag': tag('Input.Action.MeleeAttack'), 'hand': unreal.AZ_MeleeHand.LEFT,
    'punch_idle_l': heavy_warped, 'punch_move_l': heavy_warped, 'punch_lunge_l': heavy_warped,
    'punch_idle_r': heavy_warped, 'punch_move_r': heavy_warped, 'punch_lunge_r': heavy_warped,
    'lunge_min_distance': 0.0, 'root_motion_seconds': 1.7, 'warp_approach_distance': 120.0,
    'damage_amount': 20.0, 'effects_on_activate': [ge_ready],
    'strike_database': db, 'strike_montage': hm,
}
for k, val in props.items():
    cdo.set_editor_property(k, val)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert eal.save_loaded_asset(bp)
cdo = unreal.get_default_object(bp.generated_class())
print('BP_GA_HeavyStrike: tag=', cdo.get_editor_property('input_tag').get_editor_property('tag_name'),
      'db=', cdo.get_editor_property('strike_database').get_name(), 'montage=', cdo.get_editor_property('strike_montage').get_name(),
      'lunge=', cdo.get_editor_property('punch_lunge_l').get_name(), 'dmg=', cdo.get_editor_property('damage_amount'))

# ---------------- F. grant with the fists (PC QuickBar slot 0) ----------------
pc_bp = unreal.load_asset('/Game/AZ/Blueprints/Player/BP_AZ_PlayerController')
pc_cdo = unreal.get_default_object(pc_bp.generated_class())
qb = pc_cdo.get_component_by_class(unreal.AZ_QuickBarComponent)
slots = list(qb.get_editor_property('slots'))
s0 = slots[0]
abil = list(s0.get_editor_property('weapon_abilities'))
if bp.generated_class() not in abil:
    abil.append(bp.generated_class()); s0.set_editor_property('weapon_abilities', abil); slots[0] = s0
    qb.set_editor_property('slots', slots)
    unreal.BlueprintEditorLibrary.compile_blueprint(pc_bp)
    assert eal.save_loaded_asset(pc_bp)
qb = unreal.get_default_object(pc_bp.generated_class()).get_component_by_class(unreal.AZ_QuickBarComponent)
print('QuickBar slot0 abilities:', [c.get_name() for c in qb.get_editor_property('slots')[0].get_editor_property('weapon_abilities')])

# ---------------- G. InputConfig row: AZ_IA_RT_HeavyStrike -> Input.Action.MeleeAttack ----------------
ia = unreal.load_asset('/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IA_RT_HeavyStrike')
cfg = unreal.load_asset('/Game/AZ/Blueprints/Input/AZ_InputConfig')
rows = list(cfg.get_editor_property('ability_input_actions'))
if not [r for r in rows if r.get_editor_property('input_action') == ia]:
    row = unreal.AZ_InputAction(); row.set_editor_property('input_action', ia); row.set_editor_property('input_tag', tag('Input.Action.MeleeAttack'))
    rows.append(row); cfg.set_editor_property('ability_input_actions', rows)
    assert eal.save_loaded_asset(cfg)
print('InputConfig rows:', [(r.get_editor_property('input_action').get_name() if r.get_editor_property('input_action') else None, str(r.get_editor_property('input_tag').get_editor_property('tag_name'))) for r in cfg.get_editor_property('ability_input_actions')])

# ---------------- H. kick the index build (needs an ACCESS — open the DB editor) ----------------
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_asset(db)
print('DONE — watch the log for "BuildIndex Succeeded" on PSD_AZ_Strike, then close the DB tab before PIE')
