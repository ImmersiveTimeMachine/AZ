---
name: feedback_planted_foot_curve_guard
description: "★ bLeftFootDown must be written ONLY while a contact curve is present — an unguarded `contact_l > 0.5` reads FALSE on every curve-less clip (all jump/land clips), so every landing picked the _LU land regardless of takeoff foot. Same guard as CMC's Update_MovementDirection; the air latch cannot save touchdown."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-31T01:00:17.178Z
---

# Planted foot: guard the curve read (2026-08-31)

**User:** *"I'm not sure that some landing and the continuation of this landing is the right leg sequence"* — correct.

**Measured:** `WalkFwdLoop -> JumpWalkStart_RU | Lfoot=1` then `JumpWalkStart_RU -> JumpWalk_LU_Land2Walk | Lfoot=0`
on every RU jump. `CHT_v2` picks the land on `bLeftFootDown` (True → `_RU`, False → `_LU`; 18 stop rows
agree), so the foot flipping to false in the air selected the wrong land every time.

**Cause:** `AZ_MoverAnimInstance.cpp` wrote `ChooserContext.bLeftFootDown = GetCurveValue("contact_l") > 0.5f`
unguarded (two sites: the Mover path and the CMC-fallback path in the same class). No jump or land clip
carries a contact curve (verified: 0 of 428 RTG_AZ clips have `FootSpeed_*`; jump clips have 0 curves),
so the read is 0 → false on every airborne frame, INCLUDING the touchdown frame the land rows read.
The existing air latch (`LastGroundedLeftFootDown`) only substitutes while `SMState` is an AIR state;
touchdown has already switched to the land transition before the write runs.

**Fix (the CMC instance already had it):**
```cpp
const float ContactL = GetCurveValue("contact_l"), ContactR = GetCurveValue("contact_r");
if (ContactL > 0.5f || ContactR > 0.5f) { ChooserContext.bLeftFootDown = (ContactL >= ContactR); }
```
The last grounded value then persists through takeoff / air / land on its own. Walk loops always have a
foot planted; the run loop's flight phase (both 0) also just holds the previous foot — correct.

**Corollary:** the curve NAMES live on the ABP CDO (`contact_l`/`contact_r`, threshold 0.5), not on the
C++ defaults (`FootSpeed_L/R`, which no clip has). Read the CDO before concluding the foot signal is dead
([[feedback_verify_never_presume]]).

Related: [[project_mover_metahuman_2026-08-31]], [[feedback_mover_spine_search_continuity]].
