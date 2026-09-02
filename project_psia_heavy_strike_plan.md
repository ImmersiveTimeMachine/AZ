---
name: project_psia_heavy_strike_plan
description: "★★★ NEXT SESSION PLAN (2026-09-02): PSIA heavy strike on the R key — hero heavy + Zombie_Walk_F_x_KnockBack_Walk segment as a PoseSearch Interaction pair. Prereqs, 5 phases with pass/fail lines, failure axes, open decisions. Reuses the catch driver pattern (TryCatchSearch, root-first, bGrabbed rooting). Task #17."
metadata:
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-09-02T04:19:46.965Z
---

# PSIA heavy strike on "R" — plan (written 2026-09-02 04:20, NOTHING BUILT YET)

**Goal.** A dedicated heavy strike (key **R**) that, when a Chalkie is inside the search window, aligns
BOTH bodies through a PoseSearch Interaction pair (hero heavy + a walk-in knockback), plays both halves
in sync so the fist meets the chest at the authored frame, and otherwise falls back to today's warped
heavy. Deterministic contact; the reaction is chosen BEFORE contact by the Chalkie's gait and pose.
Decision context: [[reference_punch_reaction_content_inventory]] § REASSESSMENT (why this is allowed
for the heavy and not for jabs). Machinery already proven on the catch: [[project_grab_grapple_design]]
tail (two-body alignment, root-first ordering, weight semantics).

**"R" = its own input.** Interpreted as the 2026-08-02 verdict ("give the heavy its own input/ability at
range"): new IA `AZ_IA_RT_HeavyStrike` (RT mirror convention, [[project_input_stack_rt_mirror]]) mapped to
R in `AZ_IMC_RT_PawnInputs`, tag `Input.Action.HeavyStrike` as a row in `AZ_InputConfig`, granted with the
fists (QuickBar equip seeds InputTag exactly like BP_GA_Punch_L/R). LMB/RMB jabs untouched. If "R" meant
something else, this is the one line to change.

## Prerequisites (do first, in order)
1. **CLI build before the editor reopens** — six Live Coding patches from 2026-09-02 die on restart.
2. **Read one `[GrabFace]` catch** and close the grabbed-facing question (0.04 spring measures ~0.25).
3. **Impact frames BY EYE** on `Zombie_Walk_F_1..6_KnockBack_Walk` (scrub in the editor, note the frame
   the torso is struck). Script detectors FAILED (torso jerk 1-5cm, root keeps walking). Start with F_5.
   Also note the walk-in root speed and how far the root moves during the reaction. Flip
   `EnableRootMotion` ON on each used sequence (all six are OFF; /Game/Zombie_01 is gitignored — this
   machine only; record the flags in memory).

## Phase 1 — content (editor + Python, ~2h)
- **Heavy trim:** `AM_Fists_Punch_Heavy_L` segment end to ~0.90s (contact 0.34 + follow-through). Needs
  `AZ_MontageUtils::SetSegmentRange(Montage, SegmentIndex, StartTime, EndTime)` — FAnimSegment
  AnimStartTime/AnimEndTime are public fields; new UFUNCTION = closed build. Re-author its warp windows
  afterwards (they currently end at 1.65).
- **Victim montage(s):** `AM_Zombie_KB_Walk_F_5_Strike` = segment [impact − 0.34s, impact + reaction] of
  the sequence (same util). Contact must sit at t=0.34 on the shared timeline = the heavy's contact.
- **Beat notify** on the victim montage at reaction end (`AZ_MontageUtils::AddGameplayEventNotify`,
  Event.Combat.BeatEnd) so the Chalkie's ability ends by the clip, not a timer.
- **Measure the CONTACT STAND-OFF:** pose both clips at their contact frames, knuckle to victim chest
  gap ~5cm (knuckle reach measured 78cm from the hero root; expect ~85-95cm root-to-root — GASP shove
  85.6, our catch 83.4). That number is the PSIA victim Origin (yaw −180, X = stand-off).

## Phase 2 — PoseSearch assets (Python via AZ_PoseSearchUtils, ~1h)
- `PSS_AZ_Strike` = dup of `PSS_AZ_Catch` with the ROLES' SKELETONS SWAPPED (Attacker = hero MetaHuman
  skeleton, Victim = UE4_Mannequin). Channels: the 2 cross-role root-Position channels only (GASP's
  strike schema). NO trajectory channel on the victim (rule R16); no pose channels needed for v1.
- `PSIA_AZ_Strike_Heavy_F5` (UPoseSearchInteractionAsset): items [{Attacker, heavy montage, wT=1, wR=1},
  {Victim, KB segment montage, Origin = (X=stand-off, yaw=−180), wT=0, wR=0}]. The attacker anchors BOTH
  translation and rotation (the player is never moved; the Chalkie is placed on the hero's forward axis,
  so the existing MeleeTarget_Facing warp sees ~0 error — no second writer).
- `PSD_AZ_Strike` (BruteForce, schema PSS_AZ_Strike): one entry per PSIA, `SamplingRange` = [0, 0.15]
  (entry only inside the wind-up). Build the index; check `BuildIndex Succeeded` and that the
  "unsupported non identity root bone" error from CalculateWarpTransforms does NOT appear.
- Variety later: one PSIA per walk variant (F_1..6); the search picks by the Chalkie's current arrangement.

## Phase 3 — code (closed build, ~3h)
- `UAZ_GA_StrikeInteraction : public UAZ_GA_MeleeAttack` — keeps hit window, cancel window, damage,
  RM drive and the warp fallback. Overrides the activation front with `TryStrikeSearch(Target)` =
  TryCatchSearch with the roles swapped (hero AnimContext = Attacker, Chalkie = Victim; `StrikeDatabase`
  UPROPERTY). On success: (1) ROOT THE VICTIM FIRST — new tag `State.Combat.StruckPair`, the infected
  ProduceInput sets `Custom.bGrabbed` for it too, defer one tick until its planar speed < 15 (the catch
  lesson); (2) close-in layered move on the CHALKIE to its aligned transform + `SetFacingOverrideWorld`;
  (3) play the heavy at SelectedTime; (4) send `Event.Strike.Victim` (payload: montage, start time,
  rate) — the Chalkie plays its half with DriveRootMotion (RM ON on the segment) and the staggered tag so
  the BT yields; (5) the normal hit window applies damage at contact. On search failure: the Super path =
  today's warped heavy (log `[Strike] FALLBACK`).
- Chalkie half: an "authored pair" entry in `UAZ_GA_HitReact` (already plays reaction montages with RM
  drive + beat clock) — recommended over a new GA_StrikeVictim. Gate the damage-triggered HitReact by the
  pair tag so the hit does not fire a SECOND reaction.
- Input: IA + IMC mapping + InputConfig row + tag; `BP_GA_HeavyStrike` BP child with `StrikeDatabase`,
  `PunchLunge_L` = trimmed heavy, InputTag = HeavyStrike. Remove the heavy from BP_GA_Punch_L's lunge slot
  or keep it as a range fallback — decision below.
- Instrumentation from day one: `[Strike] search cost/entry/align`, `[Strike] victim@0.15s/0.34s
  moved/dist/vel` (copy the chalkie@ probe), `[Strike] contact fist->chest=Ncm at t=0.34`.

## Phase 4 — PIE (seam-trace first, [[feedback_seam_trace_before_pie]])
Trace before pressing R: Chalkie walking in at ~90cm/s, gap 150 → search selects entry t≈0.05 → aligned
victim transform ~90cm ahead on the hero's forward axis, facing him → close-in 0.15s (asked ≤60cm, else
FALLBACK "implausibly far") → both play → contact at 0.34: knuckle ~5cm from chest → KB root motion
carries the Chalkie back N cm (from the by-eye measurement) → BeatEnd ends the victim ability.
PASS: `[Strike] align ... d<60`; `[Strike] victim@0.34s dist ≈ stand-off ±5`; `[MeleeHit]
BP_GA_HeavyStrike -> Chalkie` at swingT 0.30-0.38 EVERY time (no whiff, no through-body); the Chalkie's
root travels backward during the reaction (RM on); `[Strike] FALLBACK` only when no Chalkie is in range.
FAIL signatures: victim slides during the wind-up → rooting missing (the catch bug again); fist through
chest → Origin X wrong → re-measure, do NOT tune the warp; double reaction → HitReact also fired on damage
→ gate by the pair tag; search never selects → schema skeleton/role mismatch (`results did not cover both
actors`).

## Failure axes (design-first rule)
- Two writers on the Chalkie: crowd slot / BT MoveTo vs the pair close-in — the pair tag must yield the
  BT (as State.Combat.Grabbing does) and root the pawn in the sim.
- Commit-before-contact: the Chalkie starts its half at swing start; a cancelled heavy (hero hit-react
  mid-swing) must END the victim's half (attacker ability end → Event.Strike.Abort).
- Heavy tail: trimmed to 0.9s is a HARD prerequisite — untrimmed, the hero walks 160cm through the victim.
- Content is gitignored: RM flags and segment choices live only on this machine — record them.
- Cost: BruteForce over 1-6 entries x 2 roles is trivial; no broad-phase needed (explicit query).

## Open decisions (answer at session start)
1. "R" = dedicated key (assumed) vs. a hold-LMB / RMB variant.
2. Victim half: HitReact "authored pair" entry (recommended) vs. a new GA_StrikeVictim.
3. Keep the warped heavy as the LMB lunge fallback, or R-only.
4. How many walk variants for v1 (F_5 only, or F_1..6 for variety).

Estimate ~1 day, two-thirds content. Task #17 tracks it.
