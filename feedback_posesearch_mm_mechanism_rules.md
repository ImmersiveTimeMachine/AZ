---
name: feedback_posesearch_mm_mechanism_rules
description: "★★★ MUST RULES R1-R15 for PoseSearch/MM selection — continuing-pose exemption, BranchIn = indexed range, cost contests, BlockTransition sizing, keep-alive undoing strips, R11 inert guards, R12 reindex = no result, R13 PHASE-LOCKED start->loop seam (content ends on loop frame 0), R14 BranchIn-less clip = silent empty search, R15 montage sections are INVISIBLE to PoseSearch (flat SlotAnimTracks[0] timeline; SamplingRange is the lever; 1 slot track only), R16 a REACTION schema must carry NO trajectory channel (else the cost picks the least-moving clip every time). Read before changing any MM pool, notify, gate or seam."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-09-01T03:27:02.924Z
---

# MM mechanism rules — written after getting the same things wrong twice in one session

**User, 2026-08-28:** *"it's the second time I've reasoned from that exemption incorrectly so create some
rules not to repeat the same errors"*

Built during the CMC jump work, where **five bugs in a row** came from misunderstanding how PoseSearch
actually selects, not from the design being wrong. Every rule below is tied to a specific error.

## R1 — The continuing-pose exemption keeps a clip SEARCHABLE. It does NOT keep it SELECTED. ★★★

**Got this wrong TWICE.** I twice wrote that a clip would "play to its end because PoseSearch exempts the
continuing pose from database filtering". It does not work that way.

- What it does: the currently-playing asset stays a valid candidate even when its database is stripped
  from the pool.
- What it does NOT do: stop a **cheaper** candidate from beating it on the very next search tick.

Measured: `AnimPro_JumpIdleLand` was taken over by `AnimPro_Idle` (cost −0.09) **292 ms into a 1.03 s
clip**, precisely because the character was standing still and idle is a near-perfect pose match.

**Rule: if a clip MUST play for a duration, protect it with `BlockTransition`. Never argue from the
continuing-pose exemption that it will survive.**

## R2 — Anything that must play at a specific MOMENT must not be in a cost contest ★★★

Same lesson, learned twice in the same feature:
- **Takeoff** lost to `AnimPro_FallingLoop` (+0.03). At spd=0 there is no horizontal signal, so the search
  falls back to pose distance, and a crouched launch frame is FURTHER from standing idle than a neutral
  fall pose is.
- **Land** lost to `AnimPro_Idle` (+0.10 vs −0.09). At touchdown the character genuinely IS standing still.

A "correct" clip that depicts a *transition* can never out-match a clip that depicts *the state you are
actually in*. Tuning costs is fighting the wrong battle.

**Rule: narrow the POOL to a single candidate for that moment. Do not try to out-tune the cost.**
GASP achieves the same thing by making takeoffs direct-play (`bUseMM=False`); on a pool architecture the
equivalent is a state-gated exclusive pool.

## R3 — `BranchIn` defines the INDEXED RANGE, not just an entry hint

Setting BranchIn to `[apex+0.05, end]` left the ENTIRE rise of every takeoff clip **out of the database**,
so MM could not select a takeoff pose at all — only post-apex falling frames, which look like the fall
loop. Re-indexing from the launch frame fixed the indexing but not the bug (see R2).

**Rule: BranchIn window == what the database contains. Index from the first frame you ever want ENTERED.**
Corollary: BranchIn also OWNS membership (`PreSaveRoot` → `SynchronizeWithExternalDependencies`), so never
also add a BranchIn clip explicitly — that produces double entries.

## R4 — `BlockTransition` marks poses as INVALID ENTRY CANDIDATES. It does NOT hold a playing clip. ★★★

**I recorded the wrong semantic here first ("blocks leaving") and built two broken notify layouts on it.**
The engine source is unambiguous:
- `FBlockTransitionFilter::Filter` passes a pose only if `!Metadata.IsBlockTransition()`
  (`PoseSearchFilter.h:195`); discarded poses are flagged `DiscardedBy_BlockTransition`
  (`PoseSearchContext.h:74`).
- "kdtrees don't contain block transition poses by construction" (`PoseSearchDatabase.cpp:2137`).

So BlockTransition = **"these frames may not be ENTERED"** — an entry-window restrictor. That is exactly
how this project's stops use it (`[10%, 100%]` = a stop may only be entered near its start), and how GASP
uses it. What it can never do is keep a playing clip alive.

Measured proof of the wrong model: BlockTransition `[0, 85%]` on the land clips made every impact pose
unselectable — a lands-only forced search returned NOTHING for a whole 250 ms window, then idle took over.
(Also re-attributes the earlier "no land, clip held 2.6 s" bug: that hold was the keep-alive of R6 alone;
the takeoff's BlockTransition was not the mechanism, my fix note for it was wrong.)

**Rule: BlockTransition restricts ENTRY. To keep a clip playing, keep its competitors out of the POOL
(R2/R5) — there is no notify for "don't leave".**

## R5 — Holding a playing one-shot = keep competitors OUT OF THE POOL until it is almost done

A fixed-time exclusive window (`0.25 s`) expired mid-clip on 0.90-1.40 s lands and idle stole the clip at
292 ms. The proven in-project mechanism (the stops, via `KeepPlayingOneShotSearchable`) is:
**while the one-shot is the CURRENT selection and `SelectedTime < FRACTION × PlayLength`, strip the competing
databases.** Time windows are for entry; progress-based pool exclusivity is for holding.

**★ The release FRACTION must land BEFORE the clip's own entry poses age out of `PoseReselectHistory`
(node default 0.30 s).** Measured on the 1.03 s idle land: with the release at 0.85 an exclusive lands-only
search at ~0.69 s had NO legal candidate except re-entering the clip's own entry window — the pick snapped
`0.70 -> 0.13` and the legs visibly dipped forward and back at the end of the landing. At 0.60 the pool is
already open when that moment arrives, idle wins on cost (−0.10 vs +8), and the snap is not merely
discouraged but impossible. **An exclusive pool with only one legal answer will re-enter its own clip
forever; always ask what the search is allowed to pick at the moment the window closes.**

Corollary instrument: `[CmcPick]` only fires when the ASSET changes, so a same-clip time snap is INVISIBLE
to it. `[CmcSnap]` (log when `SelectedTime` jumps backwards or forwards more than a frame's worth, excluding
loop wrap) is what made this diagnosable in one PIE.

## R6 — Know what runs AFTER your strip. `KeepPlayingOneShotSearchable` re-adds. ★★

```cpp
Pool = Get_DatabasesToSearch();            // my rising/falling split stripped JumpStarts
KeepPlayingOneShotSearchable(node, Pool);  // ...and this put them straight back, every frame
```
The split silently never held. The keep-alive re-adds a playing one-shot's DB for
`OneShotKeepAliveFractionTunable` (0.7) of clip length — 2.47 s on a 3.53 s takeoff — so the takeoff stayed
selected through apex, the fall AND the whole land window.

**Rule: a strip in `Get_DatabasesToSearch` is not final. Trace the full per-frame path before believing a
pool change took effect — and add the pool to the log rather than assuming.**

## R7 — Log the POOL, not just the pick

`[CmcPick]` originally printed only the chosen database. That cannot distinguish "wrong clip won" from
"right clip was never a candidate" — and those need opposite fixes. Adding `pool=[...]`, `justLanded=`
and `velZ=` turned three separate guessing rounds into two-minute diagnoses.

**Rule: for any selection bug, print the CANDIDATE SET before theorising. Do it FIRST, not after the third
wrong fix.**

## R8 — A severity/variant clip with no gate will win on the common case

`AnimPro_JumpIdleLandHard` sat in the same DB as the normal land and won every ordinary jump. There was no
impact-speed gate yet.

**Rule: do not put a variant in the pool before the condition that selects it exists. Keep it out until
its gate is built.**

## R9 — An MM-selected clip can only fire notifies while it is the NEWEST blend-stack player ★★★

`FAnimNode_BlendStack_Standalone::UpdateAssetPlayer` (`AnimNode_BlendStack.cpp:893-898`) gives the ACTIVE
context to the newest player only, then `bLocalIsContextActive &= !bIsPlayerContextActive` — every
blending-out player is updated with `AnimPlayerContext.AsInactive()`.

**Consequence: a notify placed after the hand-back point of a one-shot NEVER FIRES.** An "end of the land
clip" gameplay-event notify is dead on arrival in this architecture, because the whole point of the design
is that idle takes over before the clip ends.

`bShouldFilterNotifies` is NOT this mechanism — it is only de-duplication (suppresses the same notify NAME
within `NotifyRecencyTimeOut`, 0.2 s), which matters because the stack can hold several copies of one clip.

**Rule: if something must be signalled when a one-shot FINISHES, the signal belongs to whoever owns the
clip's progress (the anim instance's game-thread snapshot), not to a notify near its end.** Notifies are
reliable only in the part of a clip where it is the newest player.

## R10 — Entry window = BranchIn opens indexing, BlockTransition closes entry after it

The working jump layout, and the general shape for any "must be entered at a specific moment" clip:

| clip | BranchIn | BlockTransition | result |
|---|---|---|---|
| takeoff `AnimPro_JumpIdleStart` | `[0.175, end]` | `[0.325, +3.208]` | enterable only in `[0.175, 0.325]` |
| land `AnimPro_JumpIdleLand` | none (explicit DB member) | `[0.150, +0.883]` | enterable only at impact `[0, 0.15]` |

Measured effect: takeoff entry `t=0.67 -> 0.20`, land entry `t=0.67 -> 0.00` (of 1.03 s). Before this, MM
entered the land 65% in and the impact was simply never seen — the clip name in the log was RIGHT the whole
time, which is why "correct name, wrong motion" must always be checked with an ENTRY TIME in the log.

## R11 — Every continuing-pose guard is INERT until the search is told what is playing ★★★

`ContinuingPoseCostBias`, `bDisableReselection` and `PoseJumpThresholdTime` all key off
`FPoseSearchContinuingProperties::PlayingAsset`. A `MotionMatch` call with a default-constructed
`FPoseSearchContinuingProperties()` (the Mover spine, until 2026-08-31) has NO continuing pose — every
tick is a fresh argmin and none of the three can engage. Measured: the walk loop re-cut itself every 1–2
frames between half-cycle phases, and two "fixes" (a −10 bias, `PoseJumpThresholdTime`) changed nothing
because there was nothing to bias. Pass the BlendStack's REAL playhead (outer node
`GetAnimAsset/GetAccumulatedTime/GetMirror`, never dead-reckoned), loops only. Full chain:
[[feedback_mover_spine_search_continuity]].

## R12 — After a database save: `PreCancelled` + FLT_MAX cost = no index, not a result

Saving a database with changed entry flags rebuilds derived data and cancels the others
(`LogPoseSearch: … PreCancelled because of PSD_X`). Until it completes, searches return NOTHING
(`cost=+3.4e38`, `entry=0.00`). A "clean" run in that state proves nothing; and a spine that falls
back to "push at frame 0" on an empty result turns it into a per-frame restart. Treat an empty result
with the same loop still playing as "continue" — **but only when the playing loop is a CANDIDATE of the
row that searched** (array member or `UPoseSearchDatabase::Contains`). An unconditional keep held the
STANDING walk loop for 513 crouched frames when crouch was pressed mid-walk (row change + empty search,
2026-08-31 03:00): a row change with an empty result must still take the row's clip at frame 0.

## R13 — A transition clip that ends ON the loop's frame 0 must hand over PHASE-LOCKED, not searched ★★★

Content fact (measured 2026-08-31, pelvis-space pose distance vs every loop frame at 30 Hz): **every**
MovementAnimsetPro transition — all Walk/Run/Strafe/Bwd/Crouch starts, all directional starts, all
`Land2Walk/Land2Run`, `JumpIdleLand2Walk` — ends within d≈1–5 of its loop's **frame 0**, and at
end−0.15 s sits at loop phase 0.87–0.93 (walk, 1.0 s loops) / 0.50–0.67 (run, 0.767 s), d≈50–100 from
frame 0. The SM flips to `LocomotionLoop` `TransitionAlmostCompleteThreshold` (0.15 s) early, and MM then
picked **frame 0** on 8/8 `WalkFwdStart -> WalkFwdLoop` handoffs (entry=1.00/1.00, cost +9…+99): the pose
the start WILL reach, not the one it is at. Under a 0.2 s crossfade that reads as "the leg plays from the
start" — reported as an idle-jump bug, it was on EVERY start (three "fixes" to jump/land code changed
nothing: the landing was never the fault).

**Rule: MM decides WHICH loop; the OUTGOING clip decides the entry PHASE.** At a LocomotionLoop push,
if the outgoing blend-stack player is a non-looping sequence with `Remaining ≤ threshold + 0.1 s`, set
`StartTime = (LoopLen − Remaining) mod LoopLen` and clamp the crossfade to `Remaining` (a non-looping
player past its end holds its last frame; a longer blend drifts the streams apart again). Verified:
`entry=0.86 seam=lock rem=0.14 blend=0.14`, 0 `[v2 Snap]`, run 0.62/0.77, crouch 0.76/0.90, user
confirmed the artifact gone. Stops re-pressed mid-clip (Remaining large) keep MM ownership.
Owner: `UAZ_MoverAnimInstance::SetBlendStackAnimFromChooser`, MM branch, `SeamLockRemaining`.

Corollary instrument: `[v2 Pick]` prints `seam=lock|mm rem= blend=` — `seam=mm` on a start->loop line
means the gate did not fire; read `rem`.

## R14 — A raw clip with NO BranchIn is a SILENT empty search (no log line at all)

`UPoseSearchLibrary::MotionMatch` over a sequence adds databases only through that sequence's
`UAnimNotifyState_PoseSearchBranchIn` notifies (`PoseSearchLibrary.cpp` `AddToSearch`). A null
`Database` on the notify logs an Error; a clip with **no notify** adds nothing and returns nothing —
no LogPoseSearch line, `cost=FLT_MAX`. Measured 2026-08-31: `AnimPro_Crouch_WalkFwd_new` (explicit
entry in `PSD_v2_StrafeCrouch`, no BranchIn) — the strafe-mode crouch row passes the DATABASE and
works (+0.05); the orientation-mode crouch row passes the raw clip and every search was empty
(7× `[v2 MMFallback]`, stop->loop resumes at frame 0). AssetRegistry check: a DB with only ABPs as
referencers has NO BranchIn clips pointing at it. Fix recipe: [[feedback_posesearch_branchin_db_sync]].

## R15 — PoseSearch indexes a MONTAGE as one flat timeline; SECTIONS DO NOT EXIST to it

Read in `PoseSearchAssetSampler.cpp` 2026-08-31: pose extraction is
`AnimMontage->SlotAnimTracks[0].AnimTrack.GetAnimationPose(Out, Ctx)` at an ABSOLUTE montage time
(:440-451), root motion likewise via `AnimTrack.GetRootMotionExtractionStepsForTrackRange` (:56-71).
**Zero references to `CompositeSections` / section lookup anywhere in the PoseSearch source.** Therefore:
1. Montages ARE first-class (`FPoseSearchDatabaseAnimMontage`, `FPoseSearchInteractionAssetItem::Animation`
   is `TObjectPtr<UAnimationAsset>`) — and GASP's PSIAs point BOTH role items at montages
   (`AM_*_A` / `AM_*_V`, verified on Walk_L / Run_F / stand_B / tackle_F_Lfoot; `PSIA_..._stand_F` is
   missing its Attacker ref — a broken sample asset, don't copy it).
2. A PSIA item / DB entry addresses the WHOLE montage, never "section X". The index covers every segment
   laid end to end, INCLUDING the segment seams that never play back-to-back at runtime, and a search may
   return `SelectedTime` anywhere in the full length.
3. **The lever is `SamplingRange`** (`FPoseSearchDatabaseAnimMontage::SamplingRange`, and the same field on
   the MultiAnimAsset DB entry; `[0,0]` = whole asset, readonly when a BranchIn drives it). Trim it to the
   window you actually want selectable.
4. `SlotAnimTracks.Num() != 1` → `UE_LOGF(Error, "so far we support only montages with one SlotAnimTracks")`
   and ref pose / identity root. **Silent-ish failure — check the count before indexing.**
5. R14 does NOT bite interactions: `MotionMatchMulti` takes the DATABASE via the availability/query, so no
   BranchIn notify is involved.

## R16 — A REACTION schema must NOT carry a trajectory channel (measured 2026-08-31)

Selecting a one-shot the character does not *choose* (hit reaction, knockback, any imposed motion): the
DB's trajectory samples are the CLIP's future = the reaction itself (flying backwards); the query's
future is the character's own predicted intent, which is never that. The cost then systematically
favours **whichever clip moves least**, not the one whose body state matches.
Measured on `PSD_AZ_ChalkieReactions` (9 knockbacks): with the cloned locomotion schema's Trajectory
channel, EVERY hit returned `AM_Zombie_KB_Chase_5` — the smallest-travel clip in the pool (85cm vs
118-224) — cost 0.5-0.8. Removing the Trajectory channel (pose channels only: Position foot_l,
Velocity foot_l/r, Heading pelvis) → Chase_1 / Atk_1 / Chase_1 / Atk_4 across four hits, cost 0.0-0.4.
⇒ **Locomotion schema = trajectory (where am I GOING). Reaction/interaction schema = pose only (what is
my body DOING).** GASP's `PSS_CharacterInteraction` has no trajectory channel either — same reason.
Gotcha while iterating: touching the SCHEMA cancels the DB index (`PreCancelled because of PSS_...`);
close the schema tab, reopen the DB alone, wait for a NEW `BuildIndex Succeeded` key.

Related: [[feedback_verify_never_presume]], [[project_cmc_input_gap_doctrine]],
[[project_cmc_jump_build_order]], [[project_jump_system_status]], [[project_cmc_mm_content_verdict]],
[[feedback_mover_spine_search_continuity]].
