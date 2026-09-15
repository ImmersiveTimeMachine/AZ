---
name: project_climb_spec
description: "★★★ USER WORK ORDER for high-ledge CLIMB (Neutral Climb_Start_2_5 set, 2026-09-14, 11 points): verify whether the clips contain the whole climb or end hanging, reuse UAZ_GA_Traversal, climb-specific geometry, explicit action priority across mantle/hurdle/climb, distance-aware entry, calibrated warp refs, single traversal ownership across phases, safe handoff, input/equipment policy, camera capture, and the 250cm L_001 blocks for validation."
metadata: 
  node_type: memory
  type: project
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-14T22:00:19.912Z
---

# HIGH-LEDGE CLIMB — user work order (2026-09-14)

Series: `/Game/AZ/Assets/GASP/AZRTG_GASP_AM_M_Neutral_Traversal_Climb_Start_2_5_{stand,walk,run}_F_{L,R}foot`
(**Neutral only** — there IS also a Relaxed set on disk, but the order specifies Neutral.)
Extend the existing traversal system; **preserve working mantle/hurdle behaviour.**
Sits after [[project_hurdle_spec]]; shares the executor built in [[project_mantle_traversal_2026-09-14]].

## 1. Verify what the animations actually contain — FIRST
Inspect every montage and referenced sequence: entry pose, root trajectory, hand contacts, warp windows,
notifies, sections, final pose. ★ **Establish whether each montage is the COMPLETE climb onto the platform
or only the approach/reach/grab phase. "Start" in the filename does not decide this.**
If a clip ends HANGING, identify the compatible continuation and implement an explicit sequence. **Do NOT
end the ability and force Walking while the character hangs.** Report any missing continuation asset before
claiming the feature works. Intended first feature = grounded approach → climb onto a supported high
platform. Free wall climbing, ladders, shimmying, indefinite hanging and mid-jump ledge catches are
SEPARATE features.

## 2. Reuse the shared executor
`UAZ_GA_Traversal` + `FAZ_TraversalRequest` exist; verify their current integration/build status and reuse.
Add climb as action-specific detection/selection/request data, or a thin `UAZ_GA_Climb` subclass. Shared
ownership of warp targets, Traversing mode, root-motion driving, collision exemptions, callbacks, cleanup
stays in the base. **Do not copy the mantle ability.** Preserve Blueprint references and concurrent
uncommitted work.

## 3. Climb's own geometry requirements
Authored `LevelBlock_Traversable` actors first, through the existing provider. Climb covers ledges ABOVE the
validated mantle range — ★ **do NOT raise mantle's global max height to make these clips selectable.**
Measure supported heights and approach distances **from these animations**; `_2_5` names the authored
family, it is not proof arbitrary nearby heights are safe. Validate: ledge height above the character's
ACTUAL supporting feet; reach distance, facing, lateral alignment; usable ledge width for the hand
contacts; body clearance through ascent AND pull-up; headroom and standing support on top; a clear
destination capsule. **A high thin wall with no usable platform is not a valid climb target.**

## 4. Explicit action selection
Geometry decides legality: low supported platform → **mantle**; crossable obstacle with validated far-side
landing → **hurdle**; higher reachable ledge with supported platform on top → **climb**. Resolve overlapping
valid ranges with **configured action priorities + movement context**. ★ **Never let multiple abilities
respond independently to the same Jump press.**
Climb is **Neutral explicitly**; the mantle's default **Relaxed** setting must not make the Neutral-only
climb set unavailable.

## 5. Suitable ENTRY, not just a speed category
Reuse the distance-aware entry work: pick montage + legal start time from distance, actual approach motion,
current input and outgoing pose/foot phase. ★ **Inspect foot semantics — previous traversal families used
inconsistent naming** (mantle stand vs walk/run were inverted; one hurdle clip had a BackLedge window
without "V2" in its name). Stand/walk/run must enter naturally without replaying approach steps on a close
press, but **never skip required launch, hand-contact or warp setup just to enter faster.** 1× playback.
Reject an unsuitable fit rather than crushing metres of authored approach into centimetres.

## 6. Calibrate the climb's warp references
Inspect the exact target names and providers on THESE montages — **do not assume the same target count or
offsets as mantle/hurdle.** The MetaHuman skeleton has no `attach` bone; apply the established calibrated
Static replacement where needed, measured from the climb clips themselves
([[feedback_motionwarping_warppoint_provider]]). Verify skeleton compatibility of sequences, montages AND
blend profiles ([[feedback_montage_blend_profile_cross_skeleton]]). Use gameplay-owned copies and the
existing **FullBody** path. Register every target BEFORE its first active window. Keep physical contact
anchors distinct from the final capsule destination.

## 7. Climb owns movement and body contacts
Traversing mode with appropriate gravity/root-motion behaviour; ★ **do not inherit the jump's
apex-to-Falling handoff during a committed pull-up.** Capsule stays under Mover. Preserve collision with
unrelated geometry; scope any exemption to the validated target. Suppress conflicting locomotion
root-motion requests, cosmetic obstacle reactions and inappropriate procedural foot planting while
climbing. **Do not reuse the grab system's paired-hand targets** without a separate verified contact
contract. If multiple animation phases are needed, keep **ONE continuous traversal ownership context** —
never briefly return to Walking between reach and pull-up.

## 8. Release only when the body can safely hand back
Author completion/handoff from the actual animation and geometry. ★ **Root reaching ledge height is
insufficient** — the capsule must be sufficiently ONTO the platform, contacts complete, and normal collision
safe to restore. Settled standing exit → smooth idle. Moving exit with held input → preserve horizontal
momentum and match into locomotion **without an Idle → Start → Loop restart**. Released input → appropriate
settle/stop. Lost support → falling/interruption path; **never report unsupported completion as a
successful grounded climb.**

## 9. Input, equipment and interruption policy (unchanged)
Jump is the single contextual input owner: success consumes the press; no matching traversal permits the
normal jump under its existing rules; an already-committed reaction/action consumes or rejects the press
with **no delayed traversal**. Climb is committed against ordinary input release. Death, grabs, destroyed
targets, genuine obstruction → controlled recovery. Armed entry = **holster → climb → draw the same weapon
relaxed**, revalidating the candidate after holstering, preserving inventory/magazine identity, requiring
fresh aim/fire input afterward.

## 10. Camera during ascent
The reported camera problem happens **during the climb onto the barrier**. Capture actual capsule, rendered
body, spring-arm collision/lag and final camera motion. **Do not change general camera smoothing** on the
basis of a fast root trajectory or the separate ordinary-jump landing anomaly. Fix unsuitable entry warping
first, then address any verified remaining camera behaviour.

## 11. Validation geometry
`LevelBlock_Traversable24` and `25` ≈ 250 cm high — recheck current placement and clearance.
★ `LevelBlock_Traversable13` has an **elevated base**: compute climb height from the ACTUAL approach
surface, not world Z or mesh height. Include too-high ledges, narrow tops, blocked headroom, close and far
presses, both feet, all three approaches, interruption recovery — and **confirm mantle/hurdle selection
still works afterwards**.
Complete build and asset read-back checks. **No automated tests; do not start PIE/editor tests without
asking.** Provide concise manual checks, then inspect the user's logs.
