---
name: project_strafe_system
description: "v2 combat-ready STRAFE locomotion — built+committed 5b34234. Tag→facing→8-way MM (walk/run/crouch), idle turn-in-place, directional starts/stops, jump-explore-only. PLUS the lean-port resume point (additive, in progress). Read first for strafe/lean/aim work."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
---

Combat-ready **strafe** for the v2 hero (`AAZ_PawnMoverHeroCharacter` + `UAZ_MoverAnimInstance` + `CHT_v2_CharacterAnimations`). Built this session, committed `5b34234` (branch `feature/rootmotion`). See [[project_v2_locomotion_progress]], [[project_combat_fist_build_plan]].

## The chain (one tag drives everything)
`FAZ_QuickSlot.bStrafeOnEquip` (tick on the fist slot in `BP_AZ_PlayerController` QuickBar) → on equip `EquipSlot` calls `ASC->AddStateTag(Movement.Strafe)` (replicated state tag; `AddStateTag/RemoveStateTag` now PUBLIC on `UAZ_AbilitySystemComponent`). Then:
- **Facing** — `ProduceInput`: `OrientationIntent = bStrafe ? camera-forward : WorldMove`. Walking mode (`AZ_PawnMovementMode_Walking::GenerateWalkMove`): strafe branch = tight `StrafeFacingTime=0.10` while moving, looser `StrafeIdleFacingTime=0.25` at idle, NO camera-snap-shorten. `CustomInputs.RotationMode` (already in `FAZ_MoverCustomInputs`) carries strafe to the mode.
- **Chooser** — `ChooserContext.bStrafe = OwnedTags.HasTag(Movement.Strafe)`; CHT col 11 = bStrafe BoolColumn.
- **SM** (`UAZ_LocomotionStateMachine`, `FAZ_LocoSMInputs.bStrafe` + `.CameraYawDelta`): strafe SKIPS body-turning starts + reversal pivots; new states **`IdleTurnLeft=10`/`IdleTurnRight=11`** (preempt IdleBreak, enter 10° / exit 4° hysteresis on CameraYawDelta) for animated idle turn-in-place.

## 8-way directional MM (the moving loops)
DB-swap in `SetBlendStackAnimFromChooser` (inside `if (ChooserOut.bUseMM)`): when `bStrafe && LocomotionLoop` → search the gait/stance DB (Crouch>Walk/Run), so MM picks the directional clip (incl. 45/135 diagonals) by trajectory. CHT loop rows 4/8 are `Dir=Any, bUseMM=True` (the row asset is just the fallback; MM overrides — that's why the chooser debug shows WalkFwdLoop while a back-pedal plays WalkBwdLoop).
- **DBs** (`/Game/AZ/Blueprints/Animation/MotionMatching/`, schema `PSS_v2_SurvivalMan_Loco`): `PSD_v2_StrafeRun` / `PSD_v2_StrafeWalk` (8 dirs each), `PSD_v2_StrafeCrouch` (8 crouch dirs). Assigned to `StrafeRun/Walk/CrouchDatabase` UPROPERTYs in the `AZ_ABP_MoverAnimInstance` CDO.
- **Starts/stops** — directional foot-aware CHT rows (forward reuses explore rows pinned `Dir==F`; B/LL/RR new rows). Walk strafe set has full Start+Stop_LU/RU; run lacks them (borrows walk).
- **Jump** — disabled in strafe (`ProduceInput` zeroes jump press when bStrafe). Explore-only for now (decided; physics-jump-in-strafe deferred).

## LEAN — additive (WIP; ★ RESUME HERE = the idle problem below)
**MM-for-lean FAILED → went additive.** MM adding `*FwdLoop_LeanL/R` to DBs → **L/R flip-flop every frame** (lean clips are pose-tilts, trajectory ≈ Fwd, MM can't select). Removed from MM (WalkLoco/RunLoco now 1-clip no-ops; the explore loco DB-swap `else if !bStrafe && LocomotionLoop && Standing → Walk/RunLocoDatabase` is a left-over no-op). Additive instead, mirroring `AZ_ABP_Mover`'s `AdditiveLeans` LinkedAnimLayer.

**DONE this session (committed):**
1. **C++ driver** — `UAZ_MoverAnimInstance::LeanAmount` (FVector2D, BlueprintReadOnly, smoothed `Vector2DInterpTo` 10/s), **FORWARD-gated** (only `bIsMoving && MovementDirection==F`, so explore+strafe both). Ported from `AZ_AnimInstance::Update_AdditiveLean` (`AZ_AnimInstance.cpp:552`): lateral-accel = `Velocity.Rotation().UnrotateVector(VelAccel).Y / mapped-divisor`, clamped ±1. Added `PrevVelocity` for the `VelocityAcceleration` derivative (v2 didn't track it). Built OK.
2. **Lean clips** (gitignored pack `Content/Assets/RTG_AZ/MovementAnimsetPro/`, LOCAL-ONLY): `AnimPro_{Walk,Run}FwdLoop_LeanL/R` set Mesh-Space additive (`AAT_ROTATION_OFFSET_MESH_SPACE`), base = the matching `FwdLoop` via **`ABPT_ANIM_SCALED`** (frame-matched). ★ KEY: `ABPT_ANIM_FRAME` (single frame) BAKES THE WALK LEG-CYCLE into the additive → applied = double-motion = "weird"; loops need AnimScaled so `Lean(t)−Fwd(t)` cancels the walk. The reference uses dedicated static lean POSES (`Lean_M_Relaxed_Walk_Pose_Lean_*` additive over a neutral `Lean_Base`) → AnimFrame is correct THERE precisely because they're static poses with no leg cycle.
3. **BS** `BS_AnimPro_WalkFwdLoop_Lean` (BlendSpace1D, axis "Lean" −1..1, `LeanL@−1` / `LeanR@+1`, both additive). LOCAL-ONLY (gitignored). No 0-sample yet (relies on symmetric cancel at center).
4. **ABP graph** (`AZ_ABP_MoverAnimInstance`) — faithfully mirrored `AdditiveLeans` LinkedAnimLayer: SaveCachedPose `PreLeanPose` → UseCachedPose into **Apply Mesh Space Additive** Base, BlendSpace Player (the lean BS) into Additive. Layer is inserted in the main AnimGraph chain. ★ The BS Player's **`X` pin must bind to `LeanAmount.X`** (Get LeanAmount → Break Vector2D → X) — if left unbound it freezes at X=0 (no lean / "looks like Fwd").

**★ NEXT SESSION START (user's words): "additive over IDLE not working — takes a root anim and so on; need the best way to apply it."** Likely the lean clips carry ROOT (and full-body) delta that the additive injects over the idle base → drift / wrong pose. Options to weigh: (a) **bone-mask the additive to upper body** (spine-up via a blend profile / per-bone weights — `AZ_SkeletonUtils`), strip root; (b) use **static lean POSES** like the reference (no root, no phase dependency) instead of the walk loops; (c) **gate the layer Alpha** by `bIsMoving && forward` so idle gets ZERO additive (the AlphaCurveName/`Alpha` pin on Apply-Mesh-Space-Additive — reference also masks turns with a curve); (d) add the BS 0-sample for a clean center. Lean over moving loco is the WIP; phase-alignment of Lean vs Fwd loops still unverified (if straight-moving looks off, switch to static poses).

## Gotchas burned this session (don't repeat)
- **`bLoop=True` required** on every directional/turn LOOP clip — they ship as one-shots → freeze at last frame. The clips live in **gitignored** `Content/Assets/RTG_AZ/` so the bLoop fix is LOCAL-ONLY (not in the repo; re-import = re-apply).
- **`EditorAssetLibrary.does_asset_exist` / `.load_asset` are FLAKY** from MCP Python (false "missing"). Use the global **`unreal.load_asset`**.
- **DB save**: `save_loaded_asset(db, only_if_is_dirty=False)` — `add_sequences_to_database` may not dirty the asset, so default save silently skips. A two-step create-empty-then-populate left `PSD_v2_StrafeCrouch` **corrupt** → fix = delete + recreate fresh (one-shot create→schema→add→save).
- **Stale makefile after Live Coding**: a CLI build says "Target is up to date" / skips real edits → `git status --short -- Source/AZ | awk '{print $2}' | xargs touch` then rebuild. And the editor being OPEN only errors ("Unable to build while Live Coding active") once there's actual work to link — an "up to date" can hide an open editor.
- **Verify DB content/assignment via `unreal_asset_dependencies`** (a populated DB lists its clips; the ABP lists its assigned DBs) — `get_editor_property` on the AnimBP CDO / PoseSearchDatabase animation list is NOT Python-exposed.
- **MM is for DISCRETE state→clip by trajectory**; continuous overlays (idle-turn, lean) do NOT belong in MM (pose-tilt clips → flip-flop). Idle-turn went to an SMState; lean goes additive.
