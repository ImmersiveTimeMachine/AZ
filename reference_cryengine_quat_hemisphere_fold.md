---
name: reference-cryengine-quat-hemisphere-fold
description: "CryEngine blend \"twitch\" trap — a joint whose bind rotation is ~180 deg off its clips folds the body at 50/50 blends; RC forward_up_axes puts the turn into the bind pelvis for meshes but into the root for clips"
metadata:
  node_type: memory
  type: reference
  originSessionId: c80db681-93c6-4d97-a1bd-f1c435991d66
  modified: 2026-10-18T21:22:11.820Z
---

CryAnimation picks each clip's quaternion sign against the skeleton's BIND (default) relative pose before the weighted sum (C:/CryEngine/Code/CryEngine/CryAnimation/Command_Commands.cpp:343-360 and 556-570, `rot * fsgnnz(default.q | rot)`). If a joint's bind rotation is ~180 deg away from what the clips play, the dot is ~0, the sign is a coin toss per clip, and at ~50/50 blend weights the sum goes near zero: the body folds/swings for 3-4 frames (Chalkie 2026-10-18: head 0.5-1.2 m per frame, "передёргивает" at every clip change).

Cause seen: RC with forward_up_axes '+Y+Z' put the 180 deg turn into the bind PELVIS for the mesh (.chr/.skin) but into the ROOT for the clips (.caf); root-motion extraction drops the root, a slot turn (kSlotTurn) faced the body forward, so it looked right except during blends.

Diagnose: per-frame character-space joint positions + layer-0 FIFO weights (Chalkie: az_npc_seam_trace 2 -> AZ_POSE/AZ_ROT, tools/npc/pose_snap_report.py); joint speeds of tens of m/s at blend midpoints = fold; log rel quat dot with GetDefaultRelJointByID. A/B proof without rebuild: `ca_SampleQuatHemisphereFromCurrentPose 1` (cheat cvar, compile-time 0 in Release — proof only).

Fix in data: build the skeleton/skin so the bind matches the clips (Chalkie: MESH_AXES '-Y+Z' in tools/npc/build_chalkie_character.py --mesh-only). Check any new CE character the same way: bind-vs-clip dot per joint should be near +-1. Related: [[project-cryengine-npc-port]].
