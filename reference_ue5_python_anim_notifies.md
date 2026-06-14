---
name: reference_ue5_python_anim_notifies
description: "Working pattern for adding PoseSearch anim notify states to AnimSequences via C++ bridge (Python can't access Notifies directly)"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 787f844b-69e1-48c0-8b39-9a9264829d57
---

## Adding Anim Notifies via Python in UE5.7

Python **cannot** directly access `AnimSequence.Notifies` — it's a protected property.

### Solution: C++ Bridge (`UAZ_PoseSearchUtils`)

Location: `Source/AZ/Public/Animation/AZ_PoseSearchUtils.h`

**Functions:**
- `AddBlockTransitionNotify(Sequence, StartTime, Duration)` — adds `UAnimNotifyState_PoseSearchBlockTransition` to a single anim
- `AddBlockTransitionToDatabase(Database)` — batch: adds BlockTransition to all anims in a PoseSearch database (80% coverage, 10% margin at start/end)

**Python call:**
```python
import unreal
db = unreal.load_asset("/Game/AZ/Blueprints/Animation/MotionMatching/Databases/PSD_NoWeapon_WalkLoops")
count = unreal.AZ_PoseSearchUtils.add_block_transition_to_database(db)
# Save all modified animations
for i in range(db.get_num_animation_assets()):
    anim = db.get_animation_asset(i)
    if anim:
        unreal.EditorAssetLibrary.save_loaded_asset(anim)
unreal.EditorAssetLibrary.save_loaded_asset(db)
```

### C++ Implementation Key Points
- `NewObject<UAnimNotifyState_PoseSearchBlockTransition>(Sequence, NAME_None, RF_Transactional)` to create notify
- Add to `Sequence->Notifies` array directly (accessible from C++, not Python)
- Set `NotifyEvent.NotifyStateClass = NotifyState` (the instance, not the UClass)
- Call `NotifyEvent.LinkSequence(Sequence, StartTime)` and `NotifyEvent.SetTime(StartTime)`
- Call `Sequence->Modify()`, `PostEditChange()`, `MarkPackageDirty()` to persist

## ★ UPDATE 2026-06-12 (UE 5.8) — notifies ARE readable, and the C++ bridge makes them EDITOR-INVISIBLE

**READ path (corrects line 9 above):** raw `seq.notifies` / `get_editor_property("notifies")` still throw, BUT `unreal.AnimationLibrary` reads them fine:
- `get_animation_notify_events(seq)` → all FAnimNotifyEvent (len = total count)
- `get_animation_notify_event_names(seq)` → names (e.g. `PoseSearchBranchIn`)
- `get_animation_notify_track_names(seq)` → named tracks (e.g. `"1"`, `"2"`)
- `get_animation_notify_events_for_track(seq, "1")` → events ON that track
- Non-destructive count diagnostic now available; no longer need `RemoveAllPoseSearchNotifies`' return value to probe.

**★★ THE VISIBILITY TRAP (the "it works but I can't see the notify" bug):** `UAZ_PoseSearchUtils::AddBranchInNotify` (and all our Add*Notify) do `Sequence->Notifies.AddDefaulted_GetRef()` — they append to the raw array but DON'T register the event on a named notify TRACK. MM reads the raw array so the notify is fully FUNCTIONAL, but the editor's notify panel only DRAWS events bound to a named track → the notify is invisible (`get_animation_notify_events`=1 but `get_animation_notify_events_for_track("1")`=0 ⇒ "orphaned"). This is why a present, working BranchIn looked "gone." Diagnose: `orphaned = total_events - sum(per-track events)`; orphaned>0 ⇒ invisible.

**FIX = add via the track-aware AnimationLibrary API instead** (visible + no build needed):
```python
AL = unreal.AnimationLibrary
AL.remove_animation_notify_events_by_name(seq, "PoseSearchBranchIn")   # clean, track-aware, returns count
if "1" not in [str(t) for t in AL.get_animation_notify_track_names(seq)]:
    AL.add_animation_notify_track(seq, "1")
ns = AL.add_animation_notify_state_event(seq, "1", 0.0, seq.get_play_length(),
                                         unreal.AnimNotifyState_PoseSearchBranchIn)  # -> the created state
ns.set_editor_property("database", db)            # BranchIn needs its DB (mandatory)
unreal.EditorAssetLibrary.save_loaded_asset(seq, only_if_is_dirty=False)
```
`add_animation_notify_state_event(seq, track, start, duration, NotifyStateClass) -> AnimNotifyState`. Returns the live notify instance → set its props (e.g. BranchIn `database`, ModifyCost `cost_addend`) AFTER. `add_animation_notify_track(seq, name, color=[1,1,1,1])`. **Permanent fix (deferred, needs build): make `AddBranchInNotify` set the event's TrackIndex + register it on a named track so future C++ adds are visible.** After any scripted notify edit, CLOSE+REOPEN the clip tab (Don't Save) to refresh the Slate panel — and a stale open tab can re-save over the script (see [[feedback_parallel_editor_edits]]).

### Available PoseSearch Notify States (UE 5.7)
| Notify | Purpose |
|--------|---------|
| `PoseSearchBlockTransition` | Blocks MM from transitioning INTO this pose during range |
| `PoseSearchExcludeFromDatabase` | Excludes range from database index entirely |
| `PoseSearchModifyCost` | Adds cost bias to poses in range |
| `PoseSearchOverrideContinuingPoseCostBias` | Controls how "sticky" current pose is |
| `PoseSearchBranchIn` | Marks allowed entry points for transitions |
| `PoseSearchIKWindow` | Marks IK blending ranges during transitions |
| `PoseSearchSamplingEvent` | Tags poses with events for query matching |
