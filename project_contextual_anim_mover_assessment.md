---
name: project_contextual_anim_mover_assessment
description: "★ Assessment 2026-08-05: a Mover-native Contextual-Anim runtime IS feasible — CAS's CMC coupling is quarantined in one replaceable class; reuse the scene ASSET + full editor, write a thin AZ runtime. Defer until first execution/finisher feature. Coupling map + Option-2 design + failure axes inside."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-06T02:32:07.508Z
---

# Contextual Animation plugin × Mover — feasibility assessment (2026-08-05)

User asked: can we analyze CAS and build our own Mover-compatible contextual-anim component / steal ideas?
**Verdict: YES, cheaply — but defer until the first real consumer (executions / stealth catch / grab-death).**

## Coupling map (verified against engine source, UE 5.8)
Plugin: `Engine/Plugins/Experimental/Animation/ContextualAnimation` (Experimental v0.1, off by default).
Runtime module 5,347 lines, editor module ~4,952 lines. ALL ACharacter/CMC coupling lives in:
- `ContextualAnimSceneActorComponent.cpp` (1,705 ln — the runtime player): warp-target setup does
  `Cast<ACharacter>(GetOwner())` at :208 and **silently returns null-Character** ⇒ no warp on Mover pawns;
  movement freeze/restore/net-correction all via `GetCharacterMovementComponent()`.
- 3 trivial satellites: cached-CMC getter in `ContextualAnimTypes` (:303), one debug cast in
  `ContextualAnimUtilities` (:231), 43-line `AnimNotifyState_EarlyOutContextualAnimWindow`.

**Movement-agnostic and reusable AS-IS (~3,500 ln data/math + entire editor):** `UContextualAnimSceneAsset`
(roles, sections, anim sets, warp points, IK target defs), `ContextualAnimSelectionCriterion` (Cone/Distance/
TransformInRange + Blueprintable base), alignment sampling in Types/Utilities, `AnimNotifyState_IKWindow`.
**Full query/alignment API is exported** (`UE_API` on GetAnimTrack, 5× GetAlignmentTransform, selection-criteria
queries, Query) ⇒ an AZ-module runtime can call everything WITHOUT engine patches.
**The ~5,000-line editor** (Sequencer-style timeline, multi-actor preview scene, warp-point ed-mode, NewAnimSet
dialog) operates on the ASSET, not the component ⇒ keep asset = keep editor for free.

## Key finding — CAS does NOT per-frame sync actors
`OnTickPose` hook is only a montage-end watcher (fires OnMontageReachedEndDelegate for bEnableAutoBlendOut=false
montages). CAS relies on same-frame starts + same-length authoring + warp. Our `MontageSync_Follow` grab is
STRONGER on drift than CAS. Our current shared-origin NAAT grab loses nothing by not using CAS.

## Option 2 design (recommended when needed): reuse asset+editor, write thin AZ runtime
1. Enable plugin; depend on runtime module for DATA types only.
2. AZ runtime = GA-centric (rail doctrine), not a fork of the 1,705-line component. Estimated 300-500 ln because
   our stack already owns the heavy parts: playback+replication = GAS montage tasks via ASC; alignment = register
   warp targets DIRECTLY on UMotionWarpingComponent (skip the ACharacter cast — proven on our pawns via melee
   through FLayeredMove_RootMotionAttribute + DriveRootMotion); movement freeze = tag gates in ProduceInput;
   entry selection = asset's exported criteria Query; contact = IKWindow notify → existing GrabIK targets.
3. Skip: late-join/replication paths (~40% of the component; SP-first rule), scene "manager", EarlyOut notify
   (redo in 20 ln with a Mover check).

## Failure axes (AAA rule)
- Experimental v0.1 asset format may churn on engine upgrades — scene assets are authoring-time content; keep
  count low; source build lets us patch.
- **Warp alignment needs root-motion-ON entry clips.** NAAT pack is RM-OFF shared-origin ⇒ current grab family
  gets ZERO from this route; new execution content must be authored/retargeted WITH root motion, else alignment
  falls back to the layered-move close-in we already have.
- Two experimental systems stacked (CAS assets × Mover) — mitigated by owning the runtime slice.
- MinimalAPI classes: anything NOT UE_API-exported that we later need = engine patch to maintain
  ([[project_local_plugin_patches]] pattern).

## First real consumers (why defer, and until when)
- Grab TakeDown→GroundMunch sections — shelved "for a real death sequence" ([[project_grab_grapple_design]]).
- Stealth catch from behind — `AM_Zombie_Atk_Start_1..5` kept on disk unreferenced for exactly this.
- Hero grab-death execution — dovetails with the pending hero GA_Death work.
Multi-entry alignment selection + multi-actor preview authoring is where CAS's asset+editor starts paying;
one-entry paired holds (today's grab) never need it.

Related: [[project_grab_grapple_design]], [[project_motion_warping]], [[project_root_motion_mode]].
