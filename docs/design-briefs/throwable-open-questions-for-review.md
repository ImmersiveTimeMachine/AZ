# CHALK throwables — open questions for external review

**Review completed:** [decisions, corrections and current evidence](C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-open-questions-review-response.md). The questions below are preserved as submitted. The response supersedes outdated graph/marker facts and records the seven loaded L_001 pickup manifests already classified Equippable.

**2026-09-17.** Written for a second AI/engineer who does **not** have this project loaded. Everything below
is either an unresolved design decision or a defect we could not close. Facts are measured, not assumed; where
something is unverified it says so.

---

## 1. Context you need

**Engine/setup.** UE 5.8. The hero is `AAZ_PawnMoverHeroCharacter`, an **`APawn` driven by the Mover plugin**
— *not* an `ACharacter`, so there is no `GetCapsuleComponent()` and no `CharacterMovementComponent`. The
capsule is the pawn's root component. Animation runs through `AZ_ABP_MoverHero_MHC`
(`UAZ_MoverAnimInstance`), whose AnimGraph is a 47-node chain: BlendStack → Pose History → Slot `FullBody`
→ AdditiveLeans → OffsetRootBone → DeadBlending → Inertialization → cached `BS Output` → `AO_Rifle_Aim`
aim offset → LayeredBoneBlend → hand_r/hand_l Modify Bone → cached `RifleFireBase` → Slot `RifleFire`
→ LayeredBoneBlend → aim idles/TwoWayBlend → two Control Rigs (procedural feet, slope warping, foot pinning,
interaction IK).

**Skeleton.** MetaHuman (`metahuman_base_skel`). Measured: `hand_r`→`middle_03_r` = **15.9 cm** where an adult
human is 9–10 cm, and foot-to-head = **153.7 cm**. The hero is roughly **1.65× human scale**. This predates
the throwable work.

**Animation source.** MovementAnimsetPro, retargeted to MetaHuman. **Critical frame fact:** in this pack
*forward is +Y*, measured — every `RunFwd`/`WalkFwd` root delta is pure +Y with zero X. The MetaHuman
reference pose instead spreads arms along ±X. Mixing the two frames is a 90° error (we hit this; see §5.1).

**Existing blend lanes in the AnimGraph** (all pre-existing, we authored none of them):

| Slot | Group | Layered blend filter | Effect |
|---|---|---|---|
| `FullBody` | `DefaultGroup` | — | replaces root, pelvis, legs |
| `RifleFire` | `WeaponFire` | `spine_01`, BlendDepth 4, mesh-space rotation | upper body only |
| — | — | `spine_01`, BlendDepth 1, weight 0 (alpha-driven) | unused by this ABP |

**Verified negative:** `WeaponRelaxedPose` (the C++ "lowered weapon carried while moving" lane) is **never
consumed** by this ABP. Searching all 47 nodes, the only upper-body references are `AimAlpha` ×2 and
`AimStanceAlpha`. Writing that variable animates nothing here. It exists in `UAZ_MoverAnimInstance` and in an
older ABP.

**Decisions already made by the user (do not relitigate):**
- Controls: **hold RMB to aim, release RMB to throw, click LMB to cancel.** No charging or cooking.
- Aiming is **exclusive**: pressing run **cancels** the aim (spending nothing); while aiming, **all** other
  actions are blocked — jump, melee, weapon fire, movement.
- Because aiming is exclusive, aim/release montages play **unmodified on `FullBody`**. Only the *carry* idle
  (walking around with the grenade holstered-in-hand) uses a masked upper-body lane.
- Grenade is an **Equippable**, not a Consumable. Selecting it shows a held idle immediately, without
  reserving or spending a unit.

---

## 2. The question we most want answered: crouch

Standing aim/throw is settled — full-body clips, movement locked. **Crouch is not.**

The throw family (`Throw_Start`, `ThrowLoop`, `ThrowEndClose`, `ThrowEndFar`, `ThrowCancel`) is **standing-only
content**. There is no crouched variant, and the clips contain a substantial pelvis rotation plus foot
replanting, so they are not a plausible source for a crouched throw.

The user's instruction: *"for crouch we will keep this mix"* — i.e. crouch keeps the **masked upper-body
blend** (throw content above `spine_01`, live crouch locomotion below), while standing uses full-body.

**Q2.1** Is a per-stance blend policy — full-body when standing, `spine_01`-masked when crouched, chosen at
the moment the action starts — sound, or does the inconsistency show up as a visible pop when the player
crouches *while already aiming*?

**Q2.2** If the player crouches mid-aim (or stands up mid-aim), what should happen? Candidates: (a) switch
blend policy live, accepting a blend seam; (b) lock the stance for the duration of the action; (c) cancel the
aim. We lean toward (b) but have not tested any.

**Q2.3** Masking the standing clip above `spine_01` for crouch means the ~176° pelvis rotation is discarded
while the arms still assume it happened. Standing, that produced visibly wrong arms (§5.1). Is there a
principled way to keep a full-body clip legible when masked — counter-rotating the retained spine, an
additive formulation relative to the clip's own first frame, or similar — or is the honest answer that crouch
needs authored content?

**Q2.4** Does a crouched throw need to be gameplay-equivalent to a standing one (same speed/arc), or is a
shorter crouched throw acceptable if the content only supports a restricted motion?

---

## 3. Movement lock on a Mover pawn

Aiming must block movement. This project has a hard-won rule, recorded in its own notes:

> Sim decisions (latches, pacing, rate limits) **must ride `FAZ_MoverCustomInputs`**, never movement-mode
> members — the motion-matching trajectory predictor runs `GenerateWalkMove` ~60× per frame and overwrites
> mode state, producing end-of-turn sway.

**Q3.1** Given that, what is the correct way to zero movement for the duration of an action? Zeroing
`WorldMove` in `ProduceInput` when the state tag is present (the melee/grab precedent in this codebase), a
dedicated custom-input flag, or something else?

**Q3.2** The release clips contain **root motion** (the character plants and pivots). If movement input is
zeroed but the montage has root motion, does root motion still apply on a Mover pawn, and is that desirable
here? We want the authored footwork, not a frozen slide.

**Q3.3** Cleanup: if the ability ends abnormally — death, grab, stagger, possession change, PIE stop — how do
we guarantee the lock is released? Precedent in this codebase is that a montage that outlives its owner
leaves state stuck.

---

## 4. Exclusivity

**Q4.1** Blocking "everything" via `ActivationBlockedTags` on every other ability means editing many
abilities. Is a single blocking tag on the throw's `ActivationOwnedTags` plus `BlockAbilitiesWithTag`
sufficient and more maintainable? Is there a failure mode where an already-active ability ignores it?

**Q4.2** Run **cancels** the aim. Sprint here is a GAS ability. Should cancellation be driven by the sprint
ability activating, or by raw input? The subtlety: if the throw blocks sprint, sprint can never activate, so
it can never trigger the cancel — a deadlock. We think input must be observed *before* GAS dispatch, in the
controller, which is where the RMB/LMB routing already lives. Is that right?

**Q4.3** Should cancellation-by-run be instantaneous, or play the authored `Cancel` clip? Instant contradicts
"the body is committed"; playing it delays the sprint the player asked for.

---

## 5. Defects we have not fixed

### 5.1 Masked full-body clips produce wrong arms (root cause understood, no general fix)

Putting `ThrowLoop` on the `spine_01`-masked lane gave a character with **both arms flung straight out to the
sides**. Measured cause:

| Clip | `hand_r` rel. pelvis | `hand_l` rel. pelvis | spread |
|---|---|---|---|
| `ThrowLoop` (wind-up) | (-21.1, -35.3, 4.2) | (-22.1, 49.2, 1.8) | **85 cm** |
| `Throw_Start` frame 0 | (-23.5, 9.1, -8.6) | (21.2, 8.4, -8.7) | 32 cm |
| `RunFwdLoop` (normal) | (-27.9, 13.6, -4.6) | (18.2, 38.2, 3.9) | 37 cm |

`ThrowLoop` is a *mid-throw frame with both arms extended*, not a ready stance. We wasted two attempts
"correcting" the pelvis before measuring the hands. Fixed for carry by sourcing the pose from `Throw_Start`
frame 0 instead. **Not fixed generally** — this is exactly Q2.3.

### 5.2 Preview/actual release origin disagree by ~35 cm

The preview draws from a calibrated anchor measured off the animation; the real throw uses the live socket at
the release cue. Measured gap: `originLocal` (98, 10, 80) actual vs (66, 13, 61) calibrated. Nothing breaks —
the preview is explicitly an estimate and the launch uses the live grip — but the plan says to measure and
tune this out, and we have not. A separate review measured Close 14.89 cm / Far 29.11 cm against the rendered
body, which does not match our number; **the discrepancy between those two measurements is itself unresolved.**

### 5.3 Unexplained floating grenade

One playtest screenshot showed a second grenade hanging in mid-air beside the character. Most likely a thrown
one at rest, but if it is a duplicate *held prop* it would mean the pooled prop is not reattached on a pawn or
definition change. Never reproduced or diagnosed.

### 5.4 Placed pickups may carry stale categorisation

The grenade's category was changed `Consumable` → `Equippable` on the Blueprint default. **Placed actors
serialise their own manifest copy**, so instances already placed in `L_001` may still be `Consumable` with the
old type tag, putting them in the wrong inventory tab. Not yet inspected — no world was loaded. Migration must
preserve GUID and stack count, not delete and recreate.

### 5.5 Grenade reads as too small in hand

Every scale in the chain is 1.0 and the mesh is a true 9.3 cm M67. It looks like a pebble because the hero is
~1.65× human scale (§1). **Q5.1:** scale props to the character (~15 cm, and `CollisionRadius` 4.5 → ~7.5 to
match), or is the character scale itself the bug? Inflating props one at a time hides a systemic issue.

### 5.6 Contact marker foreshortening

The marker is a quad laid on the hit surface, sized in reference pixels converted at camera distance. Flat on
the ground at a grazing angle it projects to a sliver. We added a **bounded tilt toward the viewer** (max 55°)
as a compromise between a surface decal and a billboard. **Q5.2:** is bounded tilt the right approach, or
should this be a true decal, or a screen-space widget with a depth test?

---

## 6. Smaller open questions

**Q6.1 — dedicated slot.** We registered a `Throwable` slot in its own `Throwable` group and added
`Slot` + `LayeredBoneBlend` nodes to the ABP, configured to mirror `RifleFire`, **but left them unconnected**
pending a decision on splice point. Options: (a) after the cached `RifleFireBase`, so a throw layers over the
weapon lane; (b) parallel to `RifleFire` before the cache. Downstream of that cache sit `AO_Rifle_Aim` and
both Control Rigs, so a wrong pin silently breaks procedural feet or the aim offset. Which splice, and does a
separate slot group risk two montages driving the same bones simultaneously?

**Q6.2 — carry while sprinting.** The existing relaxed-pose gate suppresses the upper-body layer during
sprint. Should the carry idle follow that convention (arm drops while sprinting) or persist?

**Q6.3 — arc dead zone.** Two fixed speeds (900/1600 cm/s) made a band of distances unreachable, since range
goes as v² so the far speed lands ~3× further at the same angle. We replaced it with a **continuous blend**
between `CloseArcDistance` (300 cm) and `FarArcDistance` (900 cm), interpolating both speed and a per-arc
origin lift. The project plan forbids "secretly increasing power" and any charge mechanic. Is a declared,
preview-visible, aim-distance→speed mapping consistent with that, or is it an auto-solve by another name?

**Q6.4 — origin lift.** A `CloseOriginLift` (currently 50 cm) raises the launch origin for close throws. A
review called this "a spawn displacement, not arc curvature… do not hide disagreement with arbitrary world-up
lifts", arguing the grip calibration should be corrected instead. The user explicitly asked for the lift as a
tunable. Which is right?

---

## 7. What is working (do not regress)

Inventory 4→3→2→1→0 consumption with matching launches; recovery returning the **same instance GUID** to the
inventory; LMB cancel spending nothing (`phase=6 cancelRequested=1 committed=0`); body facing tracking aim
exactly (`body-aim = -0.0°`, `velYaw == aimYaw`); response-container-aware flight prediction; the pickup
spawning 12 cm clear of the surface (spawning *at* the contact point was silently refused by
`AdjustIfPossibleButDontSpawnIfColliding`, and the item vanished).
