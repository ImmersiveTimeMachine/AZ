---
name: feedback-slot-mask-weight-squared
description: "A LayeredBoneBlend gated by a montage's own weight SQUARES the blend when its base pose and the Slot node's source are the same pose — use a 0/1 gate, not the weight."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-21T01:33:39.047Z
---

When a LayeredBoneBlend splices a montage Slot node in, and the LBB's **Base Pose** and the Slot node's
**source** are the same pose (in CHALK: the `PreThrowable` cached pose feeds both, in `AZ_ABP_MoverHero_MHC`),
driving the LBB weight from the montage's own weight `w` renders `lerp(Base, Clip, w*w)`. The Slot node has
**already** applied `w`. Every authored blend curve in that slot is silently squared: the pose barely moves
through most of the transition and then snaps home at the end of it.

Symptom as reported (2026-09-20): "дергается в конце" on taking a grenade out and on putting it away, while
the log showed correct, single-frame sequencing and the right montages playing.

**Why:** `lerp(Base, lerp(Base, Clip, w), a)` with `a = w` is a double application. With `a = 1` it is exactly
`lerp(Base, Clip, w)` — the Slot keeps owning the crossfade, which is where the authored timing belongs. With
no montage the Slot passes its source through unchanged, so blending it over an identical base is a no-op and
opening/shutting the gate cannot pop by itself.

**How to apply:** make the mask a **gate** (`0` or `1`), open while any montage instance owns that slot.
Test instance lifetime, **not `IsPlaying()`** — `FAnimMontageInstance::Stop()` clears `bPlaying` immediately
and leaves the instance fading for the rest of its blend, so `IsPlaying()` slams the mask shut mid-fade and
cuts the clip off. `MontageInstances` drops the instance once the blend is over, and its weight is zero by
then. Still never `GetSlotMontageGlobalWeight` — that already includes the graph weight and feeds back to a
latch at zero (see the comments in `UAZ_MoverAnimInstance::NativeUpdateAnimation`).

This applies only to a mask whose two inputs are the same pose. A genuine **overlay** LBB (a different pose on
the blend input, e.g. the fists-up or aim-pose locks) legitimately takes an eased 0..1 alpha.

Related: [[feedback-posesearch-mm-mechanism-rules]], [[project-rifle-aim-upper-body-lock-2026-09-10]],
[[feedback-livecoding-header-reinstances-animbp]].
