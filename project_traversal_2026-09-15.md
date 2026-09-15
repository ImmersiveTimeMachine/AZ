---
name: project_traversal_2026-09-15
description: "★★★ TRAVERSAL RESUME POINT (2026-09-15). Mantle + hurdle + CLIMB all shipping on the Mover hero through one shared executor. Holds the warp-point contract that fixed the hurdle, the measured per-clip data, the style-as-preference change, and the ranked open list (moving-exit restart is next)."
metadata: 
  node_type: memory
  type: project
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-15T01:18:55.520Z
---

# Traversal — mantle / hurdle / climb, state at 2026-09-15

Supersedes [[project_mantle_traversal_2026-09-14]] for status. The work orders still govern intent:
[[project_mantle_handoff_spec]], [[project_hurdle_spec]], [[project_climb_spec]].

## Status: all three actions run

`UAZ_GA_Traversal` executor + thin `UAZ_GA_Mantle` / `UAZ_GA_Hurdle` / `UAZ_GA_Climb` wrappers.
Ability classes are the **native C++ classes directly** (`/Script/AZ.AZ_GA_Climb`), listed in the pawn's
`startup_abilities` — there are no BP wrappers.

One session's measured outcome: 20 traversals (9 hurdle, 8 mantle, 3 climb), every rejection explained,
zero silent paths.

## ★ THE WARP-POINT CONTRACT — this was the hurdle bug

Every GASP traversal clip is authored with **the obstacle's front face at the animation origin (y=0) and
its top at the obstacle height** (mantle's calibrated warp point states this literally: `(0,0,100)`).

`WarpPointAnimProvider::Bone` with the missing `attach` bone returns identity, which does **not** merely
disable the window — it makes `CachedOffsetFromWarpPoint` identity so **every window with the same target
name resolves to the SAME world root transform.** Every hurdle clip has TWO FrontLedge windows whose
authored root ends differ enormously (first still on the ground ~36 cm out, second airborne at the face),
so both were driven onto one point: the character was yanked to the takeoff apex up to 0.4 s early and held
frozen there. That float-at-the-wall was the "not realistic" the user reported.

Fix: `Static` provider with a per-clip measured warp point, so each window derives its own destination from
its own window-end root transform.

- **Hurdle** — warp point `(0,0,0)`, i.e. the face at GROUND level; target = front face at ground. Ground-
  anchored because you jump from the ground. 32 windows across 20 montages.
- **Climb** — warp point `(0, faceY, 247.9)`, i.e. the LEDGE; target = the real front top edge. Ledge-
  anchored because a climb is a reach and the hands must meet the edge. Five clips sit on the convention;
  `Climb_2_5_Run_Neutral_R`'s origin is genuinely shifted **−59.3** — measure per clip, never assume.
- **Mantle** — unchanged, `Static (0,0,100)` / `(0,-50,100)` on the stand variants.

Measure the face plane as `max(bone.y)` over bones **below** the top: the body cannot pass through the wall.

## Measured per-clip data (all baked into the pawn CDO)

- `AuthoredClearHeight` (renamed from the misleading `AuthoredApexRise`) = lowest BODY bone while crossing
  the barrier span y 0..20. **99–111 cm on clearing clips against their own 100 cm barrier** — they graze.
  Step-ons read 108–126 only because their lowest point there is the foot PLANTING on it.
  The root track's peak (97–99) is NOT a clearance figure and must never be compared to an obstacle height
  — see [[feedback_animpose_root_motion_double_count]] for how that error was made and caught.
- `ApexLift` = `max(0, height + HurdleApexClearance − AuthoredClearHeight)`, **clearing clips only**
  (`RequiredTopSupport <= 0`). Lifting a step-on lifts its plant off the wall. Applied to a separate
  `FrontLedgeApex` target carried only by windows whose authored root end is airborne (>30 cm), so the
  grounded run-up windows are not raised.
- Climb clips **contain the whole action** — all six end standing at root z 247.9 with both feet on the
  platform, 50–64 cm past the face. No hang phase, no continuation asset needed. ("Start" in the source
  filename does not mean a partial action.)

## Bands and gates

mantle **75–125**, climb **205–285**, hurdle **≤110** (`MaxHurdleHeight`, added because the restored lift
formula would otherwise ask for 160 cm of levitation on a 2.5 m wall). Disjoint by construction; the gap
between 125 and 205 is an animation coverage limit, not a bug.

`bTopStandable` is one trace shared by mantle and climb; height alone classifies which action it qualifies
for. `UsableTopSupport` is now **measured** (`MeasureTopSupportDepth`, 10 cm steps from the lip) instead of
`FLT_MAX` whenever hurdle is illegal — which was always, for climb, making its baked 59–74 cm requirement
dead data.

## Selection rules that matter

- **Style is a PREFERENCE, not a gate** (penalty 500, larger than any legal error). As a gate, the
  character's carriage silently decided which obstacles existed: Relaxed had no thin-wall clip inside
  61 cm while Neutral enters from 89.
- **Foot is a preference** too (penalty 40); `Unknown` counts as a match.
- Approach falls back ONE family (Run→Walk, Walk→Stand) — except **climb, which may fall two** (Run→Walk→
  Stand): you cannot carry speed through 2.5 m of wall, so braking into a standing reach is what happens.
- Entry samples are capped at the clip's first warp window. Entering mid-window **skips the warp
  registration entirely** (UE does not retroactively fire NotifyBegin), so this is load-bearing, not
  conservatism. A clip whose window opens at t=0 therefore has NO legal entry — fix the asset, not the
  selector. That is what the Relaxed run mantles needed (window moved 0.000→0.330, 1 sample → 7).

## Open, ranked

1. **The ~0.62 s moving-exit restart.** Traversing makes the locomotion SM return `IdleLoop`, so a moving
   exit at 655 cm/s selects `RunFwdStart` at zero and reaches the loop 0.617 s later. Oldest open item;
   user chose "play only the start then hand back to the loop" long ago. Needs an explicit successor
   context from the actual outgoing pose/velocity/intent.
2. **No ascent-path validation.** Only the destination is checked; nothing validates the crossing volume,
   and the whole target primitive is collision-exempt including any overhang.
3. Startup is not a transaction; only root motion is generation-scoped (warp names, collision exemption,
   pending mode, PendingCandidate are not).
4. Thin 20 cm wall from standing has no authored action (all four standing hurdles are step-ons needing
   34–44 cm of top). `LevelBlock_Traversable_StepOn45` at **(−4619, 4180)** was added to exercise the
   step-ons — 45 cm deep on purpose: `LandingInset` is 50, so mantle's probe falls past the far face and
   the step-ons become the only legal answer. **Still untested.**
5. Surface-provider boundary so fences etc. can be traversable (`no-ledge … splines=0`); co-op request
   contract; camera sign-off. All per the external review in
   `docs/design-briefs/traversal-affordances-review-and-proposal.md`.

## Traps already paid for

[[feedback_animpose_root_motion_double_count]] · [[feedback_held_input_retries_ability]] ·
[[feedback_motionwarping_warppoint_provider]] · [[feedback_montage_blend_profile_cross_skeleton]] ·
[[feedback_python_save_only_if_dirty]]. Also: `FAnimNotifyEvent` structs read from Python are copies, so
trigger times cannot be set in place — remove by name and re-add with
`add_animation_notify_state_event_object`, reusing the SAME notify-state object to keep its modifier
settings. And `unreal.Rotator(roll, pitch, yaw)` — passing `(0,90,0)` for yaw tips the actor on its side.
