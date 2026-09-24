---
name: project-gamepad-analog-gait-mismatch
description: "★★★ NEXT SESSION STARTS HERE (2026-09-22): a partly-pushed gamepad stick breaks locomotion animation — crossing legs under sprint, wrong clips at low speed. Gait comes from TAGS while speed comes from the ANALOG magnitude, so the two disagree."
metadata: 
  node_type: memory
  type: project
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-23T03:37:57.489Z
---

# Gamepad stick: small deflection wrecks the locomotion animation

**Reported by Artur 2026-09-22, to be the first task of the next session.** Symptoms, his words:

- push the stick a little forward → the character crawls, and the animation that plays is wrong for it;
- **start a sprint with the stick barely pushed → the legs cross over**, plus other artefacts;
- his own read: the stick should probably just be **one value**, not a continuum of slower/faster.

## The mechanism, already traced (do not re-derive)

Gait and speed come from two different places and nothing reconciles them.

**Gait is chosen from GAMEPLAY TAGS only** — `AZ_PawnMoverHeroCharacter.cpp`, the
`FAZ_MoverCustomInputs& CustomInputs` block (~line 1305):

```
if      (!bStrafe && !bAiming && Movement.Sprinting) Gait = Sprint;
else if (Movement.Running && !bThrowableReady)       Gait = Run;
else                                                 Gait = Walk;
```

The walking mode's `ResolveGait` maps that enum to WalkSpeed / RunSpeed / SprintSpeed and to the clip set.

**Speed comes from the ANALOG MAGNITUDE**, which survives untouched into
`CharacterDefaultInputs.SetMoveInput(EMoveInputType::DirectionalIntent, WorldMove)` (~line 1073) — verified
2026-09-21 while doing the gamepad work.

So a barely-pushed stick with the sprint tag held asks for **the sprint clip set at walking velocity**.
Motion matching / the chooser then serve strides authored for ~600 cm/s against a body travelling a
fraction of that, and the feet have nowhere to go — that is the crossing. On a keyboard this can never
happen: WASD is digital, magnitude is always 1, and the tag and the speed always agree.

★ Note the asymmetry this creates: **the default gait is WALK**, so on a pad LB must be held almost
permanently just to move at a normal pace.

## ★ CORRECTION 2026-09-22 (second session): GASP does NOT select clips by measured speed

An earlier version of this note claimed GASP avoids the mismatch because MM follows velocity. **Wrong.**
GASP's own `Get_Gait` (see [[gasp_pawn_bp_full]] line ~168) reconciles the INPUT: `DDCVar.AnalogInputStyle`
0 = sprint button → Sprint else Run; 1 = stick magnitude > 0.8 → Run else Walk. GASP also makes the input
agree with the label. The Mover walking mode here already mirrors GASP's ceiling (`MaxSpeedOverride` by gait,
`AZ_PawnMovementMode_Walking.cpp` ~line 116); the stick scales DirectionalIntent under that ceiling.

`CHT_v2_CharacterAnimations` c2 filters rows by the gait LABEL, and `AZ_MoverAnimInstance.cpp` ~line 675
reads the gait from INTENT **on purpose**: measured speed is ~0 on a run start's first frame and would pick
walk rows; it also makes walk→run flip immediately. So "select clips by measured speed" is NOT a cheap
repair — it would regress starts and gait changes. Dropped.

## 2026-09-23 run→walk felt abrupt (Artur: W+Shift, release Shift = "очень резко")
Cause: `AZ_BP_PawnMoverHero_MHC` walking mode had `GaitChangeDeceleration = 2000` (C++ default and GASP = 300)
→ 450→165 cm/s in ~0.14 s. Set to **700** (~0.4 s) by script on the BP mode template; user compiles + saves the
hero BP (scripted compile of this pawn BP once hung the editor). Anim swaps to walk rows on intent immediately,
so a much softer decel would show walk clips at run speed — 700 is the compromise.
Artur then: capsule slides, anim still snaps. Fix (Live Coding 03:37 UTC, `AZ_MoverAnimInstance.cpp` gait block):
`ChooserContext.Gait = max(intent, speed band)`, band thresholds = mid(Walk,Run) / mid(Run,Sprint) from the
walking mode (weapon profiles included) — the CMC `SelectionGait` rule. Starts unchanged (band Walk at 0 speed).
Next option offered: GASP Run→Walk / Sprint→Run transition clips exist for UNARMED only
(`/Game/AZ/NoWeapons/RT/RT_NWP_M_Neutral_Transition_*`); rifle pack has none.

## ✔ DONE — Artur tested on the pad 2026-09-23: "всё работает". Implemented 2026-09-22 (option 1)

`ProduceInput_Implementation`: `TransformVector(CachedMoveInputIntent.GetSafeNormal2D())`. Normalised THERE,
not in OnMoveTriggered, because `AttackCancelInputDeadzone` (0.25) reads the raw deflection so a drifting
stick cannot cancel an attack. Mover already clamps intent to 1 (MovementUtils), so keyboard is unchanged.
IMC dead zone left at 0.15 — raise it only if Artur reports accidental walking. Live Coding: succeeded.
Uncommitted.

## My recommendation (2026-09-22)

Option 1: keep the stick's DIRECTION analog, force its MAGNITUDE to 1 once past the dead zone, in
`AAZ_PawnMoverHeroCharacter::OnMoveTriggered` (raw stick → `CachedMoveInputIntent`, before
`MovementCapability->ConstrainIntent`, so the wall-slide scaling still applies). The pad then behaves exactly
like WASD — the configuration every clip set, start/stop and traversal clearance was validated against.
Option 2 rejected by me: a lightly tilted stick under Walk still crawls on walk clips authored for 165 cm/s,
which is Artur's FIRST symptom. Likely raise the Move dead zone a bit, since any tilt past it now = full walk.

Industry practice found while researching: many third-person games simply **force full deflection while
sprinting** — you cannot sprint slowly — and the common UE community recipe is two fixed states (slight
tilt = walk, full tilt = run). Both are variants of "make the input agree with the label".

## Options (recommendation above)

1. **Normalise the pad's move magnitude to 1** — Artur's own suggestion. Speed then comes only from the
   gait buttons, exactly as on the keyboard, and tag and speed can never disagree. Loses analog finesse.
2. **Clamp the magnitude per gait** — free stick under Walk, forced to 1 under Run/Sprint. Keeps slow
   creeping where it reads well, removes the mismatch where it breaks.
3. **Derive the gait FROM the magnitude on a pad** (slight = walk, full = run, L3 = sprint), the console
   standard. **Artur considered and rejected this on 2026-09-22** — he prefers keeping the run button.
   Do not re-propose it without new evidence.

Existing dead zone on the Move mapping: `InputModifierDeadZone` radial, lower 0.15 / upper 0.98, on
`Gamepad_Left2D` in `AZ_IMC_RT_PawnInputs`.

Related: [[project_v2_locomotion_progress]], [[feedback_mover_mode_state_not_rollback_safe]],
[[feedback_posesearch_mm_mechanism_rules]], [[project_input_stack_rt_mirror]].
