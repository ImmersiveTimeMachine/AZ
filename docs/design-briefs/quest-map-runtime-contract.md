# Quest/map implementation coordination contract

September19. Full implementation now authorized. Root owns editor mutations, UI/art, build coordination and final integration. All agents write only their assigned source boundaries. No automated tests, agent-started PIE, commits or plugin/source-pack modifications. Preserve existing uncommitted Config/character/throwable changes from other work.

## Agreed integration types

Navigation agent owns `Navigation/AZ_NavigationTypes.h`:

- `FAZ_NavigationTargetDescriptor`: FName TargetId, MapId, LayerId; bool bHasWorldLocation=false; FVector WorldLocation=Zero; float SearchRadius=0. RuntimeActor is NOT serialized in this struct. TargetId may resolve an Actor/SceneComponent through the world target registry. A valid explicit location may exist without a loaded provider.
- `FAZ_MapWaypoint`: bool bActive=false; FName MapId,LayerId; FVector WorldLocation. One personal waypoint for initial UI; quest tracking is separate.
- `UAZ_MapDefinition` PrimaryDataAsset: MapId/LayerId, texture and invertible planar world/image calibration. WorldToMapNormalized/MapNormalizedToWorld return success (reject invalid span/input), no implicit abs or clipping. Root supplies calibrated actual map artwork later.
- `UAZ_NavigationTargetComponent` on authored actors: editable stable FName TargetId/MapId/LayerId; location from SceneComponent/owner, optional explicit offset; registers in `UAZ_NavigationTargetSubsystem` WorldSubsystem. Registry rejects ambiguous duplicate IDs, emits targets-changed, weak refs only, scoped per world. Root/quest agent may query by stable descriptor.

Quest agent owns `Quests/AZ_QuestTypes.h`, `AZ_QuestDefinition.h`, `AZ_QuestProgressComponent.h/.cpp`, reach/interaction providers, PlayerState wiring:

- `EAZ_QuestStatus`: Available, Active, Completed, Failed, Cancelled. Absence from accepted records means not acquired; definition prerequisite checks govern availability. No separate UI focus encoded as quest status.
- `EAZ_QuestObjectiveKind`: ReachArea, Interact, PossessItem, DeliverItem.
- `FAZ_QuestObjectiveDefinition`: FName ObjectiveId; FText Description; kind; int32 RequiredCount>=1; bool bOptional; FGameplayTag ItemType; FAZ_NavigationTargetDescriptor Target; TArray<FName> PrerequisiteObjectives; completion/failure policy explicit in definitions.
- `FAZ_QuestObjectiveProgress`: FName ObjectiveId; typed status, int32 CurrentCount, terminal receipt IDs as needed. `FAZ_QuestProgressRecord`: FName QuestId, TSoftObjectPtr<UAZ_QuestDefinition> Definition, EAZ_QuestStatus Status, TArray<...> Objectives.
- Quest component on PlayerState, authority progress mutations, owner-replicated snapshot/change notifications. `GetQuestRecords`, `FindQuestDefinition`, `GetTrackedQuestId`, `GetTrackedObjectiveId`, `SetTrackedObjective(QuestId,ObjectiveId)`, `GetTrackedTarget(out Descriptor)` are BP-friendly APIs. Exact signatures communicated when headers exist. Tracking requests can RPC with validation; an untracked eligible objective can still progress.
- `AcceptQuest(UAZ_QuestDefinition*)`, `ReportObjectiveFact` / idempotent validated completion API, `RefreshInventoryObjectives`, `TryDeliverObjective(QuestId,ObjectiveId, AActor* Recipient, FGuid ReceiptId)`.
- `ExportProgress` / `RestoreProgress` canonical structs for persistence; no saved widget/actor pointers. Restore rebuilds snapshots without replaying old rewards.
- Reach areas authoritative 3D, explicit instigator PS, active objective guard and already-inside reconciliation. No first-player lookup, no test framework or auto-accept narrative content.

Inventory/persistence agent owns new Quests/AZ_QuestInventoryAdapter.h/.cpp and Game/AZ_CampaignSaveGame.h/.cpp + save coordinator/checkpoint actors; allowed narrow inventory component/fastarray/snapshot extensions, NOT PlayerState or coreQuest/Nav files:

- Shared inventory contract: `CountOwnedItems(APlayerController*, FGameplayTag ItemType)` returns canonical current backpack/equipped (same owned backingitem) quantity once; inserted child handling documented. Does not count callback frequency.
- Delivery must validate target/quest requirements then consume and commit once with receipt before any observable callback. Coordinate API with quest agent; do not expose client arbitrary-consume shortcut. Use reservation/revision/checkpoint guard if needed.
- Capture/restore inventory snapshot data (manifest template/data + state + placements/stack counts), not UObject pointers. Validate all before mutation; preserve exact magazine links/identities, reject active reload/throw/equipment transactions. Root handles equipment/quickbar restore bindings if outside allowedscope.
- User approved autosave at important quest moments + authored save points (campfire can be one presentation). No save-anywhere feature is assumed. Coordinator must create usable versioned snapshot including quest/nav prefs and inventory/player/world requirements, complete save failure handling and player-context API. No tests or editor calls. File-safe writes built from actual Unreal API.

All proposed types can be narrowed/refined with root and peer messages before broad edits. Announce exact header contracts early. Root makes final calls on UI shape, world art and authored asset registration; no agent changes active HUD or opens/calls the editor.
