---
name: feedback-metahuman-modular-hero
description: "MetaHuman hero on our SurvivalMan animations — re-assembly wipes the fixup, and LeaderPoseComponent must be SET BY THE SETTER, not the property."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-16T23:46:49.242Z
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

Related: [[project-cmc-backport-spike]], [[feedback-no-hardcoded-asset-paths]]
