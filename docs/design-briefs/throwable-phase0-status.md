# CHALK throwable — Phase 0 content / input / collision audit

Completed 2026-09-16 against the live editor (PID 33988) and current source. No gameplay code, assets or config
were modified in Phase 0; no build, PIE or tests. This is the evidence gate the implementation plan requires
before Phase 1 ([throwable-system-implementation-plan.md](throwable-system-implementation-plan.md) §12).

Everything below was re-measured rather than carried over from the 2026-09-15 planning audits. Where a planning
claim was confirmed it is marked **confirmed**; where this pass found something the audits did not, it is marked
**new**.

> **Reviewed and corrected** by [throwable-phase0-review.md](throwable-phase0-review.md), 2026-09-16. Four of its
> findings are folded in below and marked **[review]**: release times are per-clip *candidates* rather than proven
> instants; the finger claim was over-stated; the preview must select a provisional arc while aiming and only
> *lock* it at commit; and a bare channel trace does not prove collision parity, because collision is bilateral.
> Both of its answers to §5 are recorded there.

---

## 1. Animation content

### 1.1 What is actually on the hero skeleton

152 assets match throw/grenade/knife. **Exactly 12 reference the MetaHuman skeleton**
`/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel`, and all 12 are the pistol
grenade family (6 in-place + 6 root-motion `…1` variants). **Confirmed** — no additional retargets have
appeared since the planning audit.

| Phase | Exact-MH asset (`/Game/AZ/Assets/Pistol/`) | Length |
|---|---|---:|
| Start | `AZ_Pistol_Grenade_Throw_Start` | 0.766667 s |
| Loop | `AZ_Pistol_Grenade_Throw_Loop` | 1.233333 s |
| Close | `AZ_Pistol_Grenade_Throw_Close` | 1.500000 s |
| Far | `AZ_Pistol_Grenade_Throw_Far` | 1.500000 s |
| Cancel | `AZ_Pistol_Grenade_Throw_Cancel` | 1.066667 s |
| Single | `AZ_Pistol_Grenade_Throw_Single` | 1.800000 s |

Avoid the `…1` suffix variants: they enable root motion, and `UAZ_MoverAnimInstance` runs
RootMotionFromEverything, so one would contaminate a live locomotion root-motion consumer.

### 1.2 Content gaps — named exactly, as the brief requires

| Family | Needed for | Status |
|---|---|---|
| MH rifle grenade throw | Phase 2 weapon context | **Absent.** Source exists at `/Game/RifleAnimsetPro/Animations/InPlace/Rifle_Grenade_Throw_*` on `/Game/RifleAnimsetPro/UE4_Mannequin/Mesh/UE4_Mannequin_Skeleton`. |
| MH unarmed throw (right-handed) | Phase 1 stone, per plan | **Absent.** Source at `/Game/MovementAnimsetPro/Animations/InPlace/Throw_*` on that pack's own `UE4_Mannequin_Skeleton`. |
| MH knife throw | Phase 3 | **Absent.** Single source candidate `/Game/FightingAnimsetPro/Animations/InPlace/KB_KnifeThrow`, 1.716667 s, on that pack's `UE4_Mannequin_Skeleton`. No MH knife hold/aim-loop set exists at all. |

**New — retargeting is feasible from an existing project template.** `RTG_RifleP01_UE4_to_MetaHuman`
(`/Game/AZ/Blueprints/Animation/Retarget/`) already targets the real hero body
`SKM_MHC_Hero_BodyMesh` via `IK_RifleP01_Tgt_MetaHuman`. Its **source** rig is built on
`/Game/Rifle_01/Character/Mesh/SK_Mannequin`, which is a different asset from the three packs' skeletons, so
the target half is reusable as-is and only a source IK rig per pack skeleton is missing. Follow
`feedback_ik_retargeter_exact_transfer` for the aligned-pose/FK-only recipe.

### 1.3 Measured release cues — **new**

The audits explicitly refused to guess these. Two facts make measurement necessary and constrain the method:

- **No release notify exists on any of the six clips.** Confirmed.
- **New: the obvious grip metric is flat.** Thumb↔middle fingertip distance is pinned at **9.37 cm** from
  early Start through the entire throw and recovery, so *that* metric yields no hand-opening cue at any
  sampling rate. **[review]** This does not prove every finger transform is static — only that this measure
  offers nothing. Confirming the stronger claim would mean checking the individual finger tracks, which is
  not needed for the generic systems work.

Release is therefore derived kinematically from `hand_l` in component space, at the point where the vertical
hand velocity crosses zero (arc apex or nadir) **and** horizontal hand speed peaks — the moment the arm stops
adding speed to the object. Both criteria coincide to within one sample in both clips, which is what makes
the number defensible rather than fitted.

**[review] These are per-clip CANDIDATES, not proven release instants and not defaults for any other family.**
A hand-height extremum coinciding with peak horizontal speed identifies a useful candidate; it does not prove
the moment the object leaves the hand. Each needs confirmation against the real held prop, the finalized pose
and the first launched frame, and the unarmed/rifle/knife families must be measured separately rather than
inheriting these. Sampling: 0.02 s coarse scan, then a 0.005 s fine pass over the peak region.

| Clip | **Release (candidate)** | hand_l at release (x,y,z) | speed at release | shape |
|---|---:|---|---:|---|
| `…_Close` | **0.280 s** | (26.8, 41.0, **52.9**) | 921 cm/s | low sweep, nadir of arc — an underhand lob |
| `…_Far` | **0.385 s** | (31.8, 61.3, **145.2**) | 1452 cm/s | apex of arc — an overhand hurl |

**Consequence that matters for the preview contract:** the two release *heights* differ by **92.3 cm**. Close
and Far are not a speed swap of one motion; they are different throws with different launch origins. The
Close/Far choice must therefore be made at commit and fed into the preview anchor, otherwise the displayed
arc is calibrated to the wrong hand position. This is direct evidence for the plan's per-clip anchor
requirement (§9) and against ever previewing from the Loop hand pose.

These are still *kinematic* release points. They want one visual confirmation from a playthrough before being
treated as final; nothing else in the content can corroborate them.

### 1.4 Seams — **confirmed and tightened**

Sampled `hand_l`, component space:

| Seam | Delta |
|---|---:|
| Start end → Loop start | 0.0062 cm |
| Start end → Close start | 0.0089 cm |
| Start end → Far start | 0.0089 cm |
| Start end → Cancel start | 0.0089 cm |
| Loop end → Loop start (wrap) | 0.0121 cm |

All four continuations begin at the identical pose. The ready seam is **Start end = 0.766667 s**, exact. A
queued early RMB can branch Start → Close/Far there with no Loop cycle and no invented delay. Branching
*earlier* than that inside Start has no supporting evidence and should not be added.

### 1.5 Recovery — **new, and it contradicts a plan assumption**

The plan allows for an early "hand-back" boundary to avoid a long tail. Measured, the MH pistol family does
not offer one:

| Clip | Hand settles | Usable tail |
|---|---|---|
| `…_Close` | drifts to ≈23 cm/s by 1.18 s, fully quiet from **1.38 s** | ~0.12 s |
| `…_Far` | **never settles** — still moving 25–90 cm/s at the 1.5 s end | none |
| `…_Cancel` | quiet from **0.92 s** of 1.067 s | ~0.15 s |

So Close/Far play to their authored end with sensible blending, and **no fixed hand-back timer is invented**.
**[review]** This does not contradict the plan's conditional early-hand-back — that was always "only where
measured content supports it", and here it does not. Equally, no "wait until the hand is still" gate should be
added, no second recovery appended, and no universal tail cut copied across clips: hand motion need not reach
zero before a legal blend and control handoff, and Mover walking and look stay responsive underneath.

---

## 2. Input routing

### 2.0 Control scheme — CORRECTED BY THE USER 2026-09-16

The execution brief specified click-LMB-to-aim / click-RMB-to-throw with "releasing either button does
nothing". **The user replaced that scheme after Phase 0 was measured.** The authoritative controls are:

| Input | Action |
|---|---|
| **RMB press, hold** | Enter preparation: Start → Loop, trajectory preview, held while the button is down |
| **RMB release** | Commit the throw |
| **LMB** (while preparing/aiming) | Cancel; spends nothing |

This inverts two rules written elsewhere in the plan and in §2 below — "do not use button release as the
throw trigger" and "exclude throw from Held retries" no longer describe the intended product.

**The safe mechanism is not the Held path.** The throw ability activates once on the RMB **Started** edge and
then stays *active* for as long as the button is held, catching the release with
`UAbilityTask_WaitInputRelease`. That is exactly how `UAZ_GA_PawnJump` already implements hold-to-jump-higher
(`AZ_GA_PawnJump.cpp:103`). Because the ASC's held loop only re-activates specs that are **inactive**
(`AZ_AbilitySystemComponent.cpp:172`), an ability that remains active while held cannot be retriggered by
held frames — so release-as-trigger is safe here, and throw still never joins the Held retry list.

Consequences of the new scheme:

- `bQuickSelectMouseReleasePending` (`AZ_PlayerController.cpp:718`) becomes load-bearing rather than a
  nicety: the RMB *release* that finishes choosing an item in QuickSelect must not also throw it.
- A quick RMB tap releases during Start. That is a valid throw: it latches one commit and executes at the
  measured ready seam (0.766667 s, §1.4), reusing the early-commit branch the plan already specifies.
- Hold duration must **not** affect throw power. Holding aims; it does not charge. Launch speed stays
  data-driven and constant, per plan §9.
- LMB is `Input.Action.PrimaryAttack`. While throw context owns input it is consumed as Cancel and must not
  reach fire or melee.



**Confirmed against the live IMC** `/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IMC_RT_PawnInputs`:

| Key | Input Action(s) |
|---|---|
| LeftMouseButton | `AZ_IA_RT_PrimaryAttack` |
| RightMouseButton | `AZ_IA_RT_SecondaryAttack` **and** `AZ_IA_RT_Aim` — two mappings on one key |
| MiddleMouseButton | `AZ_IA_RT_Lethal` — **new**, pre-existing and unused by this plan |

So one RMB press produces **two** semantic actions and a throwable context must consume both or it will enter
firearm aim while committing the throw.

Routing insertion point is `AAZ_PlayerController::AbilityInputTagPressed`
(`Source/AZ/Private/Player/AZ_PlayerController.cpp:542`), after the `IsInventoryInputCaptured()` /
`MenuSuppressedInputTags` handling at :544–549 and **before** `Asc->AbilityInputTagPressed(InputTag)` at :592
and every firearm/melee branch below it.

Two existing precedents to follow rather than reinvent:

- **Fresh-press activation:** the Jump branch at :599 — `Asc->AbilityInputTagHeld(InputTag, false)` on the
  Started edge, because ASC `Pressed` alone never activates an inactive spec.
- **Held exclusion:** :656–660 already excludes Aim, PrimaryAttack, Reload, Crouch and Jump from
  `AbilityInputTagHeld`. Throw must join that list; a held button must never retry a throw.

`bQuickSelectMouseReleasePending` (:718) already exists to stop the RMB that *chose* an item from also acting
on it — reuse it, do not add a second gate.

---

## 3. Collision

**New — the situation is worse than the planning audit recorded.** `Source/AZ/AZ.h` carries **three**
overlapping macro sets over the same five game trace channels:

| Macro | Channel | Config name for that channel | Agreement |
|---|---|---|---|
| `ECC_SkeletalMesh` (:17) | Ch1 | Ability | ✗ |
| `ECC_HitBox` (:18) | Ch2 | Projectile | ✗ |
| `ECC_Projectile` (:24) | Ch1 | **Ability** | ✗ **off by one** |
| `ECC_Target` (:25) | Ch2 | Projectile | ✗ |
| `COLLISION_ABILITY` … `COLLISION_INTERACTABLE` (:28–32) | Ch1–5 | Ability…Interactable | ✓ **correct set** |

Config (`Config/DefaultEngine.ini:222–226`): Ch1 Ability, Ch2 Projectile, Ch3 AbilityOverlapProjectile,
Ch4 Pickup, Ch5 Interactable.

The existing `Projectile` **profile** (:219) is `QueryOnly`, **Overlaps Pawn**, ignores Visibility/Camera —
an overlap-style ability projectile. It is unsuitable for a physical thrown object, which must *block* on
bodies so a friendly obstructs the throw even when damage policy excludes them.

**The migration risk the plan warned about is small but the plan's instruction still holds.** Total live uses:
`ECC_Projectile` 1 (`AZ_CharacterBase.cpp:25`, sets the character mesh to *overlap* Ch1),
`ECC_Target` 1, `COLLISION_PICKUP` 1, `COLLISION_INTERACTABLE` 2, and `ECC_SkeletalMesh`, `ECC_HitBox`,
`COLLISION_PROJECTILE`, `COLLISION_ABILITY` **0 each**.

**Decision — additive, no migration:** add a new `ThrowableProjectile` profile with object type `Projectile`
(Ch2), `QueryOnly`, blocking WorldStatic / WorldDynamic / Pawn / PhysicsBody / Destructible and ignoring
Camera, Visibility, Ability, Pickup and its own channel. Nothing existing is renamed, repointed or removed;
`ECC_Projectile` and `AZ_CharacterBase.cpp` are left alone.

Preview/runtime agreement: the runtime sphere is an *object* of type Projectile, so
`PredictProjectilePath` must use a **channel trace on `ECC_GameTraceChannel2`** (`bTraceWithChannel`), which
asks exactly "what blocks the Projectile channel" and therefore returns the same blockers the moving sphere
will hit. World geometry defaults to Block on Ch2 and the stock `Pawn` profile blocks it, so friendly bodies
obstruct the preview as required.

---

## 4. Decisions carried into Phase 1

0. **Controls are RMB-hold-to-aim / RMB-release-to-throw / LMB-cancel** (§2.0), superseding the brief.
1. Presentation is data-driven, so the stone slice's code does not depend on which family ships first. Build
   against the **verified MH pistol family** and treat the unarmed right-hand family as a presentation swap.
   Taken as the default so the slice is playable this session; reversible by profile data.
2. Release cue = the measured per-clip value (§1.3), stored in the presentation profile as data. No notify is
   added to source-pack assets; project-owned montages carry the cue.
3. **[review — corrected]** Close/Far is selected **provisionally while aiming**, with hysteresis, and the
   displayed arc uses *that* candidate's calibrated release transform from the first frame of preview. RMB
   release **locks** the displayed candidate and the accepted aim intent; it does not defer the choice until
   then. An early release during Start stays one pending intent, with the preview still updating until the
   ready seam commits it, unless LMB cancels first. "Choose once at commit" meant *lock* once. The original
   wording here would have previewed the wrong throw whenever the choice changed — with a 92.3 cm error in
   release height, that is highly visible.
   The anchor is stored as a full transform in **mesh component space**, composed through Mesh-to-World and
   then the grip offset; a component-space XYZ is not an actor-space origin.
4. Recovery plays to the authored end; no fixed hand-back timer (§1.5).
5. **[review — corrected]** Collision as §3, but a bare channel trace does **not** prove parity. Unreal
   resolves a contact from *both* sides, and `PredictProjectilePath`'s own sweep carries no per-projectile
   response container — so an Ability object that blocks Ch2 would stop the prediction while the real
   throwable, which ignores Ability, sails through. The solver therefore takes ballistic samples with native
   tracing **off** and sweeps each segment itself using the profile's real channel *and* its response
   container, which is also what the launch-clearance check uses. Vehicle, Destructible and PhysicsBody are
   now stated explicitly in the profile rather than left to the default.

## 5. Questions — both answered by the review

- **Unarmed stone presentation.** → **Retarget the unarmed right-hand family before the Phase 1 stone slice
  ships.** The MH pistol family may carry temporary internal pipeline work but must not become the shipped
  unarmed stance merely because it exists. Inventory, input and projectile foundation proceed in parallel
  while the retarget is prepared. Reuse the existing retargeter as a *template*, verifying each source rig's
  mesh, retarget pose, chain mapping and target body — and **re-measure the resulting unarmed grip and release
  rather than copying the pistol cue times**.
- **MiddleMouseButton → `AZ_IA_RT_Lethal`.** → **Leave it unchanged.** Out of scope; an existing action is not
  a reason to alter the agreed controls.

---

# Phase 1 — code slice (2026-09-16)

All C++ for the stone slice is written. **Not yet compiled**: the editor was open with a Live Coding session,
and the slice adds new reflected types, so it needs a full CLI build with the editor closed — a Live Coding
patch would compile without ever registering `UAZ_ThrowPreviewComponent`, `FAZ_ThrowPreviewStyle`,
`FAZ_Inv_CommonUI_ThrowableFragment` or the new tag. UHT already ran clean over the new types
(`UHT processed AZEditor ... 10 generated files written`) before UBT refused the link.

## Files

**New**
- `Throwables/AZ_ThrowableTypes.h` — shared contract (phase, arc, impact behaviour, solution, preview result).
- `Throwables/AZ_ThrowPresentationProfile.h` — animation family + measured cues + calibrated release anchors.
- `Throwables/AZ_ThrowableDefinition.h` — per-item behaviour data asset.
- `Throwables/AZ_ThrowLaunchSolver.{h,cpp}` — the ONE producer of a launch solution, plus bilateral-contract
  flight prediction.
- `Throwables/AZ_ThrowableProjectile.{h,cpp}` — authoritative flight, impact hearing, pickup recovery.
- `Throwables/AZ_ThrowPreviewComponent.{h,cpp}` — owner-only pooled ribbon + four-corner marker (Quiet Sage).
- `AbilitySystem/Abilities/AZ_GA_Throw.{h,cpp}` — the one throw action.

**Changed**
- `Config/DefaultEngine.ini` — additive `ThrowableProjectile` profile (nothing renamed or repointed).
- `AZ_GameplayTags.{h,cpp}` — `Ability.State.ThrowPreparing`, `Event.Throw.Release`.
- `AZ_Inv_CommonUI_InventoryComponent.{h,cpp}` — the throw reservation/commit transaction.
- `AZ_Inv_CommonUI_ItemFragment.h` — `FAZ_Inv_CommonUI_ThrowableFragment`.
- `AZ_Inv_CommonUI_InventoryItem.{h,cpp}` — `IsThrowable()`, keyed on the fragment, not on a category.
- `AZ_QuickBarComponent.{h,cpp}` — `IsReadyable()`; binding and readiness now accept throwables.
- `AZ_AbilitySystemComponent.{h,cpp}` — `SendAbilityInputEdge()`, input by class rather than by tag.
- `AZ_PlayerController.{h,cpp}` — `RouteThrowInput()` in front of ordinary ASC dispatch.

## Controls, as implemented

Hold RMB to aim (both physical routes, `Input.Action.SecondaryAttack` and `Input.Action.Aim`, are consumed
while a throw owns the mouse), release RMB to throw, click LMB to cancel. A cancel disarms the pending
release first, so lifting the still-held RMB afterwards throws nothing and does not restart — a fresh press
is required.

## Two defects found and fixed while writing this

1. **`ACharacter` is the wrong base.** The AZ hero is `AAZ_PawnMoverHeroCharacter : APawn` — a Mover pawn with
   no `GetCapsuleComponent()`. The solver's original `const ACharacter*` signature would have cast to null on
   every activation and the action would have silently never run. It now takes `const APawn*` plus an explicit
   mesh (the hero also carries LeaderPose garment meshes, so `FindComponentByClass` would pick an arbitrary
   one), and resolves the capsule from the root component the way the firearm clearance check does.
2. **An ended montage task still cancels the ability.** `UAbilityTask::ShouldBroadcastAbilityTaskDelegates()`
   asks only whether the *ability* is active, not whether the *task* has finished, and the task's `OnDestroy`
   unbinds its montage callbacks only when it stops the montage itself. Starting the Loop interrupts Start, so
   the ended Start task would have fired `OnInterrupted` and cancelled the throw on its own seam. Delegates
   are now cleared before every transition (`DetachPresentation`). Relatedly, every presentation montage now
   uses `bStopWhenAbilityEnds=true`: nothing else stops a *looping* aim montage, so an action that ended
   without it would leave the character winding up forever.

## Still owed for Phase 1

- Full CLI build with the editor closed, then restart.
- Retarget the unarmed right-hand throw family (`/Game/MovementAnimsetPro/Animations/InPlace/Throw_*`) and
  **re-measure** its grip and release cues — do not carry the pistol numbers over.
- Project-owned montages from that family, with `Event.Throw.Release` notifies at the measured times.
- A `UAZ_ThrowableDefinition` + `UAZ_ThrowPresentationProfile` + stone item with the throwable fragment.
- Preview art: arc segment mesh/material and one corner mesh/material for the marker.

---

# Phase 1 — content (2026-09-16)

## Animation

Retargeted `SKEL_SurvivalMan` → `metahuman_base_skel` through `RTG_SurvivalMan_to_MetaHuman_Aligned` (whose
target preview mesh IS `SKM_MHC_Hero_BodyMesh`), into **fresh names** — the 5.8 batch op crashes in
`NotifyUserOfResults` only when it overwrites names that already exist.

`/Game/AZ/Assets/Throwables/Anims/AZ_RTG_MH_{Throw_Start, ThrowLoop, ThrowEndClose, ThrowEndFar, ThrowCancel}`

**Measured on these clips, not carried over from the pistol family:**

| | release time | release anchor (mesh space) | shape |
|---|---|---|---|
| Close | **0.490 s** | (-17.55, 31.69, **37.31**) | underhand lob past the hip, peak hand speed 760 cm/s |
| Far | **0.700 s** | (-7.16, 65.21, **143.51**) | overhand above the head, peak hand speed 910 cm/s |

Method: peak horizontal speed of the throwing hand coinciding with the zero-crossing of its vertical
velocity. `hand_r` is the throwing hand (`hand_l` peaks at 198/562 cm/s against 760/910). The two release
heights are **106 cm apart** — larger than the pistol family's 92 cm, and the reason the arc must be selected
live while aiming rather than at commit.

**Ready seam = 0.900 s**, Start's own end. Verified exact: Start's last frame matches Loop, Close, Far and
Cancel frame 0 to within **0.009 cm across all 342 bones**. Loop end → Loop start is 0.018 cm, and Loop end →
Far start 0.010 cm, so a branch is exact at every clip boundary; only a mid-loop branch needs BranchBlendTime.

**★ This pack's forward is +Y**, measured — every `RunFwd`/`WalkFwd` root delta in the pool is pure +Y with
zero X. That is exactly what the hero mesh's `SetRelativeRotation(0, -90, 0)` undoes, so composing
`Anchor * Mesh->GetComponentTransform()` is what turns the anchor into the actor's forward. Read raw, the
release velocity looks 80° off the aim.

**★ FullBody slot, which roots the player while aiming.** The clips swing the pelvis ~176° and reposition the
feet; on an upper-body slot that rotation floats on standing legs. The plan's header asserted "must NOT be
FullBody" — the content contradicts it, so the assertion was corrected rather than the data bent. Switching
back is one script (rebuild the montages on another slot); the authored footwork is the price.

## Assets created

| Asset | Notes |
|---|---|
| `AM_AZ_Throw_{Start,Loop,Close,Far,Cancel}` | `FullBody`; Loop self-links; `Event.Throw.Release` verified at 0.4900 / 0.7000 |
| `DA_ThrowProfile_Unarmed` | all measured cues above, `GripBone = hand_r` |
| `DA_Throwable_Grenade` | `SM_M67_Preview` (already real 8.5×9.0×7.1 cm, so `HeldMeshSize = 0`), `CollisionRadius 4.5`, Close 900 / Far 1600, **BounceAndSettle** — fuse and detonation are Phase 2 |
| `M_AZ_ThrowPreview` | unlit translucent, `Color` / `Intensity` / `Opacity` |
| `BP_AZ_GA_Throw` | Quiet Sage style: arc `#B5C8B7` 2.5 cm ribbon, marker `#EEEAE0`, four 10 cm corner ticks at radius 18 |
| `BP_Pickup_Grenade` | Consumable, `Item.Type.Consumable.Throwable`, grenade icon, stack 3, ThrowableFragment → the definition |
| `AZ_BP_PawnMoverHero_MHC` | `BP_AZ_GA_Throw_C` appended to `StartupAbilities` |

## Controls — confirmed against the actual bindings

`AZ_IMC_RT_PawnInputs` binds **RMB to BOTH** `AZ_IA_RT_SecondaryAttack` *and* `AZ_IA_RT_Aim`, and **LMB** to
`AZ_IA_RT_PrimaryAttack`. That duplication is exactly why input is dispatched **by ability class**
(`SendAbilityInputEdge`) instead of by input tag: a tag dispatch would press the spec twice on one click and
release it twice on one lift. `AZ_IA_RT_Lethal` (G / MMB) was left untouched.

## Still owed

- Compile the three Blueprints — script wrote their CDO/SCS templates, and compiling from Python is what
  kills this editor (`feedback_python_gc_crash`), so `UAZ_Inv_AuthoringUtils` deliberately does not.
- A `BP_Pickup_Grenade` placed in a level, then a play test.
- Phase 2 (grenade fuse/detonation + weapon-context families), Phase 3 (knife), Phase 4 (polish).
