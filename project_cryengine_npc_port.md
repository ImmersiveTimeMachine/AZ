---
name: project-cryengine-npc-port
description: "Chalkie NPC port Unreal -> CryEngine AZ: analysis + 13-stage plan written 2026-10-17, awaiting Artur's yes; key verified facts (assets, numbers, engine pieces) and where the plan lives"
metadata:
  node_type: memory
  type: project
  originSessionId: c80db681-93c6-4d97-a1bd-f1c435991d66
  modified: 2026-10-17T19:49:05.928Z
---

Plan + analysis: `C:/CryEngine/AZ/docs/implementation/npc-port/chalkie-npc-port-analysis-and-plan.md` (v01, 2026-10-17). Stage 1 = analysis only; implementation waits for Artur's «да» and his answer on the first-person grab look.

Verified facts (not in the repo elsewhere):
- NPC mesh = `/Game/ZombiePackV1/ZombieAC/Mesh/ZombieAC_A/SK_ZombieAC_A` on `/Game/Zombie_01/Core/Character/Mesh/UE4_Mannequin_Skeleton`: 71 bones (UE4 mannequin + EyeBone_l/r, JawBone), 1 LOD, 2 materials (PBR, ORM packed), 6 textures 2048, morphs ZombieOpenMouth/ZombieGrin. No FBX of the mesh exists anywhere -> export from UE.
- `Content/Zombie_01/Source/FBX/In-Place` + `Root_Motion` = 150+150 source FBX, each with mannequin mesh+skin -> satisfies CryEngine RC's "skinned mesh required" rule.
- Live BP perception: Sight 15 m / Lose 23 m / Hearing 7 m (the 500/800 in [[project_chalkie_fight_rules]] is stale).
- CryEngine side: component-only AI works with IEntityNavigationComponent (velocity callback), IVisionMap/IAuditionMap directly, Factions.xml, MBT via IBehaviorTreeManager::StartModularBehaviorTree(entityId); CryPerceptionSystem plugin is legacy IAIActor (unusable). AZ has no NPC code, no Navigation.xml, empty Factions.xml, no navmesh in l_000.
- Damage hook: AZ::CWeaponImpactBus (Code/Weapons/ParticleBullet.h), one receiver per entity; Chalkie HP planned 500 GameSDK units (= 50/100).

**Why:** Artur asked (2026-10-17) to port the Chalkies with the same pack models, skeletons, anims and the whole NPC system, strictly analysis -> plan -> his yes -> implementation.
**How to apply:** resume from the plan doc; implement myself (CryEngine AZ AGENTS.md: no subagents); own files under `Code/NPC/**` etc. after announcing in agents-coordination.md. Related: [[project_npc_foundation]], [[project_zombie_ai_plan]], [[project_chalkie_fight_rules]], [[reference-cryengine-build]], [[reference-cryengine-test-level-l000]].
