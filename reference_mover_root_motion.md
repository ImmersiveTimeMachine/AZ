---
name: reference_mover_root_motion
description: "Complete map of how the UE 5.8 Mover plugin supports ROOT MOTION (validated 2026-06-27 by 5-agent source sweep of Engine/Plugins/Experimental/Mover). The three RM layered moves (attribute + montage GT + montage async/SimDriven), how RM is triggered (Play Mover Montage proxy / QueueLayeredMove), the RM+Falling per-axis combo, motion-warping (GT adapter vs ChaosMover blackboard), backend split (AZ runs NetworkPrediction → which paths are live vs dormant), replication, all RM CVars, and the OverrideAll trap. Read FIRST for any Mover root-motion / montage / warping work."
metadata:
  node_type: memory
  type: reference
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
---

# Mover plugin — root-motion support (complete map)

Validated 2026-06-27 by a 5-agent full-source sweep of `C:\UnrealEngine\Engine\Plugins\Experimental\Mover\` after the user synced the Mover plugin to a newer CL (HEAD touched `MoverComponent.cpp` + `MoverNetworkPredictionLiaison.*`; the sync brought in the whole MONTAGE-RM family that AZ did not have before). Supersedes/expands [[project_root_motion_mode]] (which only covered the attribute path). File:line anchors are from that CL — re-verify before quoting as gospel.

## Mental model (internalize this)
1. **A movement MODE never reads root motion.** `WalkingMode`/`FallingMode` are pure intent + gravity. RM is delivered as a separate **layered move** that outputs a **velocity** (`RM delta ÷ dt → LinearVelocity + AngularVelocityDegrees`), never a transform.
2. **`MixMode` decides who wins.** The state machine mixes layered moves BEFORE the mode runs. If the combined move is `OverrideAll` and CVar `mover.SkipGenerateMoveIfOverridden` is set, **the mode's GenerateMove is skipped entirely** (`MovementModeStateMachine.cpp:364-366`). That's why an RM move (default `OverrideAll`) fully replaces locomotion. `EMoveMixMode` (`MoveLibrary/MovementUtilsTypes.h:17`): AdditiveVelocity=0, OverrideVelocity=1, OverrideAll=2, OverrideAllExceptVerticalVelocity=3.
3. **Mover does NOT consume `UAnimInstance::RootMotionMode`.** It either re-extracts RM from a montage itself, or reads a `"RootMotionDelta"` mesh attribute. Two families:

## The three RM layered moves (the entire surface)
All descend from `FLayeredMoveBase` (`LayeredMove.h:74`; key fields MixMode `:83`, Priority `:87`, DurationMs `:94` [>0 expires / ==0 ticks once / <0 manual-end], FinishVelocitySettings `:102`). Queue via `UMoverComponent::QueueLayeredMove(TSharedPtr<FLayeredMoveBase>)` (`MoverComponent.h:314`, clones, activates next sim frame). Group API: `CancelFeaturesWithTag`/`HasFeaturesWithTag` (`MoverComponent.h:363-370`), `FindActiveMove<T>`/`ForEachActiveMoveOfType<T>`.

| Move | Header | Source | Thread | Self-end | Tag | AZ uses? |
|---|---|---|---|---|---|---|
| `FLayeredMove_RootMotionAttribute` | `DefaultMovementSet/LayeredMoves/RootMotionAttributeLayeredMove.h:19` | `"RootMotionDelta"` mesh attribute @ bone 0 (written by the AnimationWarping IAnimRootMotionProvider when a sampled seq `bEnableRootMotion`) | GT | persistent (`DurationMs=-1`); returns false (no contribution) on attribute-less frames AND on `bIsJumpJustPressed`/`Mover.SkipAnimRootMotion` (air-control escape) unless under montage warp influence | `Mover.AnimRootMotion.MeshAttribute` | **YES** — the hero's sequence-RM chain ([[project_root_motion_mode]]) |
| `FLayeredMove_AnimRootMotion` | `.../AnimRootMotionLayeredMove.h:32` | a **montage** on the mesh, re-extracted via `UMotionWarpingUtilities::ExtractRootMotionFromAnimation` | GT | ends when `Montage_IsPlaying`==false (`cpp:44-64`) | `Mover.AnimRootMotion.Montage` | **THIS ONE for us** (NPP) — NEW from the sync |
| `FLayeredMove_AnimRootMotion_SimDriven` | `.../AnimRootMotionLayeredMove.h:79` (child of the above) | same montage, worker-thread authoritative | async | direction-aware position/duration thresholds; flips MixMode to `OverrideAllExceptVerticalVelocity` on blend-out (`cpp:407-410`); NetSerializes lifecycle flags for resim | `Mover.AnimRootMotion.Montage` | **NO** — ChaosMover-only, dormant for us |

All default to `MixMode=OverrideAll` in their ctor. RootMotionAttribute sanitizes attribute scale→1 (`cpp:84`) and warns (no clamp) past `mover.debug.RootMotionAttributesExcessiveSpeedSq` (default 2000² cm/s). Montage moves have no such sanitization.

## How you TRIGGER a montage RM move
- **High-level (recommended):** `UPlayMoverMontageCallbackProxy::CreateProxyObjectForPlayMoverMontage(MoverComp, Montage, PlayRate=1, StartPos=0, StartSection=None, bStopAll=true, BlendOutOverride=-1)` (`PlayMoverMontageCallbackProxy.h:29`). BP node = **"Play Montage (Mover Actor)"** (category Animation|Montage). It: plays the anim, `PushDisableRootMotion()` on the montage instance (so anim doesn't double-apply), `CancelFeaturesWithTag(Mover.AnimRootMotion.Montage)` to enforce ONE RM montage at a time (`cpp:62-63`), computes `DurationMs` from remaining montage time, and `QueueLayeredMove`s the right variant via `IsBackendAsync()`. Delegates: `OnCompleted/OnBlendOut/OnInterrupted/OnNotifyBegin/OnNotifyEnd`. **MixMode is hardcoded `OverrideAll`** here (the only knob the proxy doesn't expose).
- **Low-level:** `MakeShared<FLayeredMove_AnimRootMotion>()`, fill `MontageState.{Montage,PlayRate,StartingMontagePosition,CurrentPosition,BlendOutTimeSeconds,bEnableAutoBlendOut}` + `DurationMs` + `MixMode`, **call `CancelFeaturesWithTag(Mover.AnimRootMotion.Montage,false)` yourself** (QueueLayeredMove does NOT auto-dedupe → moves stack otherwise), then `QueueLayeredMove`. `FMoverAnimMontageState` lives in `MontageStateProvider.h:17` (NOT MoverDataModelTypes).
- **No global enable/disable RM** on the component. RM is opt-in per queued move. The only "disable" is `MontageInstance->PushDisableRootMotion()` (stops anim applying RM so the layered move owns it).

## RM + Falling combo — "everything except vertical is RM, gravity owns vertical" (= the hero hybrid-jump doctrine, NATIVE)
1. `FallingMode` ctor adds tag `Mover.SkipVerticalAnimRootMotion` (`FallingMode.cpp:32`; also AsyncFallingMode).
2. State machine sees that tag on an `OverrideAll` RM move (with `Mover.AnimRootMotion`) → **downgrades MixMode to `OverrideAllExceptVerticalVelocity`** (`MovementModeStateMachine.cpp:327-332`), which re-enables FallingMode's GenerateMove.
3. `MovementMixer::MixProposedMoves` merges per-axis: **horizontal vel + rotation from RM, vertical vel from gravity** (`MovementMixer.cpp:205-213`).
Full-skip escape (pure air control): the attribute move returns false on jump-pressed / `Mover.SkipAnimRootMotion` (`RootMotionAttributeLayeredMove.cpp:137-141`).

## Motion warping — TWO implementations, backend-split
- **GT path (ours, NPP):** if the actor has a `UMotionWarpingComponent`, Mover auto-creates a `UMotionWarpingMoverAdapter` (`MoverComponent.cpp:286-290`) and the GT move warps through `ConvertLocalRootMotionToWorld` + the live component. **Warped RM IS available to us — on the game thread.** Needs: UMotionWarpingComponent on the actor + a montage with MotionWarping notify windows carrying a `WarpTargetName`; register targets at runtime via `UMotionWarpingComponent::AddOrUpdateWarpTarget*`.
- **Async path (NOT ours):** `FMoverResolvedWarpTarget`/`FMoverMotionWarpingInputs` (`AnimRootMotionWarpingTypes.h`) snapshotted to a blackboard, skew-warp re-implemented on the worker thread, warp targets replicated through the input command. Written only by ChaosMover's state machine. **ChaosMover-only.** (A SimDriven move accidentally run under NPP silently falls back to the inherited GT `GenerateMove` — still warps, via the adapter.)

## Backends — AZ runs NetworkPrediction (decisive)
`IsBackendAsync()` = `BackendLiaisonComp->IsAsync()` (`MoverComponent.h:687`); defaults false; NPP + Standalone liaisons return false. **ChaosMover IS installed AND enabled in `AZ.uproject`, BUT both AZ pawns (hero + infected) create `UNetworkPredictionComponent` → they run the NetworkPrediction backend → `IsBackendAsync()` is FALSE.** Consequences for us:
- We use the GT `FLayeredMove_AnimRootMotion` (polls live AnimInstance, self-ends on montage stop) + GT motion warping via the adapter.
- The entire async sim-driven montage delivery (`FMoverSimDrivenMontageData`, `FlushMontageStates`, `ApplyMontageEntry` async branches, sub-frame `Interpolate`) is **dormant**.
- Sim-proxy RM replication runs through `FMoverAnimMontageState` + `UCharacterMoverComponent::UpdateSyncedMontageState` with a **±10% play-rate nudge** to correct drift (`CharacterMoverComponent.cpp:255-277`).

## RM CVars (all `#if !UE_BUILD_SHIPPING`)
`mover.debug.LogRootMotionAttrSteps`, `mover.debug.DisableRootMotionAttributes` (A/B the attribute consumer), `mover.debug.RootMotionAttributesExcessiveSpeedSq` (warn-only, no clamp), `mover.debug.LogAnimRootMotionSteps` (montage move). Plus `mover.SkipGenerateMoveIfOverridden` (the mode-skip optimization).

## ⚠ The OverrideAll trap (the #1 footgun — source now confirms the mechanism)
`FLayeredMove_RootMotionAttribute` is `MixMode=OverrideAll` + `DurationMs=-1`, and contributes WHENEVER the `RootMotionDelta` attribute is present — and `RootMotionMode=RootMotionFromEverything` makes the AnimInstance write that attribute EVERY frame for EVERY clip (≈zero delta on in-place loops). Net: a permanently-queued attribute move OverrideAll-zeroes capsule velocity during velocity-driven loops → press W from idle → walking builds velocity → move overrides to ~0 → never crosses IdleSpeedThreshold → **idle deadlock**. Fix: gate with the `Mover.SkipAnimRootMotion` tag during idle/loco loops (velocity drives), clear it during transition clips (RM drives); or queue finite-duration only for the transition window. See [[project_root_motion_mode]] for the full diagnostic recipe.

## When would the async/ChaosMover path be BETTER? (decision guide)
NPP (ours) is the proven, simpler, SP-first path and RM-via-montage works fully on it. Switch a pawn to the ChaosMover async backend ONLY when:
- **Characters must be first-class physics citizens** — pushed by explosions/impulses, interacting with Chaos rigid bodies/destruction, physics-blended ragdoll/hit-reactions, riding genuinely physics-simulated platforms/vehicles. (Most likely future trigger for CHALK: physics hit-reactions / ragdoll on the combat + obstacle-reaction systems.)
- **You commit the whole netcode to Chaos networked physics** and want the sim to be the single rollback-reconciled source of truth (sim-authoritative, worker-thread-deterministic RM + warping).
- **Large crowds** where moving the movement sim off the game thread is a measured win (caveat: anim TICK is usually the real horde bottleneck, not movement sim — see [[project_npc_foundation]] Option B).
Costs: even more experimental; anim-pose-to-motion latency (README); some Mover features break (e.g. teleport effect); per-pawn backend swap; abandons the NPP validation surface (v2 stack, Iris MP, crouch patch). **Verdict for CHALK: stay NPP** until physics-character interactions or a Chaos-physics netcode pivot force it.
