# Rifle P01 retarget rebuild — UE4 Manny → SurvivalMan (→ MetaHuman) — PLAN

**Date:** 2026-09-08 · **Status:** design, awaiting go · **Owner:** animation

Goal: the most exact possible transfer of the 886 `Rifle_01` clips (UE4 Mannequin) onto the hero.
"Exact" here means: **bone rotations identical to the source in world orientation**; positions differ only by
limb-length ratio (unavoidable — the hero's arms are 79–85 % of the source).

---

## 0. Facts this plan rests on (all measured 2026-09-08)

**Skeleton family.** `UE5 Manny ≡ SKEL_SurvivalMan` — **0.00° rest-orientation difference on every bone**.
`metahuman_base_skel` matches SurvivalMan on feet/head (0.0°) and arms (2–3°) but the **hand is 21.2° off**.
`UE4_Mannequin_Skeleton` is the outlier: spine `01..03` (target has `01..05`), neck `01` (target `01, 02`),
hand rest 18.6° / lowerarm 19.4° / foot 8.7° from SurvivalMan.

**Who animates the hero.** `AZ_ABP_MoverHero_MHC` targets **`SKEL_SurvivalMan`**; the MetaHuman body plays it via
`metahuman_base_skel.compatible_skeletons = [SKEL_SurvivalMan]`. Every PSD / chooser / clip is SurvivalMan.
⇒ **The retarget target is `SKEL_SurvivalMan`.** SurvivalMan *is* the "proxy skeleton" — no new one is needed.
Retargeting straight onto `metahuman_base_skel` would orphan the whole animation pipeline.

**The existing retargeter `RTG_RifleAnimsetPro_SurvivalMan` — why it deviates:**
1. **No retarget-pose alignment at all.** Both sides use "Default Pose" with **zero offsets on every bone.** The
   18.6° hand / 19.4° lowerarm / 8.7° foot rest differences are therefore carried into every frame. This is the
   dominant, fully fixable cause.
2. **`Run IK Rig` op is enabled** — an IK solve on top of FK is by definition not the source pose.
3. Source rig `IK_UE4_Mannequin_MCO1` is bound to **SurvivalMan** (not UE4 Manny); it resolves by bone name
   only. Non-standard chain spans on both rigs (legs `thigh→ball`, `Head = neck_01→head` merged).
4. Unmapped target chains (8 × twist_02, 8 × metacarpal) sit in retarget pose — minor.

**Rifle-specific data that must survive.**
- The pack **animates the IK bones**: `ik_hand_gun` / `ik_hand_l` move 51–66 cm in `W2_Stand_Aim_Holster`
  (≈0.4 cm in idles). Both skeletons carry all seven (`ik_hand_gun/root/l/r`, `ik_foot_root/l/r`) — they must
  be chains, and their **translation retarget mode decides where the weapon target lands** on shorter arms.
- UE4 source has **`hand_l_wep` / `hand_r_wep`** — weapon attach bones with **no counterpart on SurvivalMan**.
  Their offset from `hand_r/l` must be captured and baked into the `AAZ_Weapon` sockets, or the weapon sits wrong
  even when the hands are perfect.
- Curves `speed`, `contact_l/r`, `rot_yaw_` drive play-rate and the foot latch; a retarget that drops them breaks
  those silently.

**Epic's official rigs** (`IK_UE4_Mannequin_Retarget` 22 chains, `IK_UE5_Mannequin_Retarget` 30 chains) have the
correct, canonical chain topology (legs end at `foot` + separate Toe; Neck and head separate) **but contain no
IK-bone or twist chains** — insufficient alone for a rifle pack.

## 1. Design

```
Rifle_01 (UE4 Manny)  --RTG_RifleP01_UE4_to_SurvivalMan-->  SKEL_SurvivalMan  --compatible skeleton-->  MetaHuman body
                        one-time, lossy only in spine/neck count             (existing pipeline, untouched)
```

### 1a. Source rig `IK_RifleP01_Src_UE4Manny` — skeleton `UE4_Mannequin_Skeleton`
Duplicate Epic's `IK_UE4_Mannequin_Retarget` (keeps its retarget root `pelvis`, leg/arm goals, finger chains), then
**add** 1-bone chains with these exact names: `HandGunIK, HandRootIK, LeftHandIK, RightHandIK, FootRootIK,
LeftFootIK, RightFootIK` and the eight `*Twist01` chains. Preview mesh = `/Game/Rifle_01/Character/Mesh/SK_Mannequin`.

### 1b. Target rig `IK_RifleP01_Tgt_SurvivalMan` — skeleton `SKEL_SurvivalMan`
Duplicate Epic's `IK_UE5_Mannequin_Retarget` (fits exactly: SurvivalMan ≡ UE5 Manny), add the **same-named** IK-bone
and twist chains, plus `*Twist02` / `*Metacarpal` (will simply be unmapped). Preview mesh `SKM_SurvivalMan_Mesh1`.

Chain correspondence after auto-map: everything 1:1 except `Spine 01→03 ↔ 01→05` and `Neck 01→01 ↔ 01→02`
(inherent; Interpolated mode).

### 1c. Retargeter `RTG_RifleP01_UE4_to_SurvivalMan`
- **Retarget pose = the fix.** Create a target pose whose per-bone rotation offsets make every SurvivalMan bone's
  *world* orientation equal the UE4 bone's world orientation in the reference pose. Computed numerically from the
  two ref poses (both already readable), applied with `set_rotation_offset_for_retarget_pose_bone`. Prediction:
  hand_r rest difference **18.6° → < 0.5°** before any animation is retargeted.
- **Ops:** Pelvis Motion (height-ratio scaling), **FK Chains**, Root Motion, Remap Curves.
  **No** Run IK Rig, **no** Stride Warping, **no** Speed Planting, **no** Pole Vector, **no** Stretch.
- **FK chain settings:** rotation mode `OneToOne` on every 1:1 chain; `Interpolated` only on Spine/Neck. IK-bone
  chains: translation mode `GloballyScaled` so weapon targets scale with the 0.79–0.85 arm ratio; test both
  modes on the holster clip (65 cm of `ik_hand_l` travel) and keep the one where `ik_hand_gun` − `hand_r` matches
  the source ratio.
- Source mesh override `SK_Mannequin` (Rifle_01); target mesh `SKM_SurvivalMan_Mesh1`.

### 1d. Weapon attach offset
Measure `hand_r_wep` / `hand_l_wep` relative to `hand_r` / `hand_l` in the UE4 ref pose; compare with the
`AAZ_Weapon` `RelaxedSocket` / `AimSocket` offsets on `SKM_SurvivalMan_Mesh1`; reconcile.

### 1e. Batch + post
`IKRetargetBatchOperation` on a **3-clip pilot** first (`NW_Stand_Relaxed_Rifle_Idle`, `W2_Walk_Aim_F_Loop_IPC`,
`W2_Stand_Aim_Holster`), then the set the project actually uses. Then, per memory: duplicate outputs to bake root
motion; re-run the `speed`-curve root reconstruction for IPC loops (R17); user saves (PSD-indexed clip rule);
PSDs reindex.

### 1f. Out of scope here, separate ticket
The **SurvivalMan → MetaHuman 21° hand twist** is *not* fixed by any of this — compatible-skeleton playback copies
local rotation verbatim and never compensates rest orientation. Fix on the MHC body's bind pose, or a Modify Bone
on `hand_l/r` in `AZ_ABP_MoverHero_MHC`, or a retargeted (not compatible) MetaHuman output.

## 2. Measurement contract (falsifiable, before/after)
Per pilot clip, at 12 frames, in component space:
- `hand_r`, `hand_l`, `head`, `foot_r` **world orientation** vs source → **before:** ≈ rest deltas (18.6°, 19.4°…);
  **after:** < 1.0°.
- `ik_hand_gun − hand_r` vector vs source, scaled by arm ratio → < 1 cm.
- Curve set on output ⊇ `{speed, contact_l, contact_r, rot_yaw_}`.
- Root motion on `rm_*` clips non-zero after the duplicate step.

## 3. Failure axes
1. **Pose-offset space.** Offsets are authored in the retarget pose's local/parent space; my alignment is computed
   in world orientation → must convert per bone (parent-first) and verify by reading back the effective pose.
2. **IK-bone translation mode** wrong → hands right, gun wrong (65 cm test clip decides).
3. **`hand_*_wep` lost** → weapon offset silently different; socket reconciliation is mandatory, not optional.
4. **Spine/neck count** → residual curvature; measure head orientation (expect ≤ 1.3°, the current value).
5. **Curves dropped** → play-rate + foot latch break with no error. Verify names on the pilot.
6. **Editor stability** → the 2026-09-08 crash was a GPU device-removed, not the API, but the editor sits at
   15–22 GB. Script in small steps, save each asset by path, **verify by file mtime**.
7. **Content churn** → 141 clips + PSDs regenerate. Pilot first; two-strike rule on the pilot before batch.
8. **Chain-name drift** → any name differing between the two rigs is silently unmapped (that's how the existing
   twist_02/metacarpals ended up frozen). Assert every source chain maps before batching.

## 4. Steps
0. Baseline measurement on the 3 pilot clips against the *current* SurvivalMan outputs (numbers to beat).
1. Build the two rigs (scripted, saved, re-read to verify chains).
2. Build the retargeter; auto-map; assert full mapping.
3. Compute + apply the aligned retarget pose; verify rest deltas → ~0.
4. Ops/chain settings as §1c; pilot retarget; measure §2.
5. Weapon socket reconciliation (§1d).
6. Batch; root-motion duplicate; curve/root reconstruction; PSD reindex; PIE.

---

## 5. RESULTS (2026-09-08, steps 0–4 executed)

**Assets built** (`/Game/AZ/Blueprints/Animation/Retarget/`, all saved and verified by re-read):
`IK_RifleP01_Src_UE4Manny` (37 chains), `IK_RifleP01_Tgt_SurvivalMan` (53), `IK_RifleP01_Tgt_MetaHuman` (46),
`RTG_RifleP01_UE4_to_SurvivalMan` (FK-only: Pelvis / FK Chains / Root Motion / Remap Curves; target pose
`RifleP01_AlignedToUE4Manny`, 64 offsets), `RTG_SurvivalMan_to_MetaHuman` (plain FK) and
`RTG_SurvivalMan_to_MetaHuman_Aligned` (pose `AlignedToSurvivalMan`, 92 offsets).

**UE4 → SurvivalMan pilot, mean world-orientation error vs source (old pipeline → new):**
hand 11.3→**0.0**, lowerarm 22.1→**0.0**, upperarm 26.9→**0.0**, foot 9–13→**0.0**, head 1.3→**0.0**,
pelvis 3.8→**0.0**; spine_03 2–10 (inherent 3→5 interpolation, does not reach the head). Curves survive.

**SurvivalMan → MetaHuman, three variants** (identical on all pilot clips — it is a rest-pose effect):
plain FK (= compatible-skeleton playback): hand 21.2 / fingers ~18 / pelvis 5.7 / arms 2–3.
FK + Pin Bone on hands: hand **0.0** but fingers **17.9**, everything else unchanged.
FK + aligned pose: **0.0 on every bone.** ⇒ Pin Bones fix one bone, not a propagating rest mismatch.

**Feet (UE4 → SurvivalMan):** `ball_r` floats 1.7 cm (idle) / ~3 cm (crouch) — 5–9 % shorter legs, no planting.
Leg-only `Run IK Rig` and the pelvis ground knobs (`affect_ik_vertical`, `use_ground_falloff`,
`floor_constraint_weight`) changed **nothing** (two strikes; stopped). Options left: constant pelvis Z offset in
the Pelvis Motion op, or runtime foot IK. The retargeter was restored to pure FK afterwards.

**Consequences found:**
- The `ik_hand_*` bones are consumed by nothing in the project (C++ 0, hero ABP 0, PSS 0) — their 8 cm
  gun-vector error is irrelevant.
- `RightHandRifleSocketRelaxed` / `…Aim` / `Hand_LeftSocket` were tuned against the old 11°-wrong hand; with
  an exact hand the rifle sits differently → visual re-tune against the AZ M16 mesh.
- Batch must be an **in-place bone-track transfer** into the existing `Riffle_P_*` assets (key counts match:
  99/99, 37/37) to keep loop flags, notifies, curves, PSD/chooser membership; skip `root` on IPC loops that carry
  the reconstructed root. 20 `AS_P01_Jump_*` clips regenerate from `Tools/rifle_p01_jump_setup.py`.

**API facts (verified in engine source / live objects):**
- Retarget pose offset is applied as `LocalRef * Delta` (post-multiplied, bone-local); globals rebuilt
  parent-first (`IKRetargetProcessor.cpp:77`). Alignment: `delta = inv(local_ref) · inv(parent_aligned_world) ·
  source_world`, walked in hierarchy order using the target's REAL parent map.
- `set_retarget_chain_settings` writes `ChainSettings_DEPRECATED` — the 5.8 op processor ignores it. Set chain
  modes via `rc.get_op_controller(i).get_settings()/set_settings()` (`IKRetargetFKChainsController`, field
  `chains_to_retarget`; the IK op's field is `chains`).
- `add_retarget_op` wants the full struct path, e.g. `/Script/IKRig.IKRetargetRunIKRigOp`.
- `add_default_ops()` leaves the Root Motion op's root bone at `pelvis` — set it to `root` on both sides.
- `IKRigController.set_skeletal_mesh(mesh)` rebinds a duplicated rig to another skeleton.
- Pin Bone pairs via `IKRetargetPinBoneController.set_bone_pair(from, to)`; modes `COPY_GLOBAL_ROTATION` /
  `MAINTAIN_OFFSET_FROM_BONE_TO_COPY_FROM`.
- Reading deprecated properties raises in this Python bridge — `warnings.simplefilter('ignore')`.

## 6. SurvivalMan → MetaHuman: the visible hand, and how to consume the aligned retargeter

Pure-geometry test (finger direction, across-palm, palm normal — independent of bone axes) vs the
SurvivalMan source, frame 0 of `W2_Stand_Aim_Idle_IPC`:

| variant | finger dir | across-palm | palm normal | hand bone |
|---|---|---|---|---|
| compatible-skeleton playback (**today's game**) | 1.4 | **24.2** | 23.2 | 21.2 |
| V1 retarget, default poses | 2.1 | 24.4 | 23.9 | 21.2 |
| V2 + Pin Bone hands | 2.2 | **25.9** | 25.3 | 0.0 |
| **V3 + aligned pose** | 7.2 | **3.9** | **8.3** | 0.0 |

The MetaHuman rest hand is *visually* rolled 24° vs SurvivalMan (not an axis convention) — that roll is
the twisted rifle. Pinning the wrist leaves the palm at 25.9° (finger bases carry their own rest delta).
The aligned pose brings the palm to 4–8°; with SurvivalMan's socket values the rifle socket is 0.0°.
The MetaHuman body's `RightHandRifleSocketAim/Relaxed` were hand-tuned to compensate the 24° roll
(`Relaxed` pitch −21.8 vs −2.0) — **revert them to SurvivalMan's values when adopting the aligned path,
never independently.** `Hand_LeftSocket` / `BackRifleSocket` are identical on both meshes.

**Path A (recommended) = GASP's `RetargetedCharacters` pattern.** Reference: `ABP_GenericRetarget` —
AnimGraph is one `RetargetPoseFromMesh` node → Root, `IKRetargeterAsset` and `CustomRetargetProfile` bound
(thread-safe) to BP variables, pose sourced from the **attached parent** mesh. For AZ: the SurvivalMan
mesh becomes the hidden driver (runs `AZ_ABP_MoverHero_MHC` + Mover writes), the MetaHuman body a child
component running the generic graph with `RTG_SurvivalMan_to_MetaHuman_Aligned`; garments keep following
the body; **weapon/grab sockets must attach to the visible body, not the driver** (`GetThirdPersonMesh()`
today returns `GetMesh()` = the single `Mesh`). Cost: one extra skeletal evaluation per frame; reopens the
2026-08-31 LeaderPose/compatible decision. Modify-Bone (path C) is not equivalent — the V2 row shows why.
