---
name: reference_zombie01_abp_demoplayable
description: "Full live-MCP audit (2026-06-27) of /Game/Zombie_01/Animations/ABP_DemoPlayable — the pack's playable-zombie Animation Blueprint we use as the STRUCTURAL TEMPLATE for AZ_ABP_Chalkie. Holds its 9 variables, the 3-state-machine pose pipeline (Locomotion blendspace + Turns SM + Attacking SM composed via BlendListByBool + LayeredBoneBlend), the EventGraph per-tick logic, and the translation map to our Mover+GAS Chalkie (what stays AnimGraph / becomes C++ in UAZ_InfectedAnimInstance / Mover replaces / GAS replaces). Read when building AZ_ABP_Chalkie or the slim anim instance."
metadata:
  node_type: memory
  type: reference
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
---

# ABP_DemoPlayable audit + translation map → AZ_ABP_Chalkie

Audited live 2026-06-27 via the native `unreal-mcp` `BlueprintTools` (`list_graphs`/`list_variables`/`read_graph_dsl`/`find_nodes`/`get_node_infos`). Path: `/Game/Zombie_01/Animations/ABP_DemoPlayable`. It is the **ZombiePack demo's PLAYER-CONTROLLED zombie ABP** built on a classic **ACharacter + CharacterMovementComponent** pawn (`BP_ZombiePlayable`) driven by `PC_DemoPlayable`. We keep its pose-graph SHAPE, replace almost all its LOGIC. See [[project_npc_foundation]] (anim-instance decision) + [[reference_mover_root_motion]] (RM montages).

## What it contains

**9 variables:** `Character` (BP_ZombiePlayable), `Controller` (PC_DemoPlayable), `FwdAxisValue` (float), `Turning` (bool), `TurnOverride` (bool), `TurnType` (int 0–3 = L90/L180/R90/R180), `Attacking` (bool), `AttackType` (int), `AttackAlphaBlend` (float).

**AnimGraph pose pipeline (node-verified):**
```
Locomotion SM ─► [1 state: IdleWalkChase = BlendSpacePlayer driven by FwdAxisValue] ─► SaveCachedPose
      │
      ▼  AnimGraphNode_BlendListByBool_0  (bool = Turning)
      ├─ false ► Locomotion cached pose
      └─ true  ► Turns SM (StateMachine_74: conduits Left/Right Turn → states Left/Right-1 90, Left/Right-1 180; uses AnimNotify EndRotation)
      │
      ▼  AnimGraphNode_LayeredBoneBlend_0  (alpha = AttackAlphaBlend)
      └─ overlays Attacking SM (StateMachine_14: Attack 1–8, incl Attack 5–8 = Hyper 1–4; conduit-gated)  on the base, per-bone (upper body)
      │
      ▼  Output Pose
```
Confirmed driver bindings: `Turning`→BlendListByBool input; `AttackAlphaBlend`→LayeredBoneBlend alpha; `FwdAxisValue`→IdleWalkChase BlendSpacePlayer X-axis.

**EventGraph (per tick `EventBlueprintUpdateAnimation`):**
- Cast pawn → `BP_ZombiePlayable`; get `PC_DemoPlayable`.
- `FwdAxisValue = GetFwdAxisValue(Controller)` — **player input axis**.
- If `FwdAxisValue>0 && !TurnOverride`: `SetMaxWalkSpeed( GetCurveValue("Fwd Vel") )` on the **CharacterMovementComponent**; `Turning=false`.
- `SetRelativeLocation(Mesh, x/z from mesh, y = GetCurveValue("Lateral Dist") * -1)` — **curve-baked lateral offset = fake root motion** for in-place turns.
- `CheckLeftTurns(0.2)` / `CheckRightTurns(0.75)` → set `TurnType` (helper fns; bodies trivial — input `DelayCheck`).
- `Attacking = GetAttacking(Controller)`; `AttackType = GetAttackType(Controller)`; `AttackAlphaBlend = FInterpTo_Constant(cur, Attacking?1:0, dt, 1.75)`.
- AnimNotifies: `EndRotation→Turning=false`, `RotationOverrideClear→TurnOverride=false`.

**Three facts that drive the translation:** (1) player-controller-driven (axis + `GetAttacking/GetAttackType` on a PlayerController) — ours is AI; (2) ACharacter+**CMC** backend — ours is **Mover**; (3) curve-driven speed + curve-baked mesh offset = in-place-clip hacks — we use Mover (loops) + RM montages (one-shots).

## Translation map → our Chalkie (Mover + GAS)
| `ABP_DemoPlayable` part | Destination |
|---|---|
| Whole pose graph (3 SMs, blendspace, BlendListByBool, LayeredBoneBlend, cached poses) | **STAYS AnimGraph** in `AZ_ABP_Chalkie` (C++ can't define pose links) |
| Locomotion blendspace (idle→walk→chase) | **KEEP**; drive its axis from **Mover speed**, not `FwdAxisValue` |
| EventGraph driver math (blendspace alpha, `Turning`, `TurnType`, `AttackAlphaBlend` smoothing, turn detection) | **→ C++** in `UAZ_InfectedAnimInstance` (NativeUpdate / thread-safe) |
| `GetFwdAxisValue(Controller)` | **Mover** → `Cached_MoverComponent->GetVelocity().Size2D()` |
| `SetMaxWalkSpeed`/"Fwd Vel" curve; "Lateral Dist" mesh-offset | **DELETED** — Mover gait owns speed; displacement via RM/Mover |
| `Attacking`/`AttackType` (controller) | **GAS** → ability sets tag; AttackType = which montage `GA_MeleeAttack` plays |
| Attacking SM + LayeredBoneBlend | **Replace with montage→`DefaultSlot`** (RM via `FLayeredMove_AnimRootMotion`); slot layers naturally, GAS-native — no in-graph attack SM |
| Death / hit-react / phase / OnFire | **GAS** tags+abilities+montages, never controller-polled ABP vars |
| `Turning`/`TurnType`/Turns SM | **Optional cosmetic** turn-in-place, C++ yaw-delta driven; not GAS |

**Net:** take the AnimGraph *shape* (blendspace loco + optional turn SM + upper-body overlay → which validates Option B); transform the input layer (player axis→AI/Mover intent), the backend (CMC→Mover velocity), and attack/death (controller-SM→GAS abilities + RM montages). Almost none of its *logic* survives, because its logic is "read a PlayerController + CMC." Translate ONLY the locomotion-driver math to C++; GAS owns the gameplay.

## Tooling note (how this was read)
Native `unreal-mcp` `BlueprintTools` reads AnimBP K2 graphs (EventGraph/functions) via `read_graph_dsl` (arg `graph:{refPath}`), but the **AnimGraph pose graph returns empty from `read_graph_dsl`** — use `find_nodes(graph,{title:""})` + `get_node_infos([{refPath}…])` to enumerate pose nodes + their variable-driver bindings. `blueprint`/`graph` args are UObject refs (`{"refPath":"…:GraphName"}`); SM sub-graphs nest as `…:AnimGraph.AnimGraphNode_StateMachine_N.<SMName>.AnimStateNode_K.<StateName>`. BlueprintTools has NO anim-authoring tools (see [[reference_ue_native_mcp_server]] gap #4) → `AZ_ABP_Chalkie`'s graph is hand-built or duplicated from `ABP_DemoPlayable`.
