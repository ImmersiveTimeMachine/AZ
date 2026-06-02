---
name: reference_noweapon_anim_catalog
description: Complete catalog of 191 NoWeapon anims at /Game/AZ/Assets/RTG/NoWeapons/RootMotions/ — categorized by locomotion group for Chooser table population
type: reference
originSessionId: f6181671-d4a5-4b82-954f-4f2f5396f92f
---
# AZ NoWeapon Animation Catalog

**Path:** `/Game/AZ/Assets/RTG/NoWeapons/RootMotions/`
**Total:** 191 animations on SurvivalMan skeleton (prefix: `LM_RM_`)

## Categories (key groups for Stand Stopped chooser chain)

**Stand Idle** (15): `Idle`, `Idle2`-`Idle6` (5 break variants), `Idle2Fists`, `Idle2Crouch`/`_new`, hit/knockdown reactions
**Stand Turn** (4): `TurnLt90_Loop`, `TurnLt180`, `TurnRt90_Loop`, `TurnRt180` — FULL 90/180 L/R
**Crouch↔Stand** (4): `Idle2Crouch`/`_new`, `Crouch2Idle`/`_new`
**Walk Stand** (20): FwdLoop/BwdLoop, 7 directional starts (90/135/180 L/R + Bwd), FwdStop/BwdStop LU/RU, ArchLoop L/R, Lean L/R, DoorWalk
**Run Stand** (24): FwdLoop/BwdLoop/LtLoop/RtLoop, 7 directional starts, FwdStop LU/RU, FwdTurn180 L/R LU/RU, ArchLoop L/R, Lean L/R, RunStrafe 45/135 L/R
**Sprint** (1): SprintFwdLoop1 only (no sprint stop)
**Walk Strafe** (12): Left/Right + 45/135 loops, starts, stops LU/RU
**Crouch** (33): Idle + Turn90 L/R, WalkFwd/Bwd/Lt/Rt with starts/stops/loops
**Jump/Fall/Land** (22): IdleStart, Walk/Run starts LU/RU, FallingLoop, Idle/Walk/Run landings (light+hard), Land2Walk/Run transitions
**Slide** (1): Slide loop only (no exit)
**Interaction** (6): ButtonPush LH/RH (+ 90°), Climb 1m/2m

## Key coverage notes for GASP chooser migration
- ✅ Turn-in-place: HAVE 90/180 L/R (was previously marked as gap!)
- ✅ Walk/Run stops with foot-phase (LU/RU)
- ✅ Directional starts at multiple angles (90/135/180)
- ⚠ No sprint stop (fallback to RunFwdStop)
- ⚠ No slide exit (fallback to Idle)
- ⚠ No backward run stops (only forward; backward uses forward variant as approx)
