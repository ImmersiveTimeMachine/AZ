---
name: project-throwable-resume-2026-09-17
description: "★★★ THROWABLE RESUME POINT: start by reading the review response + slot splice decision; both contain approved decisions and corrections to my earlier claims"
metadata: 
  node_type: memory
  type: project
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-17T05:45:25.956Z
---

Committed and pushed as `eb3f951` on `spike/cmc-backport` (287 files). **Start next session by reading, in
this order:**

1. `docs/design-briefs/throwable-open-questions-review-response.md` — answers every open question.
2. `docs/design-briefs/claude-throwable-slot-splice-decision.md` — the approved graph splice, exact pins.
3. `docs/design-briefs/claude-throwable-completion-work-order.md` — the standing work order.

## Approved specification (do NOT re-ask)

Standing aim/release = **exclusive + FullBody + movement locked**. Crouch = **masked upper-body mix**, stance
**captured and locked at activation**, stance toggles consumed (not buffered) during the action. Run **cancels**
the aim — observed before GAS dispatch, else deadlock, since a blocked Sprint can never activate to trigger it.
Equipped carry stays **movable**. Grenade is **Equippable**. Art is **Codex-owned**; preserve
`Brightness=1.5` and `PulseWidthPixels=7.5`.

## The splice (decided, not mine to choose)

Live path is `… AdditiveLeans → RifleFireBase cache → masked RifleFire → AdiativePoses cache → final aim blend
[A82E98F4] → FullBody [5BF55C28] → DeadBlending → OffsetRootBone → feet → paired hands → PoseHistory → Output`.

Replace **only** the `A82E98F4 → 5BF55C28` edge: new SaveCachedPose `PreThrowable` from `A82E98F4.Pose`, feeding
both `A30C1E6B.Source` (existing Throwable slot) and `62E42C8E.BasePose` (existing mask), then
`A30C1E6B.Pose → 62E42C8E.BlendPoses_0` and `62E42C8E.Pose → 5BF55C28.Source`. Use **depth 1**, not the depth 4
I set — live RifleFire is depth 1, and depth 4 is a gradual ramp, not a four-bone cutoff. Compile/save via the
project's native workflow **after** the Python call returns; never compile an AnimBP inside Python.

## Corrections to things I asserted — do not repeat them

- **1.65× character scale was an over-claim** from one wrist-to-finger bone. Do **not** resize props or
  collision. Live grenade radius is 4.5 even though the native default is 6 — read the asset, not the header.
- **Marker tilt was wrong and is already superseded.** My 55° tilt buried the lower edge 21.28 cm below ground.
  Current fix = surface-aligned plane + bounded 4× projected-height compensation. Do not reintroduce tilt.
- **My graph order was not an execution trace**; `RifleFireBase` is *before* `RifleFire`, and the alpha-driven
  aim mask is connected (weight 0 is a default, not "unused").
- **Hand-span arithmetic was wrong** — §5.1's coordinates give ~84.5/44.7/52.9 cm, not 85/32/37.
- **Stale placed pickups: CLOSED.** All seven loaded `L_001` grenades read Equippable with
  `Item.Type.Equippable.Throwable`. No migration needed.
- `ThrowLoop`'s 85 cm span proves it is unsuitable as a *relaxed carry* pose — it does **not** prove it is a bad
  authored wound-up hold. Keep carry and held-aim as distinct presentations.

## Real unfinished work

- **Movement lock:** zero voluntary `WorldMove` in `ProduceInput` **and** carry an action lock bit in
  `FAZ_MoverCustomInputs`, consumed read-only in the sim. Zeroing input alone only brakes. `ThrowPreparing`
  currently affects aim-facing only, not movement.
- **Root motion:** `GA_Throw` does **not** queue Mover's root-motion bridge; a root-motion checkbox is not
  evidence the capsule moves. Inspect raw root deltas before adding a driver.
- **~37.3 cm preview/live origin gap** — log calibrated anchor, live grip, declared lift and final origin in the
  same frame at the actual cue. Not excused by "the preview is an estimate".
- **Floating grenade:** classify before fixing (held prop / flying / stopped / recovered pickup). Likely the
  recovery pickup spawned at surface+12 cm with no physics. My "missing reattachment" guess is unestablished.
- Exclusivity via `BlockAbilitiesWithTag` on **ability asset tags** — must not block death, damage reactions or
  grabs, and must not cancel the active crouch owner.
- Still unbuilt: grenade fuse/detonation, knife, weapon-context profiles, HUD widget consumer.

Related: [[project-throwable-art-assets]], [[feedback-verify-never-presume]].
