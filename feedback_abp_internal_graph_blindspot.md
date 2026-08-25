---
name: feedback-abp-internal-graph-blindspot
description: "★ ListAnimGraphNodes only lists the TOP-LEVEL AnimGraph — the CMC ABP's Steering ×2, OrientationWarping, ResetRootTransform live INSIDE the MM node's blend-stack bound graph. OW alpha = clip curve Enable_Warping; missing curve = 0 = node silently OFF (AnimPro set lacked it entirely; authored on 33 clips 2026-08-24). Never report node ABSENCE from one non-recursive dump."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-24T19:40:50.868Z
---

# The 2026-08-24 "no OrientationWarping" false negative

**What happened:** `UAZ_AnimGraphNodeUtils::ListAnimGraphNodes` returns only the TOP-LEVEL AnimGraph
(22 nodes in AZ_ABP_CmcAnimInstance). The pose-processing chain — `ResetRootTransform → Steering →
Steering(TIP, gated on CurrentDatabaseTags CONTAINS "TurnInPlace") → OrientationWarping` — lives inside
the **Motion Matching node's blend-stack bound graph** (list it with `ListBlendStackGraphNodes` + the MM
node GUID). I declared the warping nodes absent from a top-level dump; the user's screenshot proved them
present. Our own [[project_gasp_cmc_abp_spec]] documents this exact spine — it was never consulted.

**The real fault found there:** OrientationWarping's Alpha = `GetCurveValueFromAnimation(CurrentBlend
StackAnim, "Enable_Warping", time)`. GASP clips carry `enable_warping` (FName compare is case-insensitive);
the retargeted AnimPro set carried it on ZERO clips → missing curve reads **0** → the one node whose job
is reducing turn foot-slide was silently disabled since the content swap. Every movement-side tune
(rotation rate, friction, lean) sat upstream of a dead node — why "nothing changes" repeated for a day.

**Fix applied 2026-08-24:** `Enable_Warping = 1.0` authored + saved on 33 clips (walk/run/sprint loops,
all starts, all stops, crouch set). Deliberately ABSENT (= 0, correct per GASP convention) on pivots,
turn-in-place, idles, arc loops — warping assumes straight motion. AnimSequence saves from Python are safe.

**Why:** absence-of-evidence from one tool is not evidence of absence, and the project's own build spec
is the first thing to check before claiming what a graph does or doesn't contain.

**How to apply:**
1. Before asserting anything about the CMC ABP graph, read [[project_gasp_cmc_abp_spec]] and dump BOTH
   levels: `ListAnimGraphNodes` AND `ListBlendStackGraphNodes(MM node GUID)`.
2. Any node whose alpha/enable comes from a clip curve: verify the curve EXISTS on the actual content
   (`AnimationLibrary.get_animation_curve_names`) — missing curve = 0 = silently off, never an error.
3. New content packs / retargets: diff their curve set against GASP's (`enable_warping`, `movedata_speed`,
   `contact_l/r`) before wiring anything that reads curves.

Related: [[feedback_blendstack_input_ref]], [[project_cmc_movement_feel_tuning]], [[project_locomotion_quality_standard]]
