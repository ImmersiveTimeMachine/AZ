# OffsetRootBone in `AZ_ABP_MoverHero_MHC` — REVISED verdict (supersedes the "option 2 port" plan)

Status: **REVISED 2026-09-07 after the step-1 build broke rotation.** The original plan below §6 is retained for the record
but is **withdrawn**. Read §1-§5 first.

---

## 1. What we found (chronological, with evidence)

| # | Finding | Evidence |
|---|---|---|
| 1 | Step 1 (five getters returning the recorded literals, bindings restored by hand) **stopped the character turning**. | user report "no rotation", PIE 00:23-00:28 |
| 2 | `Accumulate` rotation makes the root **counter** capsule rotation — "the root will stay in place". | engine doc on the enum, `AnimationWarpingTypes.h`; `ShouldCounterComponentDelta(Accumulate)=false`, `AnimNode_OffsetRootBone.cpp:52-66` |
| 3 | GASP can afford `Accumulate` because Steering feeds rotation back. **This graph has no Steering, no warping, no AimOffset, no foot IK** — nothing restores the rotation the offset cancels. | `search_nodes` on the MHC ABP: 0 matches for each |
| 4 | **The node is not in the committed asset.** `git show 229f9b9:…/AZ_ABP_MoverHero_MHC.uasset` contains no `AnimGraphNode_OffsetRootBone`; the working copy does. It was pasted after the last checkpoint — the same day as this investigation. | blob grep: `229f9b9 OFR=0 (400528 B)`, worktree `OFR=1 (442363 B)`; `c913da3` did not yet track the file |
| 5 | Therefore **every PIE-verified behaviour in this project — RM-driven stops/starts/turns, paired strikes, grab alignment, crouch, exploration sprint — was validated with no OffsetRootBone in the graph.** | follows from #4 and the commit history |
| 6 | The node does **not** double-drive the capsule: it writes the *original* root-motion delta back to the attribute stream (pass-through), so `FLayeredMove_RootMotionAttribute` still receives full RM. | `AnimNode_OffsetRootBone.cpp:516-522` (`OverrideRootMotion(RootMotionTransformDelta, …)`, the computed "remaining" delta is unused) |
| 7 | With **in-place loops** (ours carry ~zero authored RM, `AZ_MoverAnimInstance.cpp:132-138`) the node's simulated root never advances during locomotion, so `Interpolate` translation builds a trailing offset every frame and bleeds it at the halflife — bounded by `MaxTranslationError` (30). A running mesh would sit behind its capsule. Not measured; stated as mechanism. | `ShouldCounterComponentDelta(Interpolate)=false`; offset = `Component − Simulated`, damped, `AnimNode_OffsetRootBone.cpp:318-344` |
| 8 | GASP's `bClampToTranslationVelocity ← IsMoving` limits catch-up to `TranslationSpeedRatio × animated-RM delta`. With in-place loops that delta is ~0, so enabling it here would **freeze** catch-up while moving. The old plan's step 5 would have been actively harmful. | `AnimNode_OffsetRootBone.cpp:326-336` |
| 9 | `AZ_PawnMovementMode_Walking.h:76` ("letting OffsetRootBone-Accumulate hide the residual") predates the v2 graph — it describes the v1 `UAZ_AnimInstance` ABP, which had OFR *and* Steering. It is stale for the Mover hero and was wrongly quoted as proof that OFR is load-bearing. | #4/#5; v1 chain per `gasp_animbp_full_audit` / `az-cpp-utility-tools` |

## 2. Retractions (things the earlier plan and discussion asserted that were wrong)

- "The node is already there, running on GASP defaults, and load-bearing for stops." — **Wrong.** It arrived tonight; nothing depended on it.
- "Step 1 returns today's literals, so it is a no-op." — **Wrong.** The literals were read before the compiler stripped the bindings; the effective value was never verified, and the build changed behaviour.
- "The semantics are settled because two ports agree." — **Misapplied.** Both ports target graphs with Steering/RM loops. Agreement between them says nothing about a graph without those.
- "OFR and the RM bridge double-consume root motion." — **Wrong**, see #6. Withdrawn.
- "`Accumulate` is what `AZ_PawnMovementMode_Walking.h:76` relies on." — **Stale comment**, see #9.
- Calling `AZ_ABP_HeroPawn` "the working reference" — its bindings resolve, but the asset has 9 compile errors (signature drift + `CharacterTrajectory` rename). Reference for wiring shape only.

## 3. Verdict

**Do not port GASP's OffsetRootBone behaviour into this graph, and do not keep the node in its current form.**

The Mover architecture already keeps capsule and animation in agreement by construction: transitions are RM-driven
through `FLayeredMove_RootMotionAttribute`, loops are velocity-driven with in-place clips, and facing springs toward
`OrientationIntent`. OffsetRootBone exists to absorb *disagreement* between an animation-driven root and a sim-driven
capsule — a problem this design chose not to have. In this graph the node has one demonstrated effect (cancelled
rotation) and one predicted effect (a trailing mesh during loops), and no consumer that needs its output.

## 4. Immediate action (editor only, no build)

Either of these fully neutralises tonight's regression:

- **Delete the node** and reconnect `DeadBlending → PoseHistory`. Restores the committed graph shape exactly.
- **Or set both `Translation Mode` and `Rotation Mode` to `Release`** (remove those two bindings first). `Release` neither
  extracts root motion nor lets the simulated root lag the component (`ShouldExtractRootMotion=false`,
  `ShouldCounterComponentDelta=true`), and it bleeds out any existing offset — steady state is identity. Keeps the node
  as a placeholder if you want to revisit.

Then compile, save, and confirm turning is back and stops/pivots match the pre-paste feel.

## 5. Code state after the revision (built 2026-09-07, editor closed, asset untouched)

The node and its five bindings are still in the working-copy asset (the user made no editor change), so the C++ side
was used to neutralise it: `Get_OffsetRootTranslationMode` and `Get_OffsetRootRotationMode` now both return
**`Release`** — engine-level identity (no RM extraction, follows the component, bleeds any offset; §1 #2, #6). This
restores turning without an asset edit. `Get_OffsetRootTranslationHalfLife` / `…Radius` (0.2 / 30) and `IsMoving`
(hard-coded `false`, not a movement helper) remain but are inert under `Release`.

**Why not revert the getters instead:** with the node still in the asset, reverting them re-strips the bindings on the
next compile and drops the rotation pin back to its pasted `Accumulate` literal — the bug returns.

**Remaining cleanup, in order:** (1) delete the node in the editor (or leave it — it is now identity), (2) then remove
the five functions + two UPROPERTYs from `UAZ_MoverAnimInstance` in a closed-editor build. The stale
`AZ_PawnMovementMode_Walking.h` comment (§1 #9) has been corrected in the same build.

## 6. If OffsetRootBone is ever wanted here — preconditions, not steps

It becomes useful only when capsule and animation are allowed to disagree on purpose. That means at least one of:

- **Rotation source in the graph** (Steering or equivalent) before `Accumulate` rotation can be enabled.
- **RM-authored loops** (or a translation-mode rule that returns `Release` during loops and `Interpolate` only during
  RM-driven transitions — the inverse of GASP's rule, because our loop/transition split is the inverse of GASP's).
- A measurement first: log `GetOffsetRootTransform()` magnitude per SM state (loop at run speed, stop, montage) before
  choosing modes. The v1 class already has the readout to port (`AZ_AnimInstance.cpp:1020-1035`).
- The thread rule still stands: any driver that reads node state or actor deltas runs on the game thread / post-event
  graph, never in the thread-safe update (`feedback_animbp_post_event_vs_thread_safe`).
- Never bind `bClampToTranslationVelocity` while loops are in-place (#8).

---

## Appendix — the withdrawn "option 2" plan (for the record)

The original staged port (five getters → montage Release → air Release → idle rule → IsMoving clamp → teleport reset)
was written on the premise that the node was a long-standing, load-bearing part of the graph. It was not. Steps 2-6 are
withdrawn; step 1 was executed and is the subject of §5.
