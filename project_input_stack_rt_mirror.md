---
name: project_input_stack_rt_mirror
description: AZ input stack — full RT mirror of IMC/IAs chosen over GASP-analog trim. 21 AZ_IA_RT_* IAs + AZ_IMC_RT_PawnInputs live in /Game/AZ/Blueprints/Input/InputActions/RT/. Legacy AZ_IMC_PawnInputs + 18 AZ_IA_* IAs orphaned pending Phase 9 decommission.
type: project
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# AZ Input Stack — Full RT Mirror (decision 2026-04-22)

## What got built

`/Game/AZ/Blueprints/Input/InputActions/RT/` — full mirror of the legacy input stack:

- **1 IMC:** `AZ_IMC_RT_PawnInputs` (29 mappings, all referencing RT IAs)
- **21 IAs with `AZ_IA_RT_` prefix:** Move, Look, Jump, Sprint, Walk, Strafe (locomotion) + Aim, Crouch, Run, ChangeFireMode, ChangeShoulder, FireWeapon, HoldBreath, Interact, Lethal, Melee, Reload, SecondaryWeapon, ToggleWeapon, TogglePerspective, Weapon_1, WeaponAccessory

**Registration:**
- `BP_AZ_PlayerController.InputMappingContext = AZ_IMC_RT_PawnInputs`
- Legacy `AZ_IMC_PawnInputs` is **not** registered by the PC anymore (but still exists on disk).
- `AZ_BP_HeroPawn` CDO — all 6 IA slots (Move/Look/Jump/Sprint/Walk/Strafe) point at RT copies.

## Options considered (user picked Option 3)

1. **GASP-analog trim** — keep only the 6 locomotion IAs in RT folder, stack legacy + RT IMCs on possess. Cleanest GASP parity but requires deleting 15 already-created RT IAs.
2. **Minimal revert** — delete RT folder entirely, add 3 new bindings to legacy IMC. Smallest diff, breaks the "new stack" separation model.
3. **Keep full mirror (chosen)** — accept duplication, plan to decommission legacy IMC + legacy IAs in Phase 9.

**Why option 3:** separation is the top priority — RT is the "new stack" that can evolve independently without mutating/breaking any existing system references (GAS abilities, weapon BPs, inventory HUD, etc. that still reference legacy IAs).

## Orphaned legacy assets (not deleted, not referenced by pawn/PC)

- `AZ_IMC_PawnInputs` — 29 mappings, unused by pawn's active stack. May still be referenced by other systems.
- 18 legacy `AZ_IA_*` — still referenced by weapon BPs / GA_* abilities / UI / etc. Do NOT blanket-delete. Per-IA migration needed.

## Phase 9 (GAS reintegration) — plan for this decision

When Phase 9 lands, decide per-IA whether to:
- **Migrate reference** — update the GAS ability / BP / UI to read the RT IA. Delete the legacy IA.
- **Keep legacy** — for IAs that only the legacy IMC-consuming systems use, leave both in place.

Likely migrations:
- `GA_Aim` reads `AZ_IA_Aim` → switch to `AZ_IA_RT_Aim`
- `GA_Shoot` / `GA_Reload` → RT copies
- `GA_Jump` → `AZ_IA_RT_Jump`
- `GA_Crouch` → `AZ_IA_RT_Crouch`
- `GA_Interact` → `AZ_IA_RT_Interact`

Weapon-local inputs (IA_FireWeapon, IA_Reload, etc.) are added via `AZ_Weapon::FireMappingContext` with a separate IMC (`FireMappingContext` — not the pawn IMC). That path is independent and already works.

## Divergence from GASP

GASP's `IMC_Default` is tight (~9 locomotion IAs). AZ's RT IMC is wide (29 mappings, 21 IAs) because we duplicated the whole container. The semantic difference doesn't affect pawn behavior — just means the RT folder is a maintenance surface of 21 assets instead of GASP's ~9.

Accept this divergence; the pawn's behavior is identical to GASP's path.

## Behavioral tweak in C++ (Get_Gait)

Along with this input rewire, `Get_Gait` was inverted from GASP's default:
- GASP: default = Run, Walk button explicit, Sprint button = fast.
- **AZ: default = Walk, Sprint button = fast (Shift).** Walk button still maps to Walk (no-op vs default; kept for force-walk overrides).

See `AZ_HeroPawn.cpp::Get_Gait` comment block for rationale.
