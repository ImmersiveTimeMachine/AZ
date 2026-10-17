---
name: reference-cryengine-test-level-l000
description: "CryEngine AZ test level L_000 (Artur 2026-10-17) - what is in it, how to run it, decal fix facts"
metadata:
  node_type: memory
  type: reference
  originSessionId: 249c364b-cc5c-42dd-b0dc-3dc5858d77eb
  modified: 2026-10-17T19:45:46.999Z
---

USER RULE (Artur 2026-10-17): `C:/CryEngine/AZ/Assets/Levels/l_000` is THE test level for agent runs in the CryEngine AZ project (not `example`).

- Contents (moved 2026-10-17 from `C:/CryEngine/GameProject/TPS/TPS` level `example`): layer `TPS_Example` (9 RigidBody cubes, Designer box, 2 school lockers, 2 cloakroom units, SpawnPoint, EnvironmentProbe, MCP test entities) + `imported scenes/Classroom_v02` (65 classroom brushes). Assets copied to `AZ/Assets/Objects/imported/classroom_v02_20261015`, `classroom_v01_20261015/shared_library`, `Objects/Blender/20261016-*`, `np7b41o`, `mcp_checks/effects_components_20261016`. Terrain flat 32 m, same coordinates as TPS.
- Game loads `level.pak`: after editing layers the level must be exported, otherwise the launcher shows the old level. Headless, no editor UI (66 s): `C:/CryEngine/bin/win_x64/Sandbox.exe -project "C:/CryEngine/AZ/Game.cryproject" -BatchMode -export "C:/CryEngine/AZ/Assets/Levels/l_000/l_000.level"` (no other Sandbox may run: single-instance dialog; take the launch lock).
- Rider config "AZ GameLauncher L_000 test (Profile build)" (= the test config with `+map l_000`), same `tools/ui/run_game_test.py --prepare/--wait-end/--collect` harness.
- Bullet decals: entity props need terrain-layer-blend off, static brushes need ERF_FOB_ALLOW_DECAL_BLEND; both set on hit in Codex's `Code/Weapons/MaterialImpacts.cpp` (AllowDecals). Decal lifetimes 100000 in FXLibs, `e_DecalsNeighborMaxLifeTime 0` in Game.cryproject; pool 512.

- Imported FBX meshes have NO collision: stock MeshImporter (AssetImporterFBX.cpp InitScene) sets every material physicalize=None. Fix headless: tools/assets/cgf_rebuild_physics.py (RC from the CGF's own embedded settings, physicalize=default; checks triangles/materials unchanged; pass absolute paths, the temp JSON must be named <fbx stem>.json). School meshes in l_000 rebuilt 2026-10-17.
- Materials without SurfaceType hit as mat_default -> GameSDK debug particle "NO SURFACE TYPE! ASSIGN SOMETHING TO THIS!". Set SurfaceType in the .mtl (school materials done 2026-10-17).
- Reaching objects in tests: sprint ~9 m/s, run ~4.6 m/s; az_weapon_input look <pitch> <yaw> (yaw + toward -X); decals are 2-10 cm, the crosshair ticks can hide them - crop screenshots.

Related: [[project_cryengine_noesis_ui]], [[reference_cryengine_build]].
