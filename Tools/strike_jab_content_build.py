# @Description: PSIA jabs (L/R) on the same rail as the R heavy — content build + BP reparent. Editor open, no C++.
# Measured 2026-09-03: Punch_L knuckle peaks 83.8cm @0.17s (x 0.3), Punch_R 78.2cm @0.19s (x 7.7); KB F_5 chest 13cm
# in front of root; 8cm knuckle->spine sink; walk-in = walk loop cut ending at frame 92 (3.067s), 35cm/s.
import unreal
eal = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary
MU = unreal.AZ_MontageUtils
PU = unreal.AZ_PoseSearchUtils
NONE = unreal.Name('None')
tools = unreal.AssetToolsHelpers.get_asset_tools()
PSI = '/Game/AZ/Blueprints/Animation/PSI'
STRIKE = '/Game/AZ/Blueprints/Animation/Strike'
GA_DIR = '/Game/AZ/Blueprints/AbilitySystem/Hero/Abilities'

def tag(name):
    t = unreal.GameplayTag(); t.import_text(name); return t

walk = unreal.load_asset(STRIKE + '/AS_Zombie_Walk_F_5_Loop_RM')
kb = unreal.load_asset(STRIKE + '/AS_Zombie_Walk_F_5_KnockBack_RM')
schema = unreal.load_asset(PSI + '/PSS_AZ_Strike')
WALK_END = 3.067          # walk loop frame 92 = best pose match to KB frame 0
REACT_LEN = 0.60          # jab recoil: KB root reaches -37cm by 0.6s (a jab, not the heavy's 2.6m)
CHEST, SINK, WALK_SPEED = 13.0, 8.0, 35.0

JABS = [
    # hand, ability BP, jab montage, contact time, knuckle reach at contact
    ('L', 'BP_GA_Punch_L', '/Game/AZ/Blueprints/Animation/Montage/AM_Fists_Punch_L', 0.17, 83.8),
    ('R', 'BP_GA_Punch_R', '/Game/AZ/Blueprints/Animation/Montage/AM_Fists_Punch_R', 0.19, 78.2),
]
built = {}
for hand, bp_name, jab_path, contact, reach in JABS:
    jab = unreal.load_asset(jab_path)
    # ---- victim half: [WalkIn contact-seconds | React = first 0.6s of the knockback]
    vpath = STRIKE + '/AM_Zombie_Strike_Jab_%s_F_5' % hand
    vm = MU.build_sectioned_montage_ranged(vpath, 'DefaultSlot', ['WalkIn', 'React'], [walk, kb], ['React', NONE],
                                           [], [WALK_END - contact, 0.0], [WALK_END, REACT_LEN], 0.15, 0.4)
    assert vm, 'victim montage build failed ' + hand
    MU.remove_gameplay_event_notifies(vm, 'Event.Combat.BeatEnd')
    assert MU.add_gameplay_event_notify(vm, 'Event.Combat.BeatEnd', contact + REACT_LEN - 0.05)
    assert eal.save_loaded_asset(vm, only_if_is_dirty=False)
    print('VICTIM', vpath, 'len=%.3f' % AL.get_sequence_length(vm), list(MU.dump_montage_sections(vm)), list(MU.dump_montage_notifies(vm)))
    # ---- PSIA: hero anchors; victim placed so the knuckle sits SINK cm short of the spine at contact
    y0 = reach + SINK + CHEST + WALK_SPEED * contact
    ppath = PSI + '/AZ_Strike_Jab_%s_F5' % hand
    psia = unreal.load_asset(ppath) if eal.does_asset_exist(ppath) else tools.create_asset('AZ_Strike_Jab_%s_F5' % hand, PSI, unreal.PoseSearchInteractionAsset, unreal.PoseSearchInteractionAssetFactory())
    assert psia, 'PSIA create failed ' + hand
    a = unreal.PoseSearchInteractionAssetItem()
    a.set_editor_property('role', 'Attacker'); a.set_editor_property('animation', jab)
    a.set_editor_property('warping_weight_translation', 1.0); a.set_editor_property('warping_weight_rotation', 1.0)
    a.set_editor_property('origin', unreal.Transform())
    v = unreal.PoseSearchInteractionAssetItem()
    v.set_editor_property('role', 'Victim'); v.set_editor_property('animation', vm)
    v.set_editor_property('warping_weight_translation', 0.0); v.set_editor_property('warping_weight_rotation', 0.0)
    v.set_editor_property('origin', unreal.Transform(location=unreal.Vector(0.0, y0, 0.0), rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0), scale=unreal.Vector(1, 1, 1)))
    psia.set_editor_property('items', [a, v])
    assert eal.save_loaded_asset(psia, only_if_is_dirty=False)
    print('PSIA', ppath, 'victim origin Y=%.1f' % y0, [(str(i.get_editor_property('role')), i.get_editor_property('animation').get_name()) for i in psia.get_editor_property('items')])
    # ---- its own database (one PSIA per ability: the driver validates PSIA attacker == StrikeMontage)
    dpath = PSI + '/PSD_AZ_Punch_%s' % hand
    db = unreal.load_asset(dpath) if eal.does_asset_exist(dpath) else tools.create_asset('PSD_AZ_Punch_%s' % hand, PSI, unreal.PoseSearchDatabase, None)
    assert db, 'DB create failed ' + hand
    db.set_editor_property('schema', schema)
    db.set_editor_property('pose_search_mode', unreal.PoseSearchMode.BRUTE_FORCE)
    PU.clear_database(db)
    assert PU.add_anim_asset_to_database(db, psia)
    assert PU.set_sampling_range_on_entry(db, 0, 0.0, 0.10)     # entry inside the jab's wind-up (contact 0.17/0.19)
    assert eal.save_loaded_asset(db, only_if_is_dirty=False)
    built[hand] = (db, jab)

# ---- BP_GA_Punch_L/R -> children of AZ_GA_StrikeInteraction (keeps their CDO values + every reference)
for hand, bp_name, jab_path, contact, reach in JABS:
    bp = unreal.load_asset(GA_DIR + '/' + bp_name)
    was_strike = isinstance(unreal.get_default_object(bp.generated_class()), unreal.AZ_GA_StrikeInteraction)
    print(bp_name, 'already a StrikeInteraction:', was_strike)
    if not was_strike:
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.AZ_GA_StrikeInteraction)   # returns None
        assert isinstance(unreal.get_default_object(bp.generated_class()), unreal.AZ_GA_StrikeInteraction), 'reparent failed ' + bp_name
    db, jab = built[hand]
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('strike_database', db)
    cdo.set_editor_property('strike_montage', jab)
    cdo.set_editor_property('max_close_in_distance', 25.0)     # a step, not a snap: 25cm in 0.17s
    cdo.set_editor_property('strike_entry_max_time', 0.15)
    if hand == 'L':
        cdo.set_editor_property('punch_lunge_l', None)          # the warped heavy leaves LMB; the heavy is R's
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert eal.save_loaded_asset(bp, only_if_is_dirty=False)
    cdo = unreal.get_default_object(bp.generated_class())
    print(bp_name, 'is StrikeInteraction:', isinstance(cdo, unreal.AZ_GA_StrikeInteraction),
          'db=', cdo.get_editor_property('strike_database').get_name(), 'montage=', cdo.get_editor_property('strike_montage').get_name(),
          'lunge_l=', cdo.get_editor_property('punch_lunge_l'), 'tag=', cdo.get_editor_property('input_tag').get_editor_property('tag_name'),
          'close<=', cdo.get_editor_property('max_close_in_distance'))

# ---- index builds need an ACCESS: open both DB editors (close them before PIE)
sub = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
sub.open_editor_for_assets([built['L'][0], built['R'][0]])
print('DONE - watch for "PSD_AZ_Punch_L/R BuildIndex Succeeded", then close the DB tabs')
