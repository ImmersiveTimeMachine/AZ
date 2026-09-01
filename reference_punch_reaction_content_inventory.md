---
name: reference_punch_reaction_content_inventory
description: "Punch/reaction content inventory (2026-08-31): hero has L/R/heavy fist clips + 5 montages (heavy has NO GA); Chalkie has 16 KnockBack clips keyed by LOCOMOTION STATE (Atk/Chase/Walk), not by hit direction, of which only 1 per variant DA is used; ZERO paired punch/reaction content — NAAT grab is the only shared-origin pair set. Verdict: jab reactions = single-role MM (content ready), hard punch = PSI (content must be authored)."
metadata:
  type: reference
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-09-01T04:15:05.686Z
---

# Punch / reaction content inventory (read off disk + CDOs, 2026-08-31)

## Hero punch side — SKEL_SurvivalMan
Source `MovementAnimsetPro` → retargeted `RTG_RM_*` / `AnimPro_*` / `LM_RM_*`:
`Fists_Punch_L`, `Fists_Punch_R` (jabs) · `Fists_Punch_Move_L/R` (moving) · `Fists_Punch_Heavy2Idle`
(the heavy) · `Idle_Punch_Move_L`.
Montages (5): `AM_Fists_Punch_L`, `_R`, **`_Heavy_L`**, `_Move_L`, `_Move_R`.
Abilities (2): `BP_GA_Punch_L`, `BP_GA_Punch_R` — **no heavy GA exists yet**.
Hero's own reaction: only `AM_Hero_Hit_L` / `AM_Hero_Hit_R` (thin).

## Chalkie reaction side — UE4_Mannequin_Skeleton (`Content/Zombie_01/Animations/`)
**16 KnockBack clips, each in Root AND InPlace (IPC) form**, keyed by the zombie's LOCOMOTION STATE:
- `Zombie_Atk_KnockBack_1..5` (5) — knocked back mid-attack
- `Zombie_Chase_1..5_KnockBack_Chase` (5) — mid-chase
- `Zombie_Walk_F_1..6_KnockBack_Walk` (6) — mid-walk
**There are NO directional (F/B/L/R) knockbacks** — only DEATHS are directional
(`AM_Zombie_Death_F/B/L/R`). Direction is not an authored axis on this pack; STATE is.
Montages wrapped so far (~10 of 16): `AM_Zombie_KB_Atk_1..5` (+`_1_Root`), `AM_Zombie_KB_Chase_1,2,3,5`.
**In actual use: ONE per variant** — `DA_ChalkieAnims_A_Standard` and `_C_Rotter` → `AM_Zombie_KB_Chase_2`
(active 1.81s, recover 0.80); `_B_Runner` → `AM_Zombie_KB_Chase_5` (active 2.15s, recover 0.45).
⇒ ~15 authored reactions sitting unused.

## ★★ THE ROOT-vs-IPC TRAP (cost one PIE, 2026-08-31)
`AM_Zombie_KB_Atk_1..5` wrap the **InPlace `_IPC`** sequences (`enable_root_motion=false`) — a montage
named like a knockback that CANNOT move a capsule. Only `AM_Zombie_KB_Atk_1_Root` (the `_Root` suffix is
the tell) and the four `AM_Zombie_KB_Chase_*` wrap `Animations/Root/` clips. Symptom when indexed:
selection works, cost is fine, and `[HitReact] capsule moved 0.0cm`. **Never add a reaction montage to a
pool without reading `SlotAnimTracks[0].AnimTrack.AnimSegments[0].AnimReference->bEnableRootMotion`.**
Second layer: the Root sequences `Zombie_Atk_KnockBack_2..5` SHIPPED with `bEnableRootMotion=false`
(only `_1` and all Chase clips were enabled) — enabled 2026-08-31 to match their siblings
(lock=False, REF_POSE unchanged), and `AM_Zombie_KB_Atk_2..5_Root` montages were created from them
(`unreal.AnimMontageFactory` with target_skeleton + source_animation works; note that duplicating a
montage would also copy the source's notifies).
⇒ `PSD_AZ_ChalkieReactions` = **9 entries, all verified rm=True**, each SamplingRange [0, 0.2].
Measured after the fix: capsule moved 30 / 96 / 85 / 34 cm (was 0.0 on every hit).

## ★★★ "PUNCH GOES THROUGH THE BODY" — MEASURED 2026-09-01 (three causes, none needing PSIA)

Measured with `AnimPoseExtensions.get_anim_pose_at_frame` + `get_bone_pose(..., AnimPoseSpaces.WORLD)`
(on a raw pose WORLD == component space; there is no COMPONENT enum). Knuckle = `middle_02_*`, which
sits **~6-8cm ahead of `hand_*`** — measuring the wrist instead is what makes this look unsolvable.
Stand-off on `BP_GA_Punch_L` CDO = **95** (NOT the C++ default 100 — read the CDO).
Victim capsule r=30, SweepSphereRadius=12.

| clip | wrist fwd | KNUCKLE fwd | contact t | valid stand-off window | at 95 |
|---|---|---|---|---|---|
| Punch_L | 75.4 | 83.7 | 0.17 | 113.7 – 125.7 | knuckle 18.7cm INSIDE |
| Punch_R | 70.1 | 78.0 | 0.18 | 108.0 – 120.0 | 13.0cm inside |
| Heavy_L | 76.6 | 82.7 | 0.34 | 112.7 – 124.7 | 17.7cm inside |

Window = [knuckle+30 (kisses capsule) .. knuckle+30+12 (sweep still reaches)]; width == sweep radius.
**COMMON window 113.7-120.0 (6.2cm) — so a single constant CAN satisfy both masters**; the fix is
~116, not a new system. (An adversarial review argued "no X satisfies both" reasoning from the header's
WRIST numbers — the knuckle offset is what creates the window. Measure the striking surface.)

**★ THE ACTUAL HEAVY BUG (found 2026-09-01 from the USER's observation "I see 2 AZ melee windows"):**
`AM_Fists_Punch_Heavy_L`'s HIT window was **0.25 → 1.95 (1.70s long)** on a punch that contacts at 0.34
and has the arm down by 0.82 — i.e. the hero was a live hit detector for 1.7s while walking 160cm
forward THROUGH the target. That is what "the punch traverses the body" is; it is not a placement error.
(1.70 also matches `RootMotionSeconds=1.7` — both were stretched to cover the walk.) Fixed to
**0.25 → 0.55**.
⚠ **TWO melee windows on a punch is CORRECT, not a duplicate.** One carries
`Event.Montage.Melee.WindowBegin/End` (the hit), the other `Event.Combat.CancelOpen/CancelClose` (the
combo-cancel/recovery window, consumed at `AZ_GA_MeleeAttack.cpp:632-638`). `DumpMontageNotifies` prints
both as "AZ Melee Window" WITHOUT the tags, so they look identical — grep the .uasset for the tag names,
or use `AddMeleeWindowNotifyState(..., bReplaceExisting=true)` which is scoped to the same BEGIN TAG and
therefore replaces the hit window while leaving the cancel window intact (verified).
Jab hit windows are 0.12→0.50 (peak 0.17) + cancel 0.50→0.80 — left alone, 0.38s is a normal window.

**Three causes, in order of size:**
1. **Heavy: 177cm of post-contact FORWARD root travel — and the warp window spreads the approach across
   all of it.** `RTG_RM_Fists_Punch_Heavy2Idle` is NOT a lunge-punch: knuckle peaks 82.7 at t=0.34 with
   only 25cm travelled, drops to −8.3 by t=0.82 (arm back), then the character WALKS to +202cm by 2.67s
   with a second small arm motion at 1.65. It is punch-then-walk-to-idle. It is wired as
   `BP_GA_Punch_L.PunchLunge_L` (used only when gap ≥ `LungeMinDistance` 110).
   Its motion-warping window is **0.0001 → 1.6501** (read via
   `unreal.MotionWarpingUtilities.get_motion_warping_windows_from_animation`), so SkewWarp distributes
   the whole gap-closing across the walk: **at the contact frame only ~14% of the approach is done**, i.e.
   the heavy contacts near its ORIGINAL gap (whiffs at long range) and keeps driving for 1.3s after.
   ⇒ Fix = move the warp window END to just past contact (~0.36) so the approach completes BY the punch,
   then release the capsule at ~0.45. **`RootMotionSeconds` ALONE CANNOT DO THIS**: the drive is
   `Max(Min(RootMotionSeconds, len), WarpWindowEnd)` (AZ_GA_MeleeAttack.cpp:342-344), so it can never cut
   below the window end. Moving that notify needs the montage editor (Notifies are protected in Python).
   Jabs travel ~0cm post-contact — heavy-only.
   Applied 2026-09-01 on `BP_GA_Punch_L` CDO (compiled+saved, verified on a fresh instance):
   `WarpApproachDistance 95 → 118`, `RootMotionSeconds 1.7 → 0.45` (the latter inert until the notify moves).
2. **Stand-off 95 vs window 113.7-120** — ~19cm too small on EVERY punch thrown from range.
3. **`Min(WarpApproachDistance, CurrentGap)` never pushes back** — a punch thrown from gap 60 stays at
   60 (knuckle ~24cm inside) regardless of the constant. Structural; needs a *bounded* back-warp
   (≤10-15cm, eased — the old unbounded Max() moonwalk is what this clamp was fixing) or distance-keyed
   clip selection (the CHT selector already keys {profile, hand, weight, movement}).
Cause (d) "victim never recoils" was REAL but is FIXED as of 2026-08-31 (see the Root-vs-IPC trap above):
knockbacks now travel 30-96cm measured.

**DECISION: do NOT use PSIA for free-flow strikes.** Reserve it for the grab and future finishers
(long, rooted, mutual, collision-carved). Reasons: a punch victim is a live Mover-simulated AI mid
crowd-slot — per-hit teleport-alignment violates the two-writers doctrine and needs a per-hit collision
carve-out; a fabricated pair (padded idle + knockback) would **freeze the victim's reaction at authoring
time, deleting the runtime pose-selected reaction built the same night**; and with no authored spatial
relationship the PSIA Origins would just be the measured stand-off wearing a schema + database + index
discipline. Shipped AAA melee (God of War, Arkham, Shadow of Mordor) warps to a per-attack ideal range,
picks the variant by distance, and sells contact with hitstop + instant victim reaction + camera impulse.

Related: [[project_motion_warping]], [[project_combat_fist_build_plan]], [[feedback_verify_never_presume]].

## Paired content: NONE outside the grab
The ONLY shared-origin authored pairs in the project are the 7 NAAT grab pairs
(`AS_NAAT_Human_*` ↔ `AS_NAAT_Zombie_*`). Hero punches come from MovementAnimsetPro and Chalkie
knockbacks from Zombie_01 — two unrelated packs, single-character clips, never authored to match.

## Verdict (matches the two-tier split)
- **Jab reactions = single-role `UPoseSearchLibrary::MotionMatch` over a KB database — CONTENT IS READY
  TODAY.** The pack's own axis (Atk/Chase/Walk) is exactly "what pose is the body in", which is what MM
  selects on; C++ picking one montage per variant is the crude version of that. Unblocked by the
  2026-08-31 work: the Chalkie now HAS a PoseHistory collector + live trajectory
  ([[project_grab_grapple_design]]).
- **Hard punch as a PSI pair = CONTENT DOES NOT EXIST.** Needs authored/mocap shared-origin pairs (hero
  heavy + matching Chalkie reaction), or hand-aligned assembly of the two packs. Authoring work, not
  code work — the driver already exists (`TryCatchSearch`).

Related: [[reference_noweapon_anim_catalog]], [[project_combat_fist_build_plan]],
[[project_grab_grapple_design]], [[feedback_posesearch_mm_mechanism_rules]].
