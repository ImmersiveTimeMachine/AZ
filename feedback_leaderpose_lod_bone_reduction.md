---
name: feedback-leaderpose-lod-bone-reduction
description: "Garments blowing off the MetaHuman body was LOD bone reduction, not proportions — leader-posed followers read the leader's CURRENT LOD bone set."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-20T04:16:52.573Z
---

Symptom (2026-08-19/20, `BP_CMC_Hero`): garments sat correctly at idle, drifted at walk/run, and blew
visibly outside the body at SPRINT. Fixed instantly by **ForcedLodModel = 1 (LOD0) + MinLOD 0** on
`CharacterMesh0`. No retarget, no re-skin needed.

**Mechanism.** `SKM_MHC_Hero_BodyMesh` has **4 LODs + `Body_LODSettings`**, which applies MetaHuman **bone
reduction** at LOD1+. Every garment has **1 LOD, no LOD settings**. A `LeaderPoseComponent` follower does
not evaluate anything — it reads the leader's bone transforms **at the leader's current LOD**. The moment
the body dropped an LOD, the bones the garment needed stopped being evaluated, so the follower skinned
from stale/absent transforms. Extreme poses (sprint) show it worst because the error scales with how far
the missing bones would have moved.

★ **Diagnostic rule: this class of bug is invisible in the DCC.** Blender/Maya evaluate one mesh at full
resolution with unlimited influences and no LODs. "It deforms perfectly in Blender" says the weights and
geometry are sound and says NOTHING about engine behaviour. When a mesh is right in the DCC and wrong in
UE, suspect the things UE adds: LOD (bone reduction), influence-count truncation + renormalisation,
tick order, URO. Check LOD counts on leader vs follower FIRST — it is one query and it is free.

**Proper fix (not yet done):** forcing LOD0 costs the hero its LOD system. Give garments matching LODs —
apply `Body_LODSettings` to each garment so it generates the same 4 levels with the same bone reduction —
then clear `ForcedLodModel` back to 0. Until then the forced LOD is a knowingly-carried workaround.

**The detour this cost, recorded so it is not repeated.** Measured bone lengths: MetaHuman vs SurvivalMan
torso differs **-16.3%** (spine_03<-pelvis 21.07 vs 17.63), arms -8.8%/-5.6%, legs match within 3%, total
stature +0.1%. That mismatch is REAL, and leader pose does copy transforms with no retargeting — but it
was NOT the operative cause. A whole evening went into re-skinning the hoodie to `metahuman_base_skel`
(and evaluating `Retarget Pose From Mesh`) before the one-line LOD test settled it. Measure the cheap
engine-side explanations before committing to content work.

**Re-skin outcome, if it is ever wanted:** `SKM_SurvivalMan_hoodie_without_hood_mh` now exists — 342 bones,
`metahuman_base_skel`, bone deltas 0.000 vs the body, original geometry/proportions preserved. It works
but is OPTIONAL; the original SurvivalMan-skeleton garments are fine with LOD0 forced. Also note it binds
to a skeleton MetaHuman assembly REGENERATES (path carries `Female/Medium/NormalWeight`), so any body-type
change owes the conform again — the argument against re-skinning as a standing solution.

**Blender export settings that round-trip correctly to UE** (verified by exporting unchanged and measuring
identical bounds): Scene Unit Scale **1.0** (NOT 0.01 — combining 0.01 with `Apply Scalings: FBX All`
double-applies and imports at 1/100), Scale 1.0, `Apply Scalings: FBX All`, -Z Forward / Y Up,
**Add Leaf Bones OFF**, Bake Animation off, Limit to Selected Objects. **Shrinkwrap is the wrong conform
tool for garments** — it projects onto the skin and destroyed thickness (extent y 18.27 -> 15.56,
z 32.94 -> 29.98). Use Surface Deform or a lattice.

Related: [[feedback-metahuman-modular-hero]] (the LeaderPoseComponent setter rule + what re-assembly wipes),
[[project-cmc-backport-spike]].
