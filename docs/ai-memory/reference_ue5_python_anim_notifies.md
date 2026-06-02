---
name: reference_ue5_python_anim_notifies
description: Working pattern for adding PoseSearch anim notify states to AnimSequences via C++ bridge (Python can't access Notifies directly)
type: reference
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
