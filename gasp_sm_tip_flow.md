---
name: gasp_sm_tip_flow
description: GASP State Controller SM topology — 9 states + multiple Conduits/Re-Enter pseudo-states for TIP entry/chaining. Per-transition rule shapes documented.
type: reference
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# GASP State Controller — Full SM Topology

**ABP path:** `/Game/Blueprints/SandboxCharacter_Mover_ABP`
**SM name:** "State Controller" (single SM in the AnimGraph)

## States (9)

```
Idle Loop
Idle Break
Transition to Idle
Transition to Locomotion
Locomotion Loop
Transition to In Air
In Air Loop
Transition to Slide
Slide Loop
```

## Pseudo-states / Conduits (visible in transition list as source/target names)

- `Conduit` — central "Grounded routing" conduit
- `Idle -> Locomotion` — routing conduit for going from idle to locomotion
- `Locomotion -> Idle` — routing conduit for stopping
- `Re-Enter` — self-transition pseudo-states (one per Trans state, for chaining)
- `In Air` / `Grounded` / `Slide` — global routing conduits

## Transition Catalog (29 total)

### Routing conduits → Trans states
| From | To | Rule nodes | Purpose |
|---|---|---|---|
| `Idle -> Locomotion` (conduit) | Transition to Locomotion | 3 | Start moving |
| `Locomotion -> Idle` (conduit) | Transition to Idle | 3 | Stop moving |
| `(global) -> In Air` | Transition to In Air | 3 | Jump/fall |
| `(global) -> Grounded` | Conduit | 3 | Grounded entry |
| `(global) -> Slide` | Transition to Slide | 3 | Slide entry |
| `Conduit` | Transition to Locomotion | 3 | Route from grounded |
| `Conduit` | Transition to Idle | 3 | Route from grounded — **TIP entry** |

### Trans state → Loop state
| From | To | Rule nodes |
|---|---|---|
| Transition to Locomotion | Locomotion Loop | 6, 6, 11, 3 (4 transitions) |
| Transition to Idle | Idle Loop | 6, 6 (2 transitions) |
| Transition to In Air | In Air Loop | 6, 6 |
| Transition to Slide | Slide Loop | 6, 6 |

### IdleBreak fidget
| From | To | Rule nodes |
|---|---|---|
| Idle Loop | Idle Break | 3 |
| Idle Break | Idle Loop | 6, 6 |

### Self re-entry (chain transitions)
| From | To | Rule nodes |
|---|---|---|
| Re-Enter | Transition to Locomotion | 15, 13, 17, 3, 9 |
| Re-Enter | Transition to Idle | **8, 21** |
| Re-Enter | Transition to Slide | 7 |
| Re-Enter | Transition to In Air | 7 |

## Verified Rule Bodies

### `Idle Loop → Idle Break` (3 nodes)
Simple enum equality:
```
MovementState == Idle  →  CanEnterTransition
```
Drives entering the random fidget after MovementState becomes Idle.

### `Conduit → Transition to Idle` (3 nodes — TIP entry)
```
Current State Time (State Controller) > N  →  CanEnterTransition
```
(Comparator B-pin default = `3.0` but actual wired value may differ.)

This is the **canonical TIP entry** path. Conduits don't have own state time — `Current State Time` here returns the upstream state's elapsed time (Idle Loop / Loc Loop). So the rule is essentially: "after being in upstream state for X seconds, route to TransIdle".

The chooser then picks the actual TIP anim once we're in TransIdle.

### `Re-Enter → Transition to Idle` (8 / 21 nodes — TIP chain self-loop)
The 8-node and 21-node variants are the self-re-entry paths. Per user's correction, the SIMPLE rule body is:
```
Enable_TurnInPlaceSteering curve < 0.1  →  CanEnterTransition
```
NO `curve > 0` guard — that was a wrong patch I added in AZ. GASP uses the bare `< 0.1` check; the curve value defaults to 0 when not present, which (correctly per GASP) doesn't trigger re-entry from non-TIP anims because the `Re-Enter` self-transition only evaluates while ALREADY in TransIdle.

The 21-node variant adds extra checks (likely `ShouldSpinTransition` / `Tags Contains Spin_*` for direction-flip overshoot).

## Architectural Lessons for AZ

1. **GASP routes via conduits, not direct state→state for entry.** AZ should consider adding a Conduit pseudo-state for grounded routing instead of hard-wiring `IdleLoop → TransIdle`. This gives clean "decide where to go" semantics without rule duplication.

2. **`Re-Enter` is a SELF-TRANSITION pattern** — a transition whose source is the destination state itself. Used for TIP chaining and for restarting transition anims.

3. **TIP entry rule is intentionally simple.** Conduit's only check is "state time > N" — it's the chooser's job to pick the right TIP anim once in TransIdle.

4. **Re-entry check is `curve < 0.1` only.** No additional guards. The curve naturally returns 0 on idle anims, but Re-Enter only fires while in TransIdle (so the anim playing IS a TIP anim with the curve), so the `< 0.1` check is meaningful only at end of the TIP anim window.

## Quick Diagnostic Queries

To inspect any GASP transition rule from AZ:
```python
unreal.AZ_AnimBlueprintUtils.list_transitions("/Game/Blueprints/SandboxCharacter_Mover_ABP", "Idle Loop")
unreal.AZ_AnimBlueprintUtils.inspect_transition_rule("/Game/Blueprints/SandboxCharacter_Mover_ABP", "Idle Loop", "From State", "To State")
```

(`IdentifyingStateName` can be any state in the SM; the function searches for the SM containing that state name.)

## Open Questions

- Exact rule for `(global) → Grounded → Conduit` (the rule that decides the Conduit gets entered — likely "is on ground").
- The 21-node `Re-Enter → Transition to Idle` rule — needs deeper inspection to map all the spin overshoot checks.
- How the `bCanEnterTransition` priority is set on each (multiple TransIdle → IdleLoop transitions exist, ordered by priority).
