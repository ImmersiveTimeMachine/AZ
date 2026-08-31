---
name: feedback_mover_visual_component_two_writers
description: "★★ Mover OWNS the mesh's relative transform: UMoverComponent::FinalizeFrame snaps the primary visual component back to BaseVisualComponentTransform every sim tick, and the crouch stance modifier re-bases it (-92 -> -57). Any game code that eases the mesh toward a stored constant fights it every frame -> whole-body bob + feet in the floor while crouched (2026-08-31). Also: start-of-frame samples cannot see an intra-frame writer; sample at OnEndFrame."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-31T03:37:49.219Z
---

# The crouch-idle "whole body twitch" (2026-08-31) — two writers on one transform

**Symptom:** crouched hero (MetaHuman AND SurvivalMan) bobs vertically every frame, feet sink ~4 cm
into the floor; standing is clean; standalone SkeletalMeshActor with the same clip is clean; AA,
shadows, AO, motion blur, F8 eject, slomo, LOD/URO, GAS/Mover crouch flags, camera rig, playhead —
all clean. Every START-of-frame sample (NativeUpdateAnimation) was static to 0.01 cm.

**What it was:** `AAZ_PawnMoverHeroCharacter::UpdateGrabMeshAnchor` (Tick) eased `Mesh` relative Z
toward `DefaultMeshRelativeZ` (−92, captured in BeginPlay). The engine crouch (`FStanceModifier::
AdjustCapsule` → `SetBaseVisualComponentTransform`) re-bases the mesh to −57, and
`UMoverComponent::FinalizeFrame` (`MoverComponent.cpp:387-391`) snaps the mesh back to that base every
sim tick. Per frame: Mover sets −57 → our Tick pulls 35 cm × (1−e^(−8·dt)) ≈ 4.2 cm down → render →
Mover restores. dt jitter (16–22 ms) = ±0.6 cm bob. Measured with an `FCoreDelegates::OnEndFrame`
sampler: relZ −61.2 ± 0.6 at end of frame vs −57.0 at start, 394 direction flips / 2,363 frames.

**VERIFIED 2026-08-31 03:40 (one PIE after the fix):** `[v2 CrouchEnd] relZ = -57.000` on all 866 crouched
frames (was -61.2 ± 0.6), user: "twitch gone", feet on the floor. Ctrl while walking:
`WalkFwdLoop -> Crouch_WalkFwd_new cost +0.05` within a frame, release -> `-> WalkFwdLoop`, press again
`-> Crouch_WalkFwd_new +0.03`; user: "ctrl works". 0 snaps / 0 fallbacks in the run.

**Rules:**
1. **Mover owns the primary visual component's relative transform.** Never ease/set the mesh's
   relative location from game code against a stored constant; the rest value is
   `MoverComponent->GetBaseVisualComponentTransform()` (changes on crouch/prone). Fix applied 03:37:
   `RestZ` read from Mover each tick; the grab lift is measured from and returns to that.
2. **A start-of-frame sample proves nothing about what is rendered.** Two writers in one frame (A sets,
   B restores before the next update) are invisible to any sample taken at the same point each frame.
   When transforms look static but the picture moves: sample at `FCoreDelegates::OnEndFrame` (after
   Mover/physics/anim, before render) and diff against the start sample of the SAME frame. That single
   instrument found in one PIE what nine pipeline tests could not.
3. Numbers that identify this class of bug: a constant offset equal to `Δbase × speed × dt` with dt-
   proportional jitter, `simZ`/actor static, `relZ` ≠ base only at end of frame.
4. NP smoothing was NOT involved: `bEnableFixedTickSmoothing` is false by default (project ini does not
   set it), so `UMoverComponent::FinalizeSmoothingFrame` never runs on this project. The
   `BP_CMC_Hero_C_0` spamming "tick prerequisite … would form a cycle" every frame lives in
   `/Engine/Transient.World_0` — the BP_CMC_Hero editor tab's preview actor, not the level.

**Sibling fix, same session:** orientation-mode crouch while walking never switched clips (513 crouched
frames rendering the standing walk loop): the loop search pool for `LocomotionLoop && Crouching` now
comes from context (`StrafeCrouchDatabase`) like the strafe branch, instead of trusting the chooser row
(which stayed on the standing row) and its BranchIn-less raw clip (R14).

Related: [[feedback_measure_the_clip_first]] (the clip was NOT the cause — the reversal metric flags
lively idles too; the standalone-actor test is the discriminator), [[project_crouch_system]],
[[feedback_posesearch_mm_mechanism_rules]], [[project_mover_metahuman_2026-08-31]].
