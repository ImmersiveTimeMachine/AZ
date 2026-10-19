---
name: project-cryengine-ragdoll
description: "CryEngine AZ ragdoll pipeline (claude-ragdoll, 2026-10-18) - Chalkie chalkie_skel.chr got 18 proxies + limits + joint frames from the Unreal vendor PhysicsAsset; tools/assets/ragdoll; skill cryengine-ragdoll; game side handed to claude-npc"
metadata:
  node_type: memory
  type: project
  originSessionId: c75968cb-38fc-412a-9a52-f5fe53bb4964
  modified: 2026-10-18T21:45:59.062Z
---

Artur approved 2026-10-18 ~16:50 ("делай рэгдолл по этому плану в отдельной сессии: утилита, скилл, затем игровая часть у claude-npc"). Why: Chalkie corpses were animation-posed (arms through the K08 wall).

State (2026-10-18): tools in `C:/CryEngine/AZ/tools/assets/ragdoll/` (ue_export_physics_asset, ragdoll_template, ragdoll_build, ragdoll_review_blender, ragdoll_pose_check, chr_inspect), work folder `work/chalkie/` (physics_asset.json, chalkie_ragdoll.fbx, report, review png, backups). Contract for the game side: `tools/assets/ragdoll/chalkie_ragdoll_contract.md` (PE_NONE first, then PE_ARTICULATED nSlot=character slot, nLod 1, mass 80, bCopyJointVelocities). Skill: `~/.claude/skills/cryengine-ragdoll`.

Non-obvious facts (also in the skill):
- RC FBX path always writes framemtx invalid (100) -> CryAnimation measures joint angles from the PARENT orientation; UE skeletons have bones 100-180 deg off their parent, so the tool patches framemtx = !parent_bind*child_bind (= what the engine does for .chr.cgf proxies, Model.cpp:482).
- CryPhysics moves a limit onto a ragdoll start angle outside it (articulatedentity.cpp:1759-1778): do not widen limits to the clips (tried, everything became free; reverted).
- PE_LIVING with nSlot -1 never builds character physics -> RelinquishCharacterPhysics would use mass 0; go PE_NONE first.
- UE vendor hinges are symmetric (knees bend backwards 45 deg) -> anatomical hinges (130 flex, 5 hyper) by default.

**How to apply:** any rebuild of the Chalkie chr (tools/npc/build_chalkie_character.py, Sandbox reimport) must be followed by ragdoll_build.py --install, else proxies/frames are lost. Related: [[project-cryengine-npc-port]], [[reference-cryengine-test-level-l000]].
