---
name: feedback-metahuman-modular-hero
description: "MetaHuman hero on our SurvivalMan animations — re-assembly wipes the fixup, and LeaderPoseComponent must be SET BY THE SETTER, not the property."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-31T01:00:41.095Z
---

Two traps hit while porting `MHC_Hero` onto `BP_CMC_Hero` (2026-08-16). Both fail SILENTLY.

## 1. MetaHuman re-assembly wipes our setup — re-run the fixup script

`/Game/MetaHumans/Common/**` and `/Game/AZ/Blueprints/Character/AZ_MHC_Hero/**` are **regenerated** by the
MetaHuman assembly. Everything we configure there dies on the next re-assembly from MetaHuman Creator:

- `metahuman_base_skel.CompatibleSkeletons += SKEL_SurvivalMan`
- `metahuman_base_skel.bUseRetargetModesFromCompatibleSkeleton = true`
- the 9 sockets copied from `SKEL_SurvivalMan` onto `SKM_MHC_Hero_BodyMesh`

**Symptom when lost:** hero silently stops animating, and every socket-attached thing (weapons via
`BackRifleSocket` / `RightHandRifleSocket*`, grab anchor via `GrabbedSocket`, grab IK via
`GrabIK_HandL/R`) attaches to nothing with no error and no log.

**Fix:** run `C:\UnrealEngine\Games\AZ\Tools\metahuman_fixup.py` in the editor. Idempotent, verified.
(That script IS committed — `Tools/metahuman_fixup.py`, since a583037.)

★ **`/Content/AZ/Blueprints/Character/AZ_MHC_Hero/` is DELIBERATELY NOT COMMITTED — do not "fix" this.**
It is **516 MB**, and **this project does not use LFS** (user confirmed 2026-08-17): `.gitattributes`
line 1 declares `*.uasset filter=lfs`, but line 5 `*.uasset !text !filter !merge !diff` overrides it —
`git check-attr filter` returns `unspecified` and tracked uassets in HEAD are raw binary, not pointers.
The whole pack was 3.88 MiB before that commit. So this folder is the ONE exception to "everything under
/Content/AZ/Blueprints is tracked": it is regenerable from MetaHuman assembly + the fixup script, so it
stays out. `Plugins/` (5.9 GB) is gitignored for the same reason.

**Why both properties live on the MetaHuman skeleton, not ours:** `CompatibleSkeletons` is documented
"not bi-directional" (`Skeleton.h:339`) — a skeleton consumes animation from the skeletons in ITS OWN
list — and the runtime reads the flag off the TARGET (`AnimationDecompression.cpp:72`,
`AnimationRuntime.cpp:2954`). The editor helper `IsCompatibleForEditor` also accepts the reverse via an
asset-registry tag, which is misleading: it does NOT make the runtime work.

`weapon_r_muzzle` is intentionally NOT ported — bone `weapon_r` doesn't exist on the MetaHuman skeleton
and nothing in `Source/` references that socket.

## 2. LeaderPoseComponent: the property is not the setter

Setting `leader_pose_component` on a BP template (or in Python) **serializes fine and reaches the
instance**, but the garments stay frozen. `SetLeaderPoseComponent()` is what registers the component into
the LEADER's `FollowerPoseComponents` list and rebuilds the bone map; without the call, nothing pushes
the pose. Worse, having the property set also stops the follower ticking its own pose — so it is frozen
from both directions. **Symptom: cloth moves with the actor but never animates.**

Second trap: the setter **early-outs when the new leader equals the current one**
(`SkinnedMeshComponent.cpp:3181`). Since the template already holds that value, you MUST pass
`bForceUpdate=true` or the call is a silent no-op.

Owner: `AAZ_CmcCharacterBase::WireModularMeshFollowers()` (called from `PostInitializeComponents`), logs
`[CmcMesh] ... wired N ... (M excluded)`. "wired 6" vs "wired 0" is the entire diagnosis.

**Do NOT give garments their own ABP.** Each ABP instance runs its own motion-matching search and drifts
out of sync with the body, at 6x the cost. Leader pose copies the body's final pose bone-for-bone and is
nearly free. The wiring function force-clears any AnimClass it finds on a follower.

**Cross-skeleton leader pose is legal** — `UpdateLeaderBoneMap()` maps purely by bone name
(`SkinnedMeshComponent.cpp:3498`) and never consults `CompatibleSkeletons`. SurvivalMan-skinned garments
ride a MetaHuman body fine as long as bone names match.

Selection is by explicit exclusion list (`ModularFollowerExclusions`, default `{"Face"}`), NOT by
"has no AnimClass" — the latter breaks the moment someone assigns an ABP to a garment. Coupling to the
name "Face" adds no new fragility: `UMetaHumanComponentBase::GetSkelMeshComponentByName` already resolves
the face by that exact string.

## Component layout that works

`Mesh` (ACharacter's own, = MetaHuman body) → `Face` (own ABP_Face) → grooms; garments also under `Mesh`.
Leave `UMetaHumanComponentUE.BodyComponentName` as **"Body"**: it fails to match `CharacterMesh0` and
falls through to Epic's documented pawn fallback ("parent component of the face"). `LODSyncComponent`
has NO such fallback — its drive entry must be `CharacterMesh0` literally (it matches on `GetFName()`).

## Mover-side transplant (2026-08-31) — the same rules, second implementation

The MetaHuman body now also runs on the Mover hero (`AZ_BP_PawnMoverHero_MHC`, see
[[project_mover_metahuman_2026-08-31]]). Facts verified while doing it:
- The body is on **`metahuman_base_skel`**, NOT `SKEL_SurvivalMan`; it animates under SurvivalMan ABPs
  only because `metahuman_base_skel.CompatibleSkeletons = [SKEL_SurvivalMan]` (one-directional). That
  registration lives in gitignored `Content/MetaHumans/` — disk only, and re-assembly wipes it.
- Setting `leader_pose_component` on the duplicated BP's components reproduced the frozen-cloth symptom
  exactly as §2 predicts; the runtime `SetLeaderPoseComponent(Leader, /*bForceUpdate*/ true)` is
  mandatory. Mover owner: file-local `WireModularMeshFollowers_Mover` in
  `AZ_PawnMoverHeroCharacter.cpp::BeginPlay` (no header change → LC-safe); logs `[MoverMesh] … wired 6 …
  (1 excluded)`. The stock hero has no followers and logs `wired 0` — a no-op, so the shared class is safe.
- `BP_CMC_Hero`'s `Cloth_Hoodie` carries BOTH a leader pose AND `AZ_ABP_CmcAnimInstance_C` — the ABP is
  dead weight under a leader pose and contradicts the "no garment ABP" rule; the other five are clean.
  The Mover duplicate gives all six `anim_class=None`.
- Cloth components are `NoCollision`; the body mesh on the CMC hero uses profile `Custom` (QUERY_ONLY).
  The `COLLISION PROFILE [Custom] is not found` spam (thousands/run) is pre-existing and NOT from the
  cloth — a red herring I chased for one round.
- Mesh bounds: MetaHuman body 149 cm vs SKM_SurvivalMan 184 cm; both heroes use relLoc Z=-92 on a
  90 cm half-height capsule. Feet-on-ground and garment fit on the smaller body are still unverified.

Related: [[project-cmc-backport-spike]], [[feedback-no-hardcoded-asset-paths]]
