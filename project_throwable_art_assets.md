---
name: project-throwable-art-assets
description: "User-chosen art for the throwable grenade item — icon for HUD/inventory, mesh for the pickup and thrown object"
metadata: 
  node_type: memory
  type: project
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-17T01:42:37.601Z
---

The user chose these for the throwable item (stated 2026-09-16, while the throwable system was being built):

- **HUD + inventory icon:** `/Game/FPS_Controller/UI/Textures/T_FragGrenadeIcon` — 32×32.
- **Pickup / held / thrown mesh:** `/Game/FPS_Controller/WeaponsAndAttachments/FragGrenade/SM_M67_Preview` —
  measured 8.49 × 9.02 × 7.05 cm, i.e. already at real M67 scale, one material section. So
  `UAZ_ThrowableDefinition::HeldMeshSize` can stay 0 (use authored size) and `CollisionRadius` ≈ 4.5 matches
  the visible object.

**Why:** the project has no stone art (the only rock meshes are 3 cm `Debris_Chunk*` under
`/Game/FPS_Controller/Effects/Meshes/Rocks/`), so the first testable throwable uses this grenade art instead
of a placeholder pebble.

**How to apply:** use these paths when authoring the throwable item's manifest (icon fragment) and its
`UAZ_ThrowableDefinition.HeldMesh`. The Phase 1 slice still ships `BounceAndSettle` behaviour — fuse and
detonation are Phase 2, so the object looks like a grenade before it behaves like one. Related:
[[project-throwable-system]].
