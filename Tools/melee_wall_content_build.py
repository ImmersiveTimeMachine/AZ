# @Description: Build stationary shortened jab/retract montages; configure ability defaults separately.
"""Run inside Unreal Editor Python; importing this file does not change assets.

    import runpy
    wall = runpy.run_path('C:/UnrealEngine/Games/AZ/Tools/melee_wall_content_build.py')
    wall['build_content']()

After the native BlockedPunchMontages property has been built and the editor restarted:

    wall['configure_abilities']()

Compile the three returned ability Blueprint paths through the dedicated Blueprint
compile MCP after Python finishes, then save them. Never compile from this script.

Only assets in OWNED_FOLDER are created/rebuilt. Original jab sequences/montages
are read only. Existing wall sequence copies are reused; rebuilding does not copy
later source animation edits onto them. All generated montages use saved package
references, positive montage playback and a negative-rate retract segment.
"""

import gc
import math

import unreal


OWNED_FOLDER = '/Game/AZ/Blueprints/Animation/Melee/Wall'
SOURCE_MONTAGES = {
    'L': '/Game/AZ/Blueprints/Animation/Montage/AM_Fists_Punch_L',
    'R': '/Game/AZ/Blueprints/Animation/Montage/AM_Fists_Punch_R',
}
CUTOFFS = (0.04, 0.07, 0.10, 0.13)
ADVANCE_RATE = 1.0
RETRACT_RATE = 1.0
BLEND_IN = 0.025
BLEND_OUT = 0.04
ABILITY_FOLDER = '/Game/AZ/Blueprints/AbilitySystem/Hero/Abilities'
ABILITY_HANDS = (('BP_GA_Punch_L', 'L'), ('BP_GA_Punch_R', 'R'),
                 ('BP_GA_HeavyStrike', 'L'))
SOURCE_TAG = 'AZ.MeleeWall.Source'


def _require(condition, message):
    if not condition:
        raise RuntimeError('[MeleeWallContent] ' + message)


def _asset(path, asset_type):
    obj = unreal.load_asset(path)
    _require(isinstance(obj, asset_type), 'Missing/wrong asset type: ' + path)
    return obj


def _owned(path):
    package = path.split('.')[0]
    _require(package.startswith(OWNED_FOLDER + '/'),
             'Refusing content modification outside owned folder: ' + path)


def _montage_path(hand, cutoff):
    return OWNED_FOLDER + '/AM_Wall_Jab_%s_%03d' % (hand, round(cutoff * 1000))


def _raw_root(seq):
    """Sample raw root at every key with root locking explicitly bypassed."""
    model = seq.get_editor_property('data_model_interface')
    _require(model is not None, 'No source data model: ' + seq.get_path_name())
    _require(model.is_valid_bone_track_name('root'),
             'Missing root track: ' + seq.get_path_name())
    # GetBoneTrackByName is deprecated and returns empty legacy keys on the live
    # AnimationSequencerDataModel. AnimPose uses that model's real evaluation.
    # IncorporateRootMotionIntoPose sets FAnimExtractContext.bIgnoreRootLock, so
    # ForceRootLock cannot make a still-moving source pass this verification.
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    options.set_editor_property('should_retarget', False)
    options.set_editor_property('extract_root_motion', False)
    options.set_editor_property('incorporate_root_motion_into_pose', True)
    options.set_editor_property('evaluate_curves', False)
    count = model.get_number_of_keys()
    _require(count > 0, 'Empty root animation: ' + seq.get_path_name())
    length = unreal.AnimationLibrary.get_sequence_length(seq)
    pos, rot, scale = [], [], []
    for index in range(count):
        time = length * index / max(1, count - 1)
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, time, options)
        root = unreal.AnimPoseExtensions.get_bone_pose(pose, 'root', unreal.AnimPoseSpaces.LOCAL)
        pos.append(root.translation)
        rot.append(root.rotation)
        scale.append(root.scale3d)
    return model, pos, rot, scale


def _vector(value):
    return unreal.Vector(value.x, value.y, value.z)


def _quat(value):
    return unreal.Quat(value.x, value.y, value.z, value.w)


def _verify_root_constant(seq):
    _, positions, rotations, scales = _raw_root(seq)
    p0, q0, s0 = positions[0], rotations[0], scales[0]
    max_position_error = max(math.sqrt(sum((getattr(p, a) - getattr(p0, a)) ** 2
                                         for a in ('x', 'y', 'z'))) for p in positions)
    max_scale_error = max(abs(getattr(s, a) - getattr(s0, a))
                          for s in scales for a in ('x', 'y', 'z'))
    max_rotation_error = max(min(
        max(abs(getattr(q, a) - getattr(q0, a)) for a in ('x', 'y', 'z', 'w')),
        max(abs(getattr(q, a) + getattr(q0, a)) for a in ('x', 'y', 'z', 'w')))
        for q in rotations)
    _require(max_position_error < 0.0001 and max_scale_error < 0.0001
             and max_rotation_error < 0.0001,
             'Root keys are not constant: ' + seq.get_path_name())
    _require(unreal.AnimationLibrary.is_root_motion_enabled(seq),
             'Root motion must remain enabled for stationary Mover action ownership')
    return max_position_error


def _bake_stationary_root(seq, original):
    _owned(seq.get_path_name())
    model, _, _, _ = _raw_root(seq)
    _, original_positions, original_rotations, original_scales = _raw_root(original)
    # Match the original first-frame root transform; changing only the root track
    # preserves local arm/torso animation and produces exactly zero extracted delta.
    count = model.get_number_of_keys()
    _require(count > 0, 'Empty animation data model: ' + seq.get_path_name())
    controller = seq.get_editor_property('controller')
    if controller is None:
        # UE 5.8 uses AnimationSequencerDataModel, not legacy AnimDataModel.
        # Instantiate the matching controller class, never hardcode AnimDataController:
        # its native SetModel uses CastChecked and would crash on a sequencer model.
        template = original.get_editor_property('controller')
        _require(template is not None, 'No matching source animation controller')
        controller = type(template)(outer=seq)
        controller.set_model(model)
    target = controller.get_model_interface()
    _require(target is not None and target.get_path_name() == model.get_path_name(),
             'Controller does not target the wall sequence copy')
    controller.open_bracket('Bake constant root for wall jab', False)
    try:
        ok = controller.set_bone_track_keys(
            'root', [_vector(original_positions[0]) for _ in range(count)],
            [_quat(original_rotations[0]) for _ in range(count)],
            [_vector(original_scales[0]) for _ in range(count)], False)
        _require(ok, 'set_bone_track_keys failed: ' + seq.get_path_name())
    finally:
        controller.close_bracket(False)
    seq.set_editor_property('rate_scale', 1.0)
    unreal.AnimationLibrary.set_root_motion_enabled(seq, True)
    unreal.AnimationLibrary.set_root_motion_lock_type(
        seq, unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
    unreal.AnimationLibrary.set_is_root_motion_lock_forced(seq, True)
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(seq)
    return _verify_root_constant(seq)


def _source_sequence(hand):
    montage = _asset(SOURCE_MONTAGES[hand], unreal.AnimMontage)
    slots = list(montage.get_editor_property('slot_anim_tracks'))
    _require(len(slots) == 1, 'Expected a single-slot stationary jab: ' + montage.get_path_name())
    segments = list(slots[0].get_editor_property('anim_track').get_editor_property('anim_segments'))
    _require(len(segments) == 1, 'Expected one stationary jab segment: ' + montage.get_path_name())
    segment = segments[0]
    _require(abs(segment.get_editor_property('anim_start_time')) < 0.0001,
             'Stationary jab source no longer starts at zero')
    seq = segment.get_editor_property('anim_reference')
    _require(isinstance(seq, unreal.AnimSequence), 'Jab source is not an AnimSequence')
    _require(unreal.AnimationLibrary.get_sequence_length(seq) > max(CUTOFFS),
             'Jab source is shorter than the longest wall cutoff')
    _raw_root(seq)
    return seq


def _build_montage(hand, cutoff, seq):
    path = _montage_path(hand, cutoff)
    _owned(path)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        _asset(path, unreal.AnimMontage)
    montage = unreal.AZ_MontageUtils.build_sectioned_montage_ranged(
        path, 'FullBody', ['Advance', 'Retract'], [seq, seq],
        ['Retract', unreal.Name('None')], [ADVANCE_RATE, RETRACT_RATE],
        [0.0, 0.0], [cutoff, cutoff], BLEND_IN, BLEND_OUT)
    _require(montage is not None, 'Montage builder failed: ' + path)
    # The current helper clamps negative rates to +epsilon. Build with positive
    # magnitudes, then change direction without changing segment/section duration.
    tracks = list(montage.get_editor_property('slot_anim_tracks'))
    track = tracks[0]
    anim_track = track.get_editor_property('anim_track')
    segments = list(anim_track.get_editor_property('anim_segments'))
    _require(len(segments) == 2, 'Unexpected generated segment count: ' + path)
    segments[1].set_editor_property('anim_play_rate', -RETRACT_RATE)
    anim_track.set_editor_property('anim_segments', segments)
    track.set_editor_property('anim_track', anim_track)
    tracks[0] = track
    montage.set_editor_property('slot_anim_tracks', tracks)
    montage.set_editor_property('rate_scale', 1.0)
    montage.set_editor_property('enable_auto_blend_out', True)
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(montage)
    _require(unreal.EditorAssetLibrary.save_loaded_asset(montage, only_if_is_dirty=False),
             'Failed to save ' + path)
    _verify_montage(montage, cutoff)
    print('[MeleeWallContent] BUILT', path,
          'duration=%.3f' % unreal.AnimationLibrary.get_sequence_length(montage),
          list(unreal.AZ_MontageUtils.dump_montage_sections(montage)))
    return montage


def _verify_montage(montage, cutoff):
    expected = cutoff / ADVANCE_RATE + cutoff / RETRACT_RATE
    _require(abs(unreal.AnimationLibrary.get_sequence_length(montage) - expected) < 0.0001,
             'Incorrect montage length: ' + montage.get_path_name())
    tracks = list(montage.get_editor_property('slot_anim_tracks'))
    _require(len(tracks) == 1 and str(tracks[0].get_editor_property('slot_name')) == 'FullBody',
             'Incorrect montage slot: ' + montage.get_path_name())
    segments = list(tracks[0].get_editor_property('anim_track').get_editor_property('anim_segments'))
    _require(len(segments) == 2, 'Incorrect montage segment count')
    for i, rate in enumerate((ADVANCE_RATE, -RETRACT_RATE)):
        _require(abs(segments[i].get_editor_property('anim_play_rate') - rate) < 0.0001
                 and abs(segments[i].get_editor_property('anim_start_time')) < 0.0001
                 and abs(segments[i].get_editor_property('anim_end_time') - cutoff) < 0.0001,
                 'Incorrect montage range or play rate: ' + montage.get_path_name())
        _verify_root_constant(segments[i].get_editor_property('anim_reference'))
    _require(not list(unreal.AnimationLibrary.get_animation_notify_events(montage)),
             'Wall response must have no gameplay notifies')


def build_content():
    """Create/save two constant-root jab copies and eight advance/retract montages."""
    try:
        # Validate both originals before creating or changing any wall content.
        originals = {hand: _source_sequence(hand) for hand in SOURCE_MONTAGES}
        result = {}
        for hand, original in originals.items():
            path = OWNED_FOLDER + '/AS_Wall_Jab_' + hand
            _owned(path)
            if unreal.EditorAssetLibrary.does_asset_exist(path):
                seq = _asset(path, unreal.AnimSequence)
                provenance = unreal.EditorAssetLibrary.get_metadata_tag(seq, SOURCE_TAG)
                _require(not provenance or provenance == original.get_path_name(),
                         'Existing wall sequence has a different recorded source: ' + path)
            else:
                seq = unreal.EditorAssetLibrary.duplicate_asset(original.get_path_name(), path)
                _require(isinstance(seq, unreal.AnimSequence), 'Duplicate failed: ' + path)
            error = _bake_stationary_root(seq, original)
            unreal.EditorAssetLibrary.set_metadata_tag(seq, SOURCE_TAG, original.get_path_name())
            _require(unreal.EditorAssetLibrary.save_loaded_asset(seq, only_if_is_dirty=False),
                     'Failed to save constant-root source: ' + path)
            print('[MeleeWallContent] SOURCE', path, 'root_key_error_cm=%.8f' % error)
            result[hand] = [_build_montage(hand, t, seq).get_path_name() for t in reversed(CUTOFFS)]
        print('[MeleeWallContent] BUILD COMPLETE; ability defaults are not configured.', result)
        return result
    finally:
        gc.collect()


def configure_abilities():
    """Assign cooked hard references after the native property exists; never compile."""
    try:
        montages = {}
        for hand in SOURCE_MONTAGES:
            montages[hand] = []
            for cutoff in reversed(CUTOFFS):
                montage = _asset(_montage_path(hand, cutoff), unreal.AnimMontage)
                _verify_montage(montage, cutoff)
                montages[hand].append(montage)
        pending = []
        for name, hand in ABILITY_HANDS:
            path = ABILITY_FOLDER + '/' + name
            bp = _asset(path, unreal.Blueprint)
            cdo = unreal.get_default_object(bp.generated_class())
            # Preflight all CDOs before writing any. Missing property means the
            # editor must be restarted after the full native build, not hot patched.
            try:
                cdo.get_editor_property('blocked_punch_montages')
            except Exception as error:
                raise RuntimeError('BlockedPunchMontages is unavailable on ' + path
                                   + '; complete the full build/restart first') from error
            pending.append((bp, cdo, path, hand))
        for bp, cdo, path, hand in pending:
            cdo.set_editor_property('blocked_punch_montages', montages[hand])
            _require(unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False),
                     'Failed to save ability defaults: ' + path)
            actual = [m.get_path_name() for m in cdo.get_editor_property('blocked_punch_montages')]
            _require(actual == [m.get_path_name() for m in montages[hand]],
                     'Ability reference read-back mismatch: ' + path)
            print('[MeleeWallContent] CONFIGURED', path, actual)
        paths = [item[2] for item in pending]
        print('[MeleeWallContent] Compile these with the dedicated Blueprint MCP, then save:', paths)
        return paths
    finally:
        gc.collect()


if __name__ == '__main__':
    print('[MeleeWallContent] Loaded only. Call build_content() or configure_abilities() explicitly.')
    gc.collect()
