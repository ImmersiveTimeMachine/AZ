---
name: reference-cryengine-test-level-l000
description: "CryEngine AZ test level L_000 (Artur 2026-10-17) - what is in it, how to run it, decal fix facts"
metadata:
  node_type: memory
  type: reference
  originSessionId: 249c364b-cc5c-42dd-b0dc-3dc5858d77eb
  modified: 2026-10-18T09:06:37.002Z
---

USER RULE (Artur 2026-10-17): `C:/CryEngine/AZ/Assets/Levels/l_000` is THE test level for agent runs in the CryEngine AZ project (not `example`).

- Contents (moved 2026-10-17 from `C:/CryEngine/GameProject/TPS/TPS` level `example`): layer `TPS_Example` (9 RigidBody cubes, Designer box, 2 school lockers, 2 cloakroom units, SpawnPoint, EnvironmentProbe, MCP test entities) + `imported scenes/Classroom_v02` (65 classroom brushes). Assets copied to `AZ/Assets/Objects/imported/classroom_v02_20261015`, `classroom_v01_20261015/shared_library`, `Objects/Blender/20261016-*`, `np7b41o`, `mcp_checks/effects_components_20261016`. Terrain flat 32 m, same coordinates as TPS.
- Game loads `level.pak`: after editing layers the level must be exported, otherwise the launcher shows the old level. Headless, no editor UI (66 s): `C:/CryEngine/bin/win_x64/Sandbox.exe -project "C:/CryEngine/AZ/Game.cryproject" -BatchMode -export "C:/CryEngine/AZ/Assets/Levels/l_000/l_000.level"` (no other Sandbox may run: single-instance dialog; take the launch lock).
- Rider config "AZ GameLauncher L_000 test (Profile build)" (= the test config with `+map l_000`), same `tools/ui/run_game_test.py --prepare/--wait-end/--collect` harness.
- Bullet decals: entity props need terrain-layer-blend off, static brushes need ERF_FOB_ALLOW_DECAL_BLEND; both set on hit in Codex's `Code/Weapons/MaterialImpacts.cpp` (AllowDecals). Decal lifetimes 100000 in FXLibs, `e_DecalsNeighborMaxLifeTime 0` in Game.cryproject; pool 512.

- Imported FBX meshes have NO collision: stock MeshImporter (AssetImporterFBX.cpp InitScene) sets every material physicalize=None. Fix headless: tools/assets/cgf_rebuild_physics.py (RC from the CGF's own embedded settings, physicalize=default; checks triangles/materials unchanged; pass absolute paths, the temp JSON must be named <fbx stem>.json). School meshes in l_000 rebuilt 2026-10-17.
- Materials without SurfaceType hit as mat_default -> GameSDK debug particle "NO SURFACE TYPE! ASSIGN SOMETHING TO THIS!". Set SurfaceType in the .mtl (school materials done 2026-10-17).
- Reaching objects in tests: sprint ~9 m/s, run ~4.6 m/s; az_weapon_input look <pitch> <yaw> (yaw + toward -X); decals are 2-10 cm, the crosshair ticks can hide them - crop screenshots.
- Player-eye playtest recipe (claude 2026-10-18, `tools/validation/claude_playtest_katimavik.txt`):
  - SpawnPoint is (68.3, 51.1, 32), facing +Y.
  - To put the player at an exact spot: `az_traversal_course X Y 0`, then `az_traversal_goto 0 6.0` (puts the player at (X, Y) facing north), then `az_traversal_course_clear`. Dead reckoning with `az_traversal_fake_run` misses by metres.
  - Real input: `az_traversal_key maxis_x N` turns 0.1146 deg per unit (+ = right/east; 785 = 90 deg); maxis_y + = down; `w down/up` walks.
  - A synthetic `mouse1 down` fires only ONE round: the real mouse state releases it. This is a harness limit, not a weapon bug.
  - The "az_weapon_input is marked [VF_CHEAT]" error is only a log line; the command still runs.
- Katimavik classroom (codex 2026-10-18; layers K08_*, Objects/kcls08 65 shell + Objects/kprops02 141 contents, offset 84 74 32, door in the east wall x 93): inside it looks great. At 05:05 it had:
  - no collision (0/206 CGFs with MeshPhysicsData);
  - the engine "Replace Me" texture on the exterior faces of the door wall and the south wall;
  - the room outside NavigationArea-1 (main.lyr).
  Reported to codex as A-PLAY-K08. Codex owns these assets, peers do not edit them.

- Meshes over 65535 vertices: RC cannot mesh-physicalize them and falls back to a Box Physicalizer (an oriented, skewed auto box: the 10-17 lockers bug). Artur 10-18: re-import, don't patch. Recipe used for the lockers: background Blender adds an axis-aligned box object with its own material over the mesh bounds (MCP producer FBX axes), RC with merge_all_nodes, render material physicalize=no, box material proxy_only on a new sub index, and the .mtl gets that sub-material (Shader Nodraw, MtlFlags 525440, SurfaceType). Scripts were in the session scratchpad; the method is in agents-chat.md DONE-LOCKERS-PROXY.

Related: [[project_cryengine_noesis_ui]], [[reference_cryengine_build]].
- Navmesh regen without the editor UI (2026-10-18, claude-npc): `Sandbox.exe -project <Game.cryproject> -BatchMode -exportAI <level>` regenerates MNM (GenerateAiAll -> ProcessQueuedMeshUpdates) but writes it LOOSE - its ExportLevel uses flags 0, so the new mnmnavmission0.bai is NOT packed (GameExporter.cpp:1660-1672). And if level.pak still holds the old navmesh, a load only difference-updates a changed boundary. Recipe: drop the old entry, run -exportAI, pack the loose file: tools/npc/pak_replace_entry.py <pak> mnmnavmission0.bai --remove / <file> (raw copy of every other entry). `-runpython` is broken in this Sandbox (argv0 parsed as the script).
- Launch checks (Artur 10-18): never auto-Ignore asserts (run_game_test.py now stops waiting with exit 3 and leaves the dialog for stack capture; --assert break|ignore|continue|stop); tools/npc/wait_scenario_end.py waits for the script end and runs codex's read-only detector; on an assert pause in Rider, xdebug_get_threads/xdebug_get_stack/xdebug_get_frame_values, then stop. Since 13:51 a level-load assert DeviceRenderPass.cpp:140 (ocean reflection CREWaterOcean -> minimum pipeline shadow pass) blocks some runs.
