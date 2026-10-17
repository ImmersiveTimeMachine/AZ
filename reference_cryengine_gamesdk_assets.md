---
name: reference-cryengine-gamesdk-assets
description: "Where the CryEngine GameSDK sample, Crysis/Ryse animation packs and Artur's CryEngine projects live; key animation facts for porting CHALK to CryEngine 5.7.1"
metadata:
  node_type: memory
  type: reference
  originSessionId: 8f8d09b9-3f5e-408a-8d76-c4e4c3910763
  modified: 2026-10-16T22:11:22.686Z
---

Checked by files on 2026-10-16. Full report: `2026-10-15/realtime-voice-chat/outputs/cryengine-gamesdk-port-research/REPORT.md`.

**Projects**
- Older CryEngine game project = `C:/CryEngine/Chalk` (still protected; the current one is AZ, see below): an IEntityComponent plugin, the TPS template turned first-person, character sdk_player. It is built inside the engine solution into `bin/win_x64/Chalk.dll`.
- `GameProject/TPS/TPS` = an untouched TPS template copy, used as the MCP test bench.

**GameSDK sample**
- Code: `C:/CryEngine/Code/GameSDK/GameDll`, about 474k lines. It is legacy: IGameStartup, IGameObjectExtension, Lua, Flash. Built as `bin/win_x64/CryGameSDK.dll`.
- Assets: `F:/CRYENGINE/CRYENGINE Launcher/Game/gamesdk.cryproject` + paks. The pak files are zip archives and contain compiled data only.
- The sample loaded the Woodland level in the source-built Sandbox on 2026-04-04.

**Launcher packs** in `F:/CRYENGINE/CRYENGINE Launcher/Assets/crytek/`
- `crysis-animation-assets`: 298 source FBX on the GameSDK `Bip01` skeleton. They are the sources of the GameSDK clips.
- `ryse-legionnaire`: its own skeleton (`root/Pelvis/Spine01…`) with 1423 `.i_caf`, 25 `.bspace`, and no ADB.

**Animation facts in CE 5.7.1**
- There is no retargeter; tracks bind to joints by the CRC of the lowercase joint name.
- Blend-space parameters come from the motion of joint 0.
- There is no Motion Matching. GameSDK locomotion is the MotionMovement fragment plus a `.comb` blend space with MoveSpeed, TravelAngle, TurnSpeed and TravelSlope.
- GameSDK first-person uses two sdk_player instances:
  - The visible one is in context Char1P (playerAnims1P + rifleAnims1P: FPIdle/FPMovement/FPSway).
  - The shadow one is in context Char3P (playerAnims3P: MotionMovement).
  - The camera is the entity position plus the Lua stance viewOffset, not the bone.
- The first-person rifle pose comes from GameSDK-registered procedural clips: WeaponPose, WeaponSway, WeaponRecoil, WeaponBump.
- CryAction AnimatedCharacter reads its Mannequin setup only from the entity Lua table, and decides 1P via IActor.

**Current working project (2026-10-16):** `C:/CryEngine/AZ` = the FPS template plus an unbuilt Motus Shift draft. It has no GameSDK ADB, controller definitions, Rifle or Lua.

**Parity review:** `outputs/cryengine-gamesdk-player-parity-review/REVIEW.md`. Verdict: a GameSDK-host project first, with the stock sample as the reference.

**How to apply:** when CHALK is ported to CryEngine, retarget the UE clips onto `Bip01` in the UE IK Retargeter, export FBX, then import them in Sandbox. Never write to the protected M16, pistol or unarmed UE sets ([[project-protected-weapon-sets]]). Related: [[project-cryengine-mcp-review]], [[project-master-skeleton]].
