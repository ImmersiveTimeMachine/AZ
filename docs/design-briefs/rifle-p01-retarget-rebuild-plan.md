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

---

## 7. Path A design — SurvivalMan driver + MetaHuman body via runtime retarget (2026-09-08, awaiting go)

### 7.0 Facts this design rests on (all measured today)

**Meshes.** Hero body vs SurvivalMan, own reference pose (spawned `SkeletalMeshActor`, component space):

| | SurvivalMan | `SKM_MHC_Hero_BodyMesh` | delta |
|---|---|---|---|
| head Z | 162.58 | 162.36 | same height |
| spine_05 Z | 141.10 | 135.33 | −5.8 (longer torso, lower shoulders) |
| pelvis Z | 95.90 | 92.33 | −3.6 (shorter legs) |
| ball_r Z | 0.75 | 1.23 | |
| hand_r | (−47.8, 15.7, 104.5) | (−51.1, 17.2, 101.2) | 4.8 cm |
| bones | 101 | 342 | |

⚠ `AnimPoseExtensions.get_reference_pose(metahuman_base_skel)` returns the **female-medium archetype**
(head 143.36) — the skeleton asset is shared by every MetaHuman body. Never use it for hero rest numbers.

**Feet through the aligned retargeter** (idle f0 on the MetaHuman mesh, rest ball 1.23): **2.18 / 2.08**
(+0.9 cm). Today's compatible playback: **4.53 / 3.94** (+3.3 cm float, because SurvivalMan's pelvis height
rides verbatim on 3.6 cm shorter legs). Path A improves the feet; pelvis offset stays 0.

**`RTG_SurvivalMan_to_MetaHuman_Aligned` as saved:** ops = Pelvis (offset 0) · FK 46 chains
`ONE_TO_ONE` / translation `NONE` · **Pin Bone (enabled, 0 pairs — leftover, remove)** · Root Motion ·
Curve Remap; target pose `AlignedToSurvivalMan`.

**Pawn `AZ_BP_PawnMoverHero_MHC`** (SCS dump): `Capsule → Mesh` (C++ subobject; `SKM_MHC_Hero_BodyMesh`,
`AZ_ABP_MoverHero_MHC_C`, rel −92 / yaw −90, NoCollision, overlap events on) → `Face` (`ABP_Face_C`: its
whole graph is one `CopyPoseFromMesh` from the *attached parent*) + 4 grooms, and 6 `Cloth_*` (leader
`Mesh`, no ABP). No MetaHumanComponent, no LODSync. EventGraph and ConstructionScript are **empty**.
`AZ_ABP_MoverHero_MHC`: parent `UAZ_MoverAnimInstance`, target `SKEL_SurvivalMan`, references nothing
MetaHuman → runs natively on the SurvivalMan driver.

**Engine.** `FAnimNode_RetargetPoseFromMesh` defaults to `RetargetFrom = ParentSkeletalMeshComponent`,
reads the parent's component-space transforms in `PreUpdate`, honours a leader-pose source, and adds **no
tick prerequisite** (neither does `CopyPoseFromMesh`). GASP's `BP_UE4_Mannequin` therefore calls
`AddTickPrerequisiteComponent(parent mesh)` itself after `DelayUntilNextTick`. Mover picks the primary
visual component as the **first `UMeshComponent` child of the capsule** (`MoverComponent.cpp` BeginPlay)
and reads root motion from it — a body attached *under* `Mesh` leaves that untouched.

**Hero-mesh consumers in `Source/` today** (grep `GetMesh()` / `GetThirdPersonMesh()` /
`FindComponentByClass<USkeletalMeshComponent>` on the Mover hero):

| consumer | uses the mesh for | under path A |
|---|---|---|
| `AZ_Inv_CommonUI_EquipmentComponent::OnPossessedPawnChange` (`OwningSkeletalMesh = Hero->GetMesh()`) | weapon attach sockets | **visual body** |
| `AZ_GA_PlayerGrabbed` `HeroMesh = Hero->GetMesh()` (anchor layered move + height match) | `GrabbedSocket` | **visual body** |
| `AZ_GA_PlayerGrabbed` ×4 `Hero->GetMesh()->GetAnimInstance()` | montages / anim | driver (unchanged) |
| `AZ_GA_MeleeAttack::GetAvatarMesh` | BOTH: `ResolveAvatarIsMoving` (anim) and strike-socket trajectories | split |
| `AZ_AT_MeleeSweep` `Avatar->FindComponentByClass<USkeletalMeshComponent>()` | fist sockets — already ambiguous with 8 SKMs on the hero | **visual body** |
| `AZ_InfectedAnimInstance` `Prey->FindComponentByClass<USkeletalMeshComponent>()` | Chalkie grab-IK reaching the hero's hands | **visual body** |
| `AAZ_PawnMoverHeroCharacter::UpdateGrabMeshAnchor` `Mesh->GetSocketTransform(GrabOwnSocket)` | measure; then moves `Mesh` | measure on body, move driver |
| `UAZ_MoverAnimInstance` (own component) | grab IK solve, pose history, MM | driver (unchanged) |
| `AZ_Weapon` / `GetThirdPersonMesh` / GA_Aim / GA_Shoot | `AAZ_HeroCharacter` = CMC pawn, not this class | untouched |

`AZ_Inv_CommonUI_EquipmentComponent.cpp/.h` and `AZ_Weapon.cpp/.h` carry **another agent's uncommitted
edits** right now — the one line needed there is a single content-anchored hunk, staged alone.

### 7.1 Layout

```
Capsule                                   (root, Mover UpdatedComponent)
└─ Mesh        SKM_SurvivalMan_Mesh1  AZ_ABP_MoverHero_MHC_C  HIDDEN  AlwaysTickPoseAndRefreshBones
   │           = Mover primary visual component, root motion, PoseSearch, montages, grab IK, GAS anim
   └─ Body     SKM_MHC_Hero_BodyMesh  ABP_GenericRetarget_C (IKRetargeter = RTG_SurvivalMan_to_MetaHuman_Aligned)
      │        identity transform · visible · post-process ABP_Body_PostProcess stays · ALL sockets
      ├─ Face  ABP_Face_C (CopyPoseFromMesh ← attached parent = Body)  → grooms
      └─ Cloth_Bag/Backpack/Belt/Boots/Hoodie/Pants   leader-posed to Body at runtime
```

### 7.2 Ownership — one owner per fact

| fact | owner |
|---|---|
| which component animates / drives Mover | C++ `Mesh` (unchanged) |
| which component is *seen* and carries sockets | `GetVisualMesh()` = `VisualBody ? VisualBody : Mesh`, resolved once in `BeginPlay` from `VisualBodyComponentName` (default `"Body"`, BP-authored child). No child → stock hero, bit-identical |
| driver hidden | **derived** in C++ from "a body exists" — never a second BP flag to get half-right |
| retargeter asset | pawn `UPROPERTY(EditDefaultsOnly) TObjectPtr<UIKRetargeter> BodyRetargeter` (assigned in BP defaults, no `/Game/` in C++), pushed in `BeginPlay` into the body anim instance's `IKRetargeter` variable (the property `ABP_GenericRetarget` binds) by reflection; loud warning if the class has no such property or the asset is null. Child-ABP-with-default rejected: a second asset owning the same fact plus a manual ABP authoring step |
| tick order | explicit, as GASP: `Body->AddTickPrerequisiteComponent(Mesh)`, `Face->AddTickPrerequisiteComponent(Body)` |
| follower wiring | leader = `GetVisualMesh()`; exclusions = `Face` (by name, as today) **and the leader itself** |
| body sockets | SurvivalMan values via `Tools/metahuman_fixup.py` (idempotent) — the hand-tuned Aim/Relaxed offsets die in the same step that makes them wrong |

### 7.3 Changes

**C++ (`AAZ_PawnMoverHeroCharacter`) — new UPROPERTYs ⇒ closed-editor CLI build:**
- `.h`: `FName VisualBodyComponentName = "Body"`, `TObjectPtr<UIKRetargeter> BodyRetargeter`,
  `Transient TObjectPtr<USkeletalMeshComponent> VisualBody`, `UFUNCTION(BlueprintPure) GetVisualMesh()`.
- `.cpp` `BeginPlay`, before the follower wiring: `SetupVisualBody()` — find the named descendant of
  `Mesh`; if found: `Mesh->SetVisibility(false)` (no propagate), body `AlwaysTickPoseAndRefreshBones`,
  the two tick prerequisites, set `IKRetargeter` on `VisualBody->GetAnimInstance()`; one log line
  `[VisualBody] <pawn>: body=<comp>(<mesh>) driver=<mesh>(hidden) retargeter=<asset> face->Body`.
- `WireModularMeshFollowers_Mover(Root=Mesh, Leader=GetVisualMesh())`: walk `Mesh` descendants, skip
  `Face` and the leader, wire the rest → `[MoverMesh] … wired 6 … to Body (1 excluded)`.
- `UpdateGrabMeshAnchor`: measure `GetVisualMesh()->GetSocketTransform(GrabOwnSocket)`, still write `Mesh`.
- Consumers per the table: equipment component (1 line), `GA_PlayerGrabbed` `HeroMesh`, `GA_MeleeAttack`
  split `GetAvatarMesh()` (anim) / `GetAvatarStrikeMesh()` (sockets), `AT_MeleeSweep` and
  `InfectedAnimInstance`: hero → `GetVisualMesh()`, else the existing lookup.

**Content:**
- `AZ_BP_PawnMoverHero_MHC`: `Mesh.SkeletalMeshAsset → SKM_SurvivalMan_Mesh1` (ABP stays); add `Body`
  under `Mesh` (asset, `ABP_GenericRetarget_C`, identity, NoCollision + overlap events like `Mesh`);
  re-parent `Face` and the 6 `Cloth_*` under `Body`; pawn default `BodyRetargeter`. Scripted via
  `SubobjectDataSubsystem` and verified with the same SCS dump; user compiles + saves. Fallback: by hand.
- `RTG_SurvivalMan_to_MetaHuman_Aligned`: remove the empty Pin Bone op.
- `Tools/metahuman_fixup.py` run (body sockets → SurvivalMan values).
- No CHT / ABP / PoseSearch / Mover changes.

### 7.4 Failure axes (with the instrument that decides each)

1. **Body one frame behind the driver** → rifle/hands jitter at speed. Guard = the prerequisite. Instrument:
   OnEndFrame `|Body.hand_r − Mesh.hand_r|` must be a **constant** (the ~4.8 cm proportion offset), never dt-shaped.
2. **Face floats** (still under the driver, or the re-parent lost): head sits on SurvivalMan's neck, 6 cm
   high. The `[VisualBody]` line prints the Face's attach parent; screenshot.
3. **Garments deform**: they are SurvivalMan-skinned and today follow a body *stretched to SurvivalMan
   lengths* (compatible playback copies translations). Path A gives the body its own lengths: same height,
   spine −5.8 / pelvis −3.6 cm → hoodie/belt compress by that much. Expected acceptable; if not, the one
   knob is FK `translation_mode` on the Spine chains (`GLOBALLY_SCALED`), in the RTG only.
4. **Feet**: +0.9 cm idle (measured) vs +3.3 today — an improvement; pelvis offset stays 0 unless PIE says otherwise.
5. **Sockets**: double-compensated if the fixup is not run; wrong if run without path A. Same step, one owner.
6. **Stock hero regression**: `VisualBody` null ⇒ every accessor returns `Mesh`; its PIE log must read
   `[VisualBody] … body=None` and `wired 0`, otherwise identical to today.
7. **Retarget variable not set** (template ABP var renamed, class swapped) ⇒ body frozen in ref pose. Loud
   `[VisualBody] anim class X has no 'IKRetargeter' property` and `retargeter=None` in the line.
8. **Editor preview**: the SCS viewport has no driver tick, so `Body` shows its ref pose there. Cosmetic.
9. **Order**: C++ (closed editor, CLI) → reopen → BP edits → compile → fixup → PIE. A BP compiled against
   the old binary has no `BodyRetargeter` slot; a Live-Coding patch cannot add the UPROPERTYs.
10. **Cost**: +1 skeletal eval (342 bones, FK-only) ≈ 0.2–0.4 ms; the driver is cheaper than the MetaHuman
    body it replaces as ABP host.
11. **Physics / ragdoll**: none on this pawn (NoCollision); anything added later targets the visual body.
12. **Hero-side `FindComponentByClass<USkeletalMeshComponent>`** anywhere else returns an arbitrary one of
    now-8 SKMs — the two call sites found are redirected; a new one is a bug by construction.

### 7.5 Pass lines (named before PIE)

- `[VisualBody] AZ_BP_PawnMoverHero_MHC_C_0: body=Body(SKM_MHC_Hero_BodyMesh) driver=Mesh(SKM_SurvivalMan_Mesh1, hidden) retargeter=RTG_SurvivalMan_to_MetaHuman_Aligned face->Body`
- `[MoverMesh] … wired 6 modular follower mesh(es) to Body (1 excluded)`
- no `[Grab] anchor SKIPPED`, no `[VisualBody]` warnings; stock hero: `body=None` + `wired 0`.
- Rifle at Aim visually straight on the hand (the §6 numbers say socket 0.0°, palm 4–8°).

### 7.6 Steps

1. RTG: drop the empty Pin Bone op, save. Write the C++ (no build while the editor is open).
2. User closes the editor → CLI build → user reopens.
3. SCS edits by script + dump verification → user compiles + saves the pawn BP (CDO default for
   `BodyRetargeter` set before the compile).
4. `Tools/metahuman_fixup.py`; verify Aim/Relaxed socket values equal SurvivalMan's.
5. User PIE (rifle: idle, aim, walk, jump, grab) → logs against §7.5, screenshots.

**STATUS (2026-09-08): PARKED after step 1.** The C++ was written, verified in place and then reverted as
too heavy for the ask ("a small adjustment of the hand"); it is kept as
`scratchpad/path_a_cpp.patch` (373 lines, applies on `4ac0954`) and the pawn-SCS script as
`scratchpad/path_a_pawn_scs.py`. The Pin-op removal stayed (committed). Path C below shipped instead.

---

## 8. Path C — SHIPPED (2026-09-08): two Modify Bone nodes, exact at the bone

**Why it is exact, not approximate.** Under compatible-skeleton playback the MetaHuman copies every
SurvivalMan LOCAL rotation verbatim, so the world error at each bone is a **constant in that bone's own
frame** — it never depends on the pose. Measured across aim idle, relaxed idle, crouch-aim idle and the
jump-air clip (12 samples): the single bone-space correction that puts `hand_r` on SurvivalMan's world
orientation varies by **0.04°** (`hand_l` 0.05°). One constant per hand, done.

| | hand bone | finger dir | across-palm | palm normal |
|---|---|---|---|---|
| today (compatible playback) | 21.2 | 1.1–1.8 | **24.2** | 21.8–23.3 |
| **+ Modify Bone (path C)** | **0.0** | 5.4–8.5 | **3.1** | **2.9–3.3** |
| path A (aligned retarget, §6) | 0.0 | 7.2 | 3.9 | 8.3 |

Path C is as good as path A on the visible palm (the finger-direction 5–8° is the fingers' own rest delta,
present in A as well) at a cost of two nodes.

**The nodes** — `AZ_ABP_MoverHero_MHC` AnimGraph, spliced between `LinkedAnimLayer AdiativeCombatGrabbed`
(the last node) and the Root, with the schema's automatic Local↔Component conversions:

```
Modify Bone  hand_r   RotationMode Additive   RotationSpace BoneSpace   Rotation (P −0.2086, Y −3.1971, R 20.9491)
Modify Bone  hand_l   RotationMode Additive   RotationSpace BoneSpace   Rotation (P −0.2086, Y −3.1971, R 20.9491)
```
(identical for both hands — the skeleton is mirrored). Translation/Scale Ignore, Alpha 1. Authored with
`AZ_AnimGraphNodeUtils.add_anim_graph_node / set_anim_node_property('Node.…') / connect_pose_link`;
the correction = `inv(handLocal_today) · (inv(lowerarmWorld_MH) · handWorld_SM)`, i.e. a right-multiply in
the hand's own frame, which is exactly what `BCS_BoneSpace` + `BMM_Additive` applies
(`InOutBoneSpaceTM *= BoneTM`). Only the MetaHuman pawn uses this ABP; the stock hero is untouched.

**Sockets reverted** to SurvivalMan's values on `SKM_MHC_Hero_BodyMesh` via `Tools/metahuman_fixup.py`
(the hand-tuned offsets compensated the 21° error and would now double-compensate). The tuned values, for
the record: `RightHandRifleSocketAim` (middle_01_r) loc (−0.86, 5.16, −1.14) rot (P 10.53, Y 106.53,
R −9.50); `RightHandRifleSocketRelaxed` (hand_r) loc (−6.54, 3.71, −0.65) rot (P −21.78, Y 84.20, R 15.63).
SurvivalMan's: Aim loc (−2.43, 4.64, −2.21) rot (P 3.63, Y 118.75, R −9.17); Relaxed loc (−6.39, 4.37,
−2.17) rot (P −2.04, Y 92.62, R 0.64). `Hand_LeftSocket` / `BackRifleSocket` were already identical.

**Not changed:** the 2.9° constant on the lowerarms (same mechanism; add two more nodes if it ever shows),
feet (+3.3 cm float under compatible playback — unchanged by this), the retargeter assets (§1–5, still the
exact UE4→SurvivalMan path for the clip batch).

**Trap recorded:** `AnimPoseExtensions.get_reference_pose(metahuman_base_skel)` returns the *female-medium
archetype* ref pose (head 143 cm); the hero mesh's own rest is 162 cm (same as SurvivalMan). Read rest
numbers off the MESH (spawned component), never the shared MetaHuman skeleton asset.

**Superseded the same night** — §9: the user retargeted the whole pack to MetaHuman-native clips; path C's
Modify Bone nodes were unlinked again (they would double-correct native clips) and are orphaned in the graph.

---

## 9. SHIPPED (2026-09-09): the rifle set is MetaHuman-NATIVE (`/Game/AZ/Assets/M16/Riffle_RTG_MH/AZ_RTG_MH_*`)

The user batch-retargeted the entire UE4 pack (919 clips, `metahuman_base_skel`) and asked for the CHT
rifle set to use them. Measured vs the UE4 original they are a default-pose retarget (hand_r 15°, hand_l
9–10°, pelvis 5.7°, left-hand grip 8–10 cm off) — accepted as-is by decision; the exact direct retargeter
`RTG_RifleP01_UE4_to_MetaHuman` (0.0° every bone, feet ±0.5 cm, aligned pose `MH_AlignedToUE4Manny`,
IK-arm op saved DISABLED — 5.8 has no blend-to-source, IK left 3–3.5 cm grip error + 3–4° arms) is
available for a regeneration into the same names whenever wanted.

**What changed (all verified by disk mtime / post-save re-read):**
- `CHT_v2_CharacterAnimations`: 50 rifle refs → `AZ_RTG_MH_*` (`AZ_ChooserUtils.remap_chooser_assets`).
  Still SurvivalMan: the two `Fall_v2` clips and the 10 `AS_P01_Jump_*_Land` composites (no MH source yet).
- 118 clips: `loop`, `enable_root_motion`, `force_root_lock`, `root_motion_root_lock`, `rate_scale` copied
  from the `Riffle_P_` twin. The user's batch had loop=False on idles and rm=False / flat root on loops.
- 6 `PSD_P01_*` (Crouch/Jog/Walk × Aim/Explore): membership swapped — 8 unique MH clips each, via
  `remove_all_pose_search_notifies(old)` + `add_branch_in_notify(new, db, 0, 0)` + save. `PSD_P01_Land`
  untouched (composites).
- `DA_WeaponAnim_P01.play_rate_loop_assets` (48), `AO_Rifle_Aim` (17 samples), `AZ_AM_Rifle_Fire` (segment →
  `AZ_RTG_MH_W2_Stand_Fire_Single`, same 0–0.379 s window — no `_Short` MH asset needed).
- 20 `Aim_Point_*` clips made additive **relative to frame 0 of `AZ_RTG_MH_W2_Stand_Aim_Point_Center`**
  (`AAT_ROTATION_OFFSET_MESH_SPACE`, `ABPT_ANIM_FRAME`, `ref_frame_index 0`).
- 49 IPC loops: root track rebuilt = the old reconstructed root copied per frame (walk F 145.7 cm/1.17 s,
  jog 223.5, crouch 108.4; fwd +Y, right −X), `contact_l/contact_r` copied by sampling. Two were flat in
  the OLD set too and were rebuilt from their mirror: `CrouchWalk_Aim_FL` (from FR, 114.6 cm),
  `Walk_Aim_BR_BkPd` (from BL, 109.9 cm). `W2_Run_R_Loop_IPC` is flat in both and unreferenced.
- Body mesh sockets (`/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh` — the pawn's
  mesh; the retarget-source twin `/Game/MetaHumans/MHC_Hero/Body/…` has NO sockets): the user tuned
  `RightHandRifleSocketAim` on `middle_01_r` (loc −1.86, 5.12, −1.77 / rot P 11.44, Y 134.07, R −1.18) and
  previews every clip attached there; `RightHandRifleSocketRelaxed` was set IDENTICAL to it (same bone,
  same transform) so the game matches the preview in both states. Two derivations were tried and rejected
  by the user's eye: "aim placement re-expressed in the hand_r frame" (−6.63/4.61/−1.86, P −3.73 Y 102.63
  R 18.36 — 40° off the grip line: the Aim socket hangs off a FINGER whose curl differs 27° between aim and
  relaxed) and a "swing about hand_r so the M16's `LeftHandGrip` lands on hand_l" (−5.54/6.15/−0.30, P 0.49
  Y 89.01 R 6.86 — 2.3 cm at the wrist BONE, still visibly off the palm). Lesson: the M16's `LeftHandGrip`
  socket vs the `hand_l` wrist bone is not what the eye judges; the user's tuned socket is the reference.
  `Tools/metahuman_fixup.py` now PRESERVES both rifle sockets (tuned per skeleton).

**Traps hit (each cost a PIE or a crash):**
1. *Additive base type.* Copying `additive_anim_type` + `ref_pose_seq` without `ref_pose_type` leaves the
   engine default `ABPT_REF_POSE`, drops `ref_pose_seq` on PostEditChange, and bakes the aim offset against
   the A-pose → the whole upper body flung around in-game while the raw clip previews fine.
2. *PSD membership is BranchIn-synced.* `PreSaveRoot` rebuilds the entry list from the BranchIn notifies;
   ALSO calling `add_sequences_to_database` double-adds (16 members). Never both — notify + save only
   (already written in `Tools/rifle_p01_setup.py`; ignored once, paid for it).
3. *Struct arrays from Python are copies.* Mutating `BlendSample`/`AnimSegment` elements of
   `get_editor_property(...)` and assigning the same list back writes nothing; build NEW struct instances
   and assign a fresh `unreal.Array`. `AnimSegment` for montage slot tracks likewise.
4. *`save_asset` returns False after a PreSaveRoot-driven save* — verify by mtime.
5. *No scriptable crop* (`AnimationLibrary` has no trim); a montage segment's `anim_start/end_time` does
   the same job without a derived asset.
6. *Sockets are per skeleton.* The fixup's "copy every socket from SurvivalMan" was right for
   compatible playback and wrong for native clips; the preview (attached on `…Aim`) and the game (attached
   on `…Relaxed`) disagreed by the un-retuned socket, not by the pose — proven by sampling the live PIE
   skeleton against the clip on the same mesh: every bone ≤0.1°, grip vector 35.1 = 35.1 cm.

**Root-motion set swapped too (same session, later):** the 122 `rm_W2_*` clips the CHT played from
`/Game/AZ/NoWeapons/RootMotions/` (starts, stops, pivots, stance changes, jump takeoffs) are the pack's
non-IPC clips with an `rm_` prefix added on import, so their MetaHuman twins are `AZ_RTG_MH_W2_<x>` (no
`rm_`). Their retargeted root motion was already in the root track (jog jump 477 vs 495 cm, 90° pivots
preserved) but the flags were off: `enable_root_motion`, `root_motion_root_lock`, `force_root_lock`
(jumps), `loop`, `rate_scale` transplanted from the old clip, notifies copied where present. CHT: 126 refs
→ 0 `rm_` refs left. The 10 `AS_P01_Jump_*_Land` composites still segment the old `rm_W2_*_Jump*` clips.

**Open:** `PSD_P01_Land` + 10 `AS_P01_Jump_*` composites + 2 `Fall_v2` still SurvivalMan-sourced;
`AimYaw` reads −90 when looking straight ahead (pre-existing, check the AO axis convention when aiming);
4 orphan nodes (2 Modify Bone + 2 conversions) in `AZ_ABP_MoverHero_MHC` to delete on the next compile.
