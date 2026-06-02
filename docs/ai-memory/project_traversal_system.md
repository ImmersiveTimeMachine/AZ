---
name: project_traversal_system
description: "Foundational vision (2026-06-02) — unified obstacle-aware movement: jump input traces ahead; if an obstacle fits a Chooser traversal entry → RM action (RMAction + AnimationWarping to measured geometry); else → physics jump. The trace→chooser→warped-RM-action pattern is the base for all contextual game movement."
metadata: 
  node_type: memory
  type: project
  originSessionId: 787f844b-69e1-48c0-8b39-9a9264829d57
---

# Obstacle-aware movement / Traversal system (vision — base for contextual game movement)

**Decision/direction 2026-06-02 (design pending — "we will think about this"):** unify jump + traversal under one obstacle-aware system.

## The model
On a jump/traversal input:
1. **Trace** the geometry ahead (front edge, top height, depth, floor-behind) — same as GASP.
2. **A Chooser (CHT) decides**: do the traced obstacle dimensions **fit** a traversal entry (vault / hurdle / mantle / climb / slide)?
   - **Fits → RM action:** play the matching authored root-motion montage, with **AnimationWarping** targets set to the *traced edges* (hand-contact = obstacle top-front, landing = floor-behind), driven by **`UAZ_PawnMovementMode_RMAction`** (no gravity/floor-snap; warped RM is authoritative for the action). No flat-ground assumption — everything is warped to the real geometry.
   - **No fit → physics jump:** the [[project_physics_jump_plan]] path (launch + Falling mode + cosmetic anim + MM land). Adapts to unknown terrain because nothing is authored about it.

## Why this is the right split (recap)
- **Free space (unknown terrain)** → physics: gravity + floor contact adapt to any height.
- **Specific obstacle (measured)** → RM + warping: precise hand/foot contacts placed on the real edges; baked RM is correct because it's warped to traced dimensions, not assumed.
This is GASP's exact architecture (CHT_Traversal chooser picks an action by traced obstacle params; no match → normal jump). We already have both legs: `RMAction` mode + `AnimationWarpingRuntime` dependency (the RM/warp leg) and `Falling` mode (the physics leg).

## Why it's the FOUNDATION ("base for all next game physics")
The pattern **trace → chooser-select-by-context → warped-RM-action (or physics fallback)** generalizes to all contextual/environmental movement, not just jump:
- now: vault, hurdle, mantle (low/high), climb, slide-under, step-over;
- later: cover entry/peek, environmental interactions (open/break, push), getups, contextual takedowns.
The Chooser (CHT) is the single decision surface; `RMAction`+warping is the execution surface for geometry-matched actions; physics is the fallback for free motion. Build it once, extend by adding CHT entries + warped montages.

## Status / sequencing
- **VISION only** — not yet designed in detail.
- Immediate next build = the **physics jump** ([[project_physics_jump_plan]]) — it's the fallback leg AND the simplest piece to land first.
- Traversal (the RM-action leg + the trace + CHT_Traversal) is the bigger follow-on; CHALK's urban chases (vault barricades, mantle dumpsters, climb fire escapes) make it gameplay-relevant — see [[project_lore_chalk]].

See [[project_physics_jump_plan]], [[project_jump_system_status]], [[project_root_motion_mode]], [[project_v2_architecture]], skill `gasp-parity-reference` (GASP Traversal).
