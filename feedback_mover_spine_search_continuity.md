---
name: feedback_mover_spine_search_continuity
description: "★★★ MUST READ before touching the Mover spine's MotionMatch call or any PoseSearch loop DB: the 'walk loop restarts from the beginning' chain — empty ContinuingProperties, outer-vs-inner BlendStack ref, bias -1.0, bDisableReselection via reflection, the reindex/PreCancelled gotcha, and the frame-0 fallback trap. THREE of these were already in memory and got re-hit."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-31T01:00:09.699Z
---

# The Mover-spine loop re-cut chain (2026-08-31) — and the memory I failed to read first

**User:** *"it's like the walk loop is restarted from the beginning"* / *"same bug still persist"* ×3.
Five rounds, four mechanisms. Three of them were ALREADY RECORDED: [[feedback_blendstack_input_node_ref]]
(outer vs inner ref), [[project_cmc_mm_content_verdict]] §0b.1 (Mover passes empty
`FPoseSearchContinuingProperties()`) and §1c (`bDisableReselection` false on every AZ entry). I re-derived
all three from engine source instead of grepping memory. **Rule reinforced: grep memory for the symbol
before reading engine source.** ([[feedback_verify_never_presume]] says the same.)

## The chain, in the order it had to be fixed
1. **Symptom in the log** (`[v2 Snap]`): the walk loop re-cut itself every 1–2 frames between its two
   half-cycle phases (`0.67↔0.93`, `0.07↔0.67`, `0.37↔0.17` — always ~0.5 s apart = left vs right
   stride) with all candidates within ±0.05 (`cost 1.66–1.73`). After a landing it re-cut to `1.00`
   (= frame 0 on a loop) at cost +150…+225 — literally "restart from the beginning".
2. **Cause 1 — no continuity.** `AZ_MoverAnimInstance.cpp` called
   `UPoseSearchLibrary::MotionMatch(this, AssetsToSearch, "PoseHistory", FPoseSearchContinuingProperties(), …)`
   — EMPTY. Every tick was a fresh argmin. Without `PlayingAsset` the engine's three guards
   (`ContinuingPoseCostBias`, `bDisableReselection`, `PoseJumpThresholdTime`) all have nothing to
   act on — **they are inert, not weak.** Loops only (the CMC spine's proven rule): a finished one-shot
   must not get a continuity bonus at LocomotionLoop.
3. **Cause 2 — wrong accessor.** First fix used `UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAsset/
   Time/Mirrored(BlendStackNode)`. Those cast to the INNER `FAnimNode_BlendStackInput`
   (`BlendStackAnimNodeLibrary.cpp:15-37`); the Mover ABP hands the OUTER node, so they return
   null/0 silently → `cont=None` on 52/52 while `nodeRefIsBlendStack=1`. Correct for an outer
   reference: `ConvertToBlendStackNode` → `GetAnimNodePtr<FAnimNode_BlendStack_Standalone>()` →
   `GetAnimAsset()` / `GetAccumulatedTime()` / `GetMirror()` (reads `AnimPlayers[0]`, the NEWEST
   player, `AnimNode_BlendStack.cpp:871`). Do NOT dead-reckon the time (§0b.1 incident).
4. **Result of 2+3:** snaps 100 → 6 per run. The 6 survivors were ALL at the loop wrap
   (`cont=…@0.98–0.99`) with cost +18…+160 — the loop could never complete a cycle. Loop verified
   seamless (first vs last frame ≤1.9° on every leg bone; `Loop=True`), so it is scoring, not content.
5. **Cause 3 — nothing forbids a same-asset re-cut.** The clip-list `MotionMatch` overloads have NO
   `PoseJumpThresholdTime` parameter (only the internal Databases overload does), so on this path the
   guards are the DB `ContinuingPoseCostBias` (public; raised −0.01 → −1.0 on the 6 loop DBs — a
   stopgap that re-prices, not a guard) and per-entry **`bDisableReselection`** (the binary guard:
   `PoseSearchDatabase.cpp:1557-1583` puts every pose of the continuing asset into `NonSelectableIdx`;
   the continuing pose is still evaluated separately via `SearchContinuingPose`, so "continue" stays
   legal). GASP ships it TRUE on all 1604 entries. Set TRUE on all 28 loop entries (Loco_Loops,
   WalkLoco, RunLoco, StrafeWalk/Run/Crouch); NOT on `PSD_v2_Jump` (one-shots get no continuity).
6. **How it was set** (the entry array is PRIVATE, not script-reflected, no public accessor; LC cannot
   add a UFUNCTION): a TEMPORARY body on an existing UFUNCTION using reflection —
   `FindFProperty<FArrayProperty>(UPoseSearchDatabase::StaticClass(), "DatabaseAnimationAssets")` +
   `FScriptArrayHelper` + `reinterpret_cast<FPoseSearchDatabaseAnimationAsset*>(GetRawPtr(i))->bDisableReselection = true`
   — then `git checkout` the file and recompile so the live editor drops the patch. Verified via T3D
   export (`bDisableReselection=True` serializes because it is non-default). The proper setter
   already exists on `spike/cmc-blendstack-spine` (`SetDisableReselectionOnDatabase`) — cherry-pick it
   at the next editor-closed build instead of repeating the hack.

## Two traps that bit AFTER the fix
- **Reindex gotcha.** Saving a database with changed entry flags rebuilds its derived data and
  CANCELS the others: `LogPoseSearch: <hash> - PSD_X PreCancelled because of PSD_v2_Loco_Loops` (38 lines).
  Until the rebuild completes (or after an editor restart), EVERY search into those DBs returns
  NOTHING: `[v2 Pick] … cost=+340282346638528859811704183484516925440.00 entry=0.00` (**FLT_MAX =
  no index**). A run with `Snap=0` in that state proves nothing. Check for `PreCancelled` and
  FLT_MAX before calling a fix confirmed. ([[feedback_posesearch_branchin_db_sync]] restart gotcha.)
- **Frame-0 fallback trap.** The spine's "MM returned nothing → push ChosenAnim at frame 0" fallback
  turned the empty result into 961 pushes of `AnimPro_RunFwdLoop@0.00` in one run. The MM node treats
  "nothing better" as "continue". Fixed: if nothing was found and the SAME loop is already playing,
  return without pushing (`AZ_MoverAnimInstance.cpp`, MM branch). Empty results are also the
  DESIGNED outcome of a single-clip pool once `bDisableReselection` forbids re-entry — so this guard
  is required, not optional.

## Instruments that made this diagnosable (keep them)
`[v2 Pick]` fires on ASSET change and carries `entry=t/len` (the field that exposes "right clip, wrong
motion"); `[v2 Snap]` fires on same-clip time jumps (loop wrap excluded) and carries
`cont=<asset>@<t>` + `nodeRefIsBlendStack`/`loopAtSearch` — that pair is what separated cause 2 from
cause 3 in one PIE; `[v2 MMFallback]` names the clip whenever the search returns nothing. Rule R7 of
[[feedback_posesearch_mm_mechanism_rules]] applies: print the candidate/continuing state before theorising.

Related: [[project_mover_metahuman_2026-08-31]], [[feedback_log_reading_traps]].
