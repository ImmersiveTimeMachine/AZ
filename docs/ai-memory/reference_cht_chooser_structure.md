---
name: reference_cht_chooser_structure
description: Structure and row patterns for CHT_AZ_CharacterAnimations and the source GASP CHT_MoverCharacterAnimations — input columns, IdleBreak routing, S_ChooserOutputs schema
type: reference
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# CHT Chooser Structure (AZ + GASP source)

## Asset paths
- **AZ:** `/Game/AZ/Blueprints/Animation/MotionMatching/CHT_AZ_CharacterAnimations`
- **AZ legacy:** `/Game/AZ/Blueprints/Animation/MotionMatching/CHT_AZ_CharacterAnimationsOld`
- **GASP source:** `/GameAnimationSample/Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData/CHT_MoverCharacterAnimations` (372 rows)

## Input columns (both)
- `E_Gait` (Walk, Run, Sprint)
- `E_Stance` (Standing, Crouching)
- `E_MovementMode` (OnGround, InAir, …)
- `E_ExperimentalStateMachineState` (IdleLoop=0, LocomotionLoop=1, InAirLoop=2, TransitionToIdle=3, TransitionToLocomotion=4, TransitionToInAir=5, IdleBreak=6, SlideLoop=9)
- `E_MovementDirection` (F, B, LL, RL, LR, RR)
- `S_ChooserOutputs` (output struct passed-through)

## Output (both)
- Array of `UAnimationAsset*` (then optionally MotionMatch'd via `bUseMM`)
- `S_ChooserOutputs` populated: `bUseMM`, `StartTime`, `BlendTime` (default 0.2), `BlendProfile`, `Tags`, `MMCostLimit`

## IdleBreak rows (per memory architecture only — exact wiring not enumerated)

Routing key: `E_ExperimentalStateMachineState == IdleBreak (6)` + `E_Stance` filter.

Three anim-groups expected (matching the 23 retargeted assets):

| Group | Stance | Variant | Count | Anim naming (RTG_) |
|---|---|---|---|---|
| Neutral Stand | Standing | "Neutral" rotation mode | 6 | `RTG_RM_M_Neutral_Stand_Idle_Break_v01..v06` |
| Relaxed Stand | Standing | "Relaxed" rotation mode | 12 | `RTG_RM_M_Relaxed_Stand_Idle_Break_v01..v12` |
| Neutral Crouch | Crouching | — | 5 | `RTG_RM_M_Neutral_Crouch_Idle_Break_v01..v05` |

**Open question:** which column distinguishes Neutral vs Relaxed in standing? Likely either:
- `E_RotationMode` (OrientToMovement vs Strafe vs Aim) — but our enum doesn't include "Neutral" / "Relaxed" naming
- A `BoolColumn` like `bIsRelaxed` based on the character's RotationMode tag set
- Or three separate rows that each output a single-stance array, and Chooser picks by Stance only (lumping Neutral+Relaxed standing into one ~18-anim array)

Verify by inspecting the GASP chooser rows under IdleBreak.

## Row anatomy (chooser concept)

Each row is a tuple of:
- **Cell values** for each input column (or "Any" wildcard)
- **Output anim array** (the AnimationAssets to consider)
- **OutputStructColumn** populated values for `S_ChooserOutputs`

When the chooser evaluates, it iterates rows top-to-bottom; the first row whose every column matches the current state is selected. For arrays, downstream MotionMatch picks the best per-frame.

## Pattern for adding new break-anim rows

If GASP has 3 distinct rows for IdleBreak (Standing-Neutral / Standing-Relaxed / Crouch):
1. Locate the IdleBreak section in CHT_AZ_CharacterAnimations.
2. For each GASP row, replace its anim-array references with the matching `RTG_RM_*` retargeted assets.
3. Keep input column values + S_ChooserOutputs identical to GASP (BlendTime, Tags, etc.).

If GASP routes by Stance only (2 rows total):
1. Standing row: array of all 18 standing breaks (6 Neutral + 12 Relaxed).
2. Crouching row: array of all 5 crouch breaks.

## C++ utility for chooser editing
- **AZ_ChooserUtils** — has helpers for chooser table config, columns, flat/nested rows, database population (per `MEMORY.md` line 86). Use this when scripting row additions instead of hand-editing in BP editor.
