# GASP Spin Bug — Solution

## Root cause

GASP's mesh stays glued to the capsule because **the capsule yaw itself is smoothed**: GASP routes Walking through `BP_MovementMode_Walking` (parent `USmoothWalkingMode`) which runs every facing change through `SpringMath::CriticalSpringDamperQuat` with per-state `FacingSmoothingTime` (Walk/Run 0.4s, Sprint 0.8s, Idle 0.2s, Slide 0.15s) and a single-spring damper (`bSmoothFacingWithDoubleSpring=False`). The OffsetRootBone + Steering anim nodes are tuned around this smooth-capsule contract — they expect the capsule to rotate at a known, bounded rate. AZ's pawn (`AAZ_HeroPawn`, registered in `Config/DefaultEngine.ini`) leaves `MovementModes` empty so Mover falls back to engine `WalkingMode`, which **snaps capsule yaw instantly to DesiredFacing**. The visible "spin" is that snap: the capsule jumps, the OffsetRootBone tries to absorb the delta into the mesh, but with AZ's current binding gaps (rotation-halflife pin not bound; Steering's `Get_DesiredFacing` chain unverified) the absorption is incomplete and visibly lags. Sources: `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\gasp_movement_modes.md` §1, §6, §7; `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\gasp_animbp_full_audit.md` §2.1, §10 (divergences A, H, I).

---

## Fix path — phased, in priority order

### Phase 1: Bind OffsetRootBone rotation pins in `AZ_ABP_Mover` (lowest risk, highest immediate impact)

**Change:** In `C:\UE57\Games\AZ\Content\AZ\Blueprints\Animation\AZ_ABP_Mover.uasset`, on the `AnimGraphNode_OffsetRootBone` (Tag=`OffsetRoot`), add property bindings:
- `RotationHalfLife` pin → bind to `UAZ_AnimInstance::Get_OffsetRootRotationHalfLife()` (already declared at `C:\UE57\Games\AZ\Source\AZ\Public\Animation\AZ_AnimInstance.h:871`, returns `OffsetRootRotationHalfLife` UPROPERTY default 0.05s).
- `MaxRotationError` pin → bind to `UAZ_AnimInstance::Get_OffsetRootMaxRotationError()` (header line 875, returns `OffsetRootMaxRotationError` default 10°).

**Why:** AZ DIVERGENCE A in `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\gasp_animbp_full_audit.md` §2.1: AZ's OffsetRootBone literal `RotationHalfLife=0.2` (matches GASP) and `MaxRotationError=-1` (no clamp). The C++ tuning UPROPERTYs (`OffsetRootRotationHalfLife=0.05`, `OffsetRootMaxRotationError=10°`) exist but are not wired. With `0.05s` half-life the mesh re-aligns to the capsule ≈4× faster than the GASP default, and the 10° clamp bounds worst-case visual drift.

**Expected impact:** Once the capsule does its instant-snap, the mesh catches up in ≈0.05s instead of 0.2s, and never visibly trails by more than 10°. **Does not** fix the underlying instant-snap; only its visible artefact.

**Dependencies:** None. The functions exist; just add the BP bindings.

**Risk:** Very low. If the bindings break, the node falls back to literal defaults (current behaviour).

---

### Phase 2: Replace engine `WalkingMode` with `BP_MovementMode_Walking` on `AAZ_HeroPawn` (THE fix)

**Change A (option 1, fastest — re-target):** On `C:\UE57\Games\AZ\Content\AZ\Blueprints\Character\Hero\AZ_BP_HeroPawn.uasset` open the `CharacterMoverComponent` defaults panel; in `MovementModes`, set:
- `Walking` → `/Game/Blueprints/MovementModes/BP_MovementMode_Walking_C` (the GASP BP imported into AZ at this path; verified live by `gasp_pawn_bp_full.md` §9).
- `Falling` → `/Game/Blueprints/MovementModes/BP_MovementMode_Falling_C`.
- `Sliding` → `/Game/Blueprints/MovementModes/BP_MovementMode_Slide_C` (only if AZ wants slide; otherwise leave engine default).

**Change B (option 2, harder but project-owned — subclass in C++):** Create `Source/AZ/Public/Movement/AZ_MovementMode_Walking.h` inheriting `USmoothWalkingMode` and replicate the BP's `GenerateWalkMove` override (per-tick `MaxSpeedOverride/Acceleration/Deceleration/TurningStrength/FacingSmoothingTime` writes derived from `FAZ_MoverCustomInputs.Gait` + `bWantsToCrouch`; the `OverridenDesiredFacing` ±179° clamp; the camera-snap-protection sub-clamp). Audit at `gasp_movement_modes.md` §1 (CDO defaults table at lines 38–55, Phase-2/3 logic at lines 104–122). Then point `AAZ_HeroPawn` ctor's `MovementModeMap` and the BP defaults at the new C++ class.

**Recommendation:** Start with **option 1** today — it's a 30-second BP edit with zero compile risk. Move to option 2 only if AZ wants to own the values or extend the logic.

**Why:** This is the actual contract violation. Source: `gasp_movement_modes.md` §6 ("GASP's mesh stays on the capsule because the capsule yaw is itself smooth. Engine WalkingMode … snaps capsule yaw instantly. The Steering anim node is tuned around this curve — feeding it instant snaps causes the visible mesh-vs-capsule lag we have now.") and §7 ("AZ today uses engine defaults … verified live 2026-05-04 via Python introspection of `AZ_BP_HeroPawn.CharacterMoverComponent.movement_modes`").

**Expected impact:** This is THE fix. After Phase 2, the capsule rotates over 0.4s (run) / 0.2s (idle) / 0.8s (sprint) instead of one tick, the Steering nodes get the input rate they were tuned for, and the visible spin disappears. Together with Phase 1's ≤10° clamp, mesh-vs-capsule drift becomes imperceptible.

**Dependencies:** `BP_MovementMode_Walking_C` and `BP_MovementMode_Falling_C` must be present at `/Game/Blueprints/MovementModes/` in AZ. Per `gasp_pawn_bp_full.md` §9 these are imported. Verify via the Content Browser before editing the pawn defaults.

**Risk:** Low. The BPs are GASP's reference impl — they assume the same `S_MoverCustomInputs` shape (Gait, RotationOffset, WantsToCrouch). AZ's `FAZ_MoverCustomInputs` (in `C:\UE57\Games\AZ\Source\AZ\Public\Animation\AZ_LocomotionTypes.h:148-242`) has the matching field names and types. The one tweak: `BP_MovementMode_Walking.GenerateWalkMove` Phase-1 reads `Get Data From Collection<S_MoverCustomInputs>` by struct type — it will pull AZ's struct as long as the InputCollection contains it. AAZ_HeroPawn already adds `FAZ_MoverCustomInputs` to its `ProduceInput` cmd (header line 124, see `project_session_2026-04-22_gasp_pawn_done.md`). **However**: the BP literally references the GASP struct path `/Game/Blueprints/Data/S_MoverCustomInputs` not AZ's `FAZ_MoverCustomInputs` C++ struct — if AZ's collection only carries the C++ struct, the BP will read defaults. Confirmation needed in PIE: place a print on `MoverCustomInputs.Gait` inside the BP's `GenerateWalkMove` Phase-1 and verify it prints the expected gait. If empty → option 2 (C++ subclass) is required.

---

### Phase 3: Verify the Steering chain inside `AZ_ABP_Mover` BlendStack inner graph

**Change:** Open `AZ_ABP_Mover` → double-click the `State Machine Blend Stack` node to enter the inner graph. Visually confirm (per `gasp_animbp_full_audit.md` §3, §10 divergences H, I, and feedback in `feedback_animgraph_node_reference_wiring.md`):
- All `K2Node_AnimNodeReference` instances tagged `State Machine Blend Stack Input` are tag-wired to: `Get_DesiredFacing` (×2 — Steering #1 and #2), `Get_DynamicPlayRate`, `EnableSteering`, `Get_ProceduralTargetTime`, `Get_StrideWarpAlpha`, `Get_StrafeWarpAlpha`.
- Steering #1 has `bAlphaBoolEnabled` bound to `EnableSteering` (otherwise steering never activates).
- Steering #2 has `Alpha=1.0` literal (always-on for TIP).
- `OrientationWarping.LocomotionDirection` bound to `Get_StrafeWarpDirection`; `Alpha` bound to `Get_StrafeWarpAlpha`.
- `StrideWarping.Alpha` bound to `Get_StrideWarpAlpha`; `LocomotionSpeed` bound to `Speed2D`.

Reconnect any unwired nodes. AZ has only 31 inner nodes vs GASP's 40, so at least 9 helper nodes are likely missing entirely — add them per `gasp_animbp_full_audit.md` §3.7 table.

**Why:** Per the wiring-rule memory (`feedback_animgraph_node_reference_wiring.md`), `FAnimNodeReference`-taking helpers fail-closed-silently if their `K2Node_AnimNodeReference` isn't tag-wired. A silently-disabled `Get_DesiredFacing` would feed the Steering node `(0,0,0,1)` identity quat, so the Steering node would not steer the pose toward the trajectory facing. That would *also* contribute to mesh-vs-capsule lag, even after Phase 2 makes the capsule smooth.

**Expected impact:** Steering anim nodes now actually rotate the mesh towards Trj_FutureFacing, which makes pre-empted turns feel anticipatory rather than reactive. Closes the long tail of "the capsule started turning but the mesh feet are still pointed forward".

**Dependencies:** Phase 2 must be in place — without smooth capsule motion, Steering can over-correct and produce a different visible glitch (over-rotated lower body).

**Risk:** Medium-low. Visual: if mistakenly bound, may briefly cause sliding feet on direction reversals (same symptom as today, just different cause). Mitigated by carefully re-using the BS-input ref pattern documented in §3.7.

---

### Phase 4: Add `Update_Trajectory` real call into `Update_Logic` (verify, not necessarily new code)

**Change:** Confirm that `UAZ_AnimInstance::Update_Trajectory` (declared at `C:\UE57\Games\AZ\Source\AZ\Public\Animation\AZ_AnimInstance.h:55`) is actually wired into the orchestration. The header documents it as part of `Update_Logic` (line 50: "Trajectory → EssentialValues → States → AimOffset → AdditiveLean"), but the AnimBP audit (`gasp_animbp_full_audit.md` §5.2, §5.4) makes it explicit: it MUST call `PoseSearchGenerateTrajectoryUsingPredictor` every frame with `Predictor=Predictor`, then `HandleTrajectoryWorldCollisions`, then derive Trj_*Velocity at the GASP sample times. Without these, Trj_FutureFacing is identity and `Get_DesiredFacing` (Phase 3) returns identity → no steering.

Read `C:\UE57\Games\AZ\Source\AZ\Private\Animation\AZ_AnimInstance.cpp` to confirm Update_Trajectory's body matches the recipe in `gasp_animbp_full_audit.md` §5.4 (lines 558–600). If `Trj_FutureFacing` looks ZeroRotator at runtime, this is the smoking gun.

**Why:** Steering has no target without trajectory. Source: `gasp_animbp_full_audit.md` §5.4, §5.15.

**Expected impact:** Pre-empted facing motion feeds Steering with real targets; mesh starts turning *before* the capsule does on player-input changes.

**Dependencies:** None. This is verification + bug-hunt of existing C++.

**Risk:** None to add a missing call; if found wrong, fixing it is a cpp-only edit.

---

### Phase 5: Tune `IsMoving` to GASP semantics (optional refinement)

**Change:** In `C:\UE57\Games\AZ\Source\AZ\Public\Animation\AZ_AnimInstance.h:910-915`, soften the threshold:
```cpp
// Before: SizeSq > 100 (10 cm/s) AND > 1 (1 cm/s²)
// After:  SizeSq > KINDA_SMALL_NUMBER * 10 (matches GASP's default tolerance)
return !FutureVel2D.IsNearlyZero() && !Accel2D.IsNearlyZero();
```

**Why:** AZ DIVERGENCE J (`gasp_animbp_full_audit.md` §5.7). AZ's tighter threshold suppresses sub-walk-speed micro-movements, which can cause `Get_OffsetRootTranslationMode` to flip into `Release` while the player is still gently inputting — that visibly snaps the root translation to capsule centre at the wrong moment. Particularly relevant for camera-driven turning while standing still where the player is barely deflecting the stick.

**Expected impact:** Smoother transition between idle and slow-walk states; fewer micro-glitches at low speed. Probably not the primary spin cause.

**Dependencies:** None.

**Risk:** Very low. May cause `IsMoving` to chatter at exact-zero input — guard with a small velocity floor if observed.

---

### Phase 6 (deferred): Port the AnimBP-side fixes flagged in the audit but not directly relevant to spin

Documented in `GASP_GAPS.md`. Not blocking the spin fix:
- AnimBP DIVERGENCE B (`PoseSearchHistoryCollector.RootBoneRecoveryTime`)
- DIVERGENCE C (`bAllowConduitEntryStates`)
- DIVERGENCE D, E (BlendStack BlendOption / BlendTime literals)
- DIVERGENCE F, G (missing MM-only path)
- DIVERGENCE K (AnimationAlmostCompleteThreshold)
- DIVERGENCE L (Biped_FootPlacement_OnBecomeRelevant wiring)

---

## Verification plan

After Phase 1 (OffsetRootBone bindings) — PIE-test:
- Stand still, whip mouse 90° in 100ms. Expected: mesh visibly trails capsule by ≈4° max instead of ≈17°. Should NOT fix the underlying spin (capsule still snaps); just bounds the visible artefact.
- Sprint in a circle. Expected: no change vs current behaviour (mesh already locked to capsule during sustained turning).

After Phase 2 (BP_MovementMode_Walking re-target) — PIE-test:
- Stand still, whip mouse 90° in 100ms. Expected: capsule rotates over ~0.2s (IdleFacingTime), mesh follows on a smooth ramp. **The spin should disappear.**
- Walk forward, whip mouse 90°. Expected: capsule rotates over 0.4s (Walk/RunFacingTime). Body anim turns naturally with no visible decoupling.
- Sprint, whip mouse. Expected: wide arc over 0.8s (SprintFacingTime).
- Land from a jump. Expected: 0.2s of "sticky" landing (JustLanded brake at 20000 cm/s² per `gasp_movement_modes.md` §1 then_3). Verify no excessive coast-after-landing.
- Verify `MoverCustomInputs.Gait` reads correctly inside the BP's Phase-1: temporarily add a `Print String` node in `BP_MovementMode_Walking.GenerateWalkMove` printing the cached `MoverCustomInputs.Gait` — should print Walk/Run/Sprint per input. **If always Run (default), AZ's struct isn't being recognized** → escalate to option 2 (C++ subclass).

After Phase 3 (BlendStack chain) — PIE-test:
- Walk forward, then change input direction by 90°. Expected: mesh begins anticipating the turn ~0.2s before the capsule yaw moves (Steering is now driving toward Trj_FutureFacing). Foot plants align with the new direction.
- Pivot from full sprint to opposite direction. Expected: smooth pivot anim, no skating.

After Phase 4 (Update_Trajectory verify) — PIE-test:
- In an AnimInstance debug overlay or via `bShowAZDebug`, watch `Trj_FutureFacing` and `Trj_FutureVelocity` while running. Expected: non-zero, leading current state by 0.4s.

What this fix path should NOT be expected to address:
- Wrong-foot starts at gait transitions (needs Phase curve baked on retargeted anims — see `GASP_GAPS.md` "Missing animation curves").
- Chooser missing rows for AZ's anim catalog (see `GASP_GAPS.md` "Missing CHT_RotationOffsetCurve and 7 curves").
- Strafe-mode foot phase selection inversions (data-model enum swaps; see `GASP_GAPS.md` "Enum value flips").

---

## What this fix does NOT address

1. **Chooser rows for stand turn-in-place anims.** Even with smooth capsule rotation, if `CHT_MoverCharacterAnimations` (in AZ via `UAZ_AnimInstance::CharacterAnimationChooser`, header line 255) has no row matching `(StateMachineState=IdleLoop, RotationMode=Aiming, MovementState=Idle)` → ShouldTurnInPlace fires → chooser returns `bNoValidAnim=true` → SM jumps to looping state with no anim → visible A-pose. This is documented in `project_idle_tip_implementation.md` and is a separate work item.
2. **Phase curve missing on retargeted RTG locomotion anims.** Per `gasp_actor_components_and_notifies.md` §5: the Phase curve (oscillating 0/1 between feet) is what MotionMatching uses to align gait phase between candidate poses. Without it, MM picks wrong-foot starts. Independent of the capsule-rotation contract. Fix path: apply `AM_BakePhaseCurveFromFootstepNotifies` to all locomotion anims after first placing `Foley_Walk_L`/`_R` notifies on them.
3. **`movedata_speed` curve missing.** `Get_DynamicPlayRate` (`gasp_animbp_full_audit.md` §5.17) reads this curve to scale anim playback to capsule speed. Without it, anims play at base rate regardless of speed — causes foot skating.
4. **Enum value divergences.** `EAZ_MovementState` flipped (Idle↔Moving), `EAZ_MovementDirection` LL↔LR swapped at indices 2/3, `EAZ_StateMachineState` 1..5 reordered, `EAZ_TraversalActionType` Vault↔Hurdle swapped (`gasp_data_model_full.md` §7). Until fixed, any Chooser cross-referencing GASP enum integers will silently mis-match. Not the spin cause; do separately.
5. **Falling mode capsule rotation.** `BP_MovementMode_Falling` writes `OutProposedMove.AngularVelocityDegrees` via `MovementUtils::ComputeAngularVelocityDegrees(TurningRateLimit=300)` (`gasp_movement_modes.md` §2). Engine `FallingMode` does not. Phase 2 option-1 also fixes this; option-2 needs a parallel `AZ_MovementMode_Falling` subclass.
6. **The TIP system itself.** AZ's `bIdleTurnInProgress` accumulator (header line 543) is independent of this fix path. The accumulator is documented in `project_idle_tip_implementation.md` and is working at the baseline tuning. The spin fix above will not regress it.
7. **GASP `JustLanded` 20000 cm/s² brake.** Comes for free with Phase 2 option-1. If you go option-2 (C++), you must replicate the `OnActivated` event chain (`gasp_movement_modes.md` §1 EventGraph block) that latches `JustLanded=true` for 200ms after Falling→Walking.
8. **Camera framework / GameplayCameras port.** GASP delegates per-stance camera tuning to the GameplayCameras plugin's CameraDirector + 12 CameraRigs (`gasp_framework_cameras_rigs.md` §3). AZ's `UAZ_PawnCameraMovementComponent` does this inline. Not relevant to capsule rotation.
9. **Traversal (vault/hurdle/mantle).** Requires `AC_TraversalLogic` + `MotionWarpingComponent` + chooser + montages — large workstream documented in `gasp_actor_components_and_notifies.md` §1.2. Independent.
