---
name: project-cryengine-doors-throws-rpg
description: "CryEngine AZ night 2026-10-19: doors (K08), distraction throws (stone/bottle), RPG core phase 1 - built and Rider-verified; where the code lives, how to test, the traps"
metadata:
  node_type: memory
  type: project
  originSessionId: 8f8d09b9-3f5e-408a-8d76-c4e4c3910763
  modified: 2026-10-19T05:59:28.580Z
---

Night of 2026-10-19 (Artur asleep, "работайте без меня… Вперёд", one cross-review per design). Session cryengine-gamesdk.
Plans: C:/CryEngine/AZ/docs/implementation/doors-and-distractions-plan.md (A-DOORS01 approved by MCP/main) and
rpg-core-design.md v03 (A-RPG01-REVIEW by claude-traversal: cross-DLL static blocker -> core lives in Game.dll).

**Doors (D1, done):** Code/World (DoorComponent, DoorSystem, WorldDoors.h, WorldTunables.h), own uber AZ_uber_world.cpp.
- Doors come from <level>/leveldata/Doors.xml; Adopt="1" hides the matching level brush (same cgf, same pose) and the
  entity draws it, so codex-katimavik's layer stays untouched. K08 door = 3 brushes adopted at 0.000 m.
- Prompt via IAZInteraction owner "AZDoors"; E / pad Y; reach = handle OR nearest leaf point (an outward-open leaf
  carries its handle away); sweep-stop never pushes; AZSave section "AZWorld".
- Chalkies: claude-npc chose a query, not navmesh markup: DoorSystem registers AZ::Npc::SetClosedDoorQuery (Code/NPC/NpcDoors.h).

**Throws (T1, done):** CHALK hunks in Codex files (ammo class AZDistraction, <distraction> block in CAZParticleGrenade),
items GameSDK.Stone/GlassBottle + GameSDKAmmo.stone/glassbottle (incendiary two-record model), recovery through the new
IAZInventoryPlugin::SpawnWorldPickup. Data: Assets/Characters/AZPlayer/Weapons/Distraction/V1, stone mesh by the import session.
Open: one combined cell for AZ Stone/Bottle (TASK 23:04 point 2), crafted AZ Incendiary is not the native throwable.

**RPG phase 1 (done):** Code/Rpg (RpgValue.h Get/Apply/Multiplier, RpgSystem), Assets/Libs/AZ/Attributes.xml, AZSave "AZRpg".
Hooks: health.max (Health.cpp: hits scale by BASE 100, no clamp on load), weapon.reloadTime (clip speed 1/m),
weapon.spread in PLAYER-side callers only (not GetSpread: AI turrets share it). Waiting: noise.footstep, melee.damage (claude-npc).

**Traps paid for tonight:**
- Never put `python -` in a command (even with </dev/null): it hung twice. Write scripts to files.
- SEntitySpawnParams::sName is a pointer: `params.sName = string(...)` dangles -> Unicode assert in ValidateSpawnParameters.
- CRY_STATIC_AUTO_REGISTER_FUNCTION needs <CryCore/StaticInstanceList.h>; uber files merge anonymous namespaces (duplicate helper names break).
- In the launcher, Schematyc registration (my Init) runs AFTER ESYSTEM_EVENT_GAME_POST_INIT: connect lazily at LEVEL_GAMEPLAY_START.
- Rider config for l_000 is "AZ GameLauncher L_000 test (Profile build)"; the plain one loads `example`.
- New sources stay OUT of Code/CMakeLists.txt until my own build window (A-WORLD-CMAKE); Codex-file hunks via a patch script with backups (Code/World/pending/apply_t1.py).
- Back up user/savegames before a run that saves, restore after.

Related: [[project_cryengine_noesis_ui]], [[project_cryengine_npc_port]], [[feedback_mechanics_rpg_hooks]].
