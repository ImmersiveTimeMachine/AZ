---
name: project_cryengine_traversal_port
description: "CryEngine AZ traversal (mantle/hurdle/climb) port from Unreal, 2026-10-18 night: T1-T4 built and verified, Player.cpp hooks H1-H3 + weapon admission applied (Codex ack) and verified with real input; quick vault over thin walls added from GameSDK 1P clips"
metadata:
  node_type: memory
  type: project
  originSessionId: c5ca5c04-c7d0-45c7-80f5-29342ab3eb94
  modified: 2026-10-18T07:31:37.312Z
---

State 2026-10-18 ~03:32 (claude-traversal session). Plan + progress + team self-assessment:
`C:/CryEngine/AZ/docs/implementation/traversal-port/traversal-port-analysis-and-plan.md` (sections 10, 11). Skill: `cryengine-traversal`.

- Verified on l_000 with REAL keys (`az_traversal_key` -> IInput::PostInputEvent; scenario `t5_real_input.txt`): Jump -> mantle /
  deep vault / climb, press buffer, momentum exit 4.08 m/s, ordinary jump unchanged, view clamp (yaw 50, pitch to the top),
  busy-weapon refusal (Codex `IsWeaponBusy`).
- Hooks in Codex files applied by me with his ack, originals in `docs/implementation/backups/traversal-hooks-20261018-031036`.
  `git apply` turned LF files into CRLF - always byte-diff against the backup after applying into someone else's file.
- Quick vault over thin walls (<= 0.5 m deep): GameSDK 1P `stand_tac_vaultinto_1p_01` + `stand_tac_vaultout_1p_01` chained
  in one fragment (`floor+quick+over`); filled the gap that was planned for a GASP retarget.
- Claw during traversal verified 04:24-04:28 (t7): hanging climb -> pulled down, torn back to a free spot, view whip, landing
  thud; low vault -> no pull-down/bump/stagger. Trap: a kinematic body can sit inside the target (unswept) - never re-enable
  physics there (it popped onto the box). A real Chalkie claw never reached a climbing player (NPC reach/rise rule).
- 2026-10-18 13:57: l_000 SHOWCASE spawns by itself next to the SpawnPoint (Artur could not find any obstacles: the course
  needed a console command). Launcher verified (t8, 6/6 with real keys); Sandbox game mode not verified yet. Russian note:
  `docs/implementation/traversal-port/artur-how-to-try.md`. South Chalkie pack attacks at spawn - mentioned in the note.
- 2026-10-19 night: eye follow (H5, Player.cpp +2 lines, approved by MCP/main) - during a traversal the 1P eye follows the
  clip's measured head (`<Eye>` track), verified t10 (climb pull-up: head over the lip, not 1.1 m above it);
  `az_traversal_eye_follow 0` = old fixed eye. TopStand (shallow tops) and az_traversal_place added 10-18.
- Open: hand IK on the edge (not done: per-frame solver vs Artur's bake rule); Chalkie reach vs a climbing player is claude-npc's call (A-T05-RUNTIME);
  inventory-equip refusal mid-traversal not exercised in game.

**Why:** Artur delegated decisions to the team overnight (01:50) and asked for team work + a self-assessment.
**How to apply:** resume from section 10 of the plan; team decisions are switchable cvars (`az_traversal*`).
Related: [[project_traversal_2026-09-15]], [[project_cryengine_npc_port]], [[feedback_cryengine_agents_lock_planning]].
