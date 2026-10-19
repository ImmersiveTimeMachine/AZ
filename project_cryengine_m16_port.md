---
name: project-cryengine-m16-port
description: "2026-10-19: CHALK's own M16 ported from Unreal into CryEngine AZ as item AZ.M16 on the GameSDK rifle rig (data only, no C++), l_000 pickup swapped; skill cryengine-weapon-port covers the next guns (AK12 parts route checked)."
metadata:
  node_type: memory
  type: project
  originSessionId: ac619f31-3329-4ad0-b54d-836c05698936
  modified: 2026-10-19T05:29:32.298Z
---

Done 2026-10-19 night by session claude-m16 (Artur asleep; coordinator claude MCP/main ACKed the shared-file edits).
- AZ.M16 = Unreal `/Game/AZ/Assets/M16/SKL/M16_Skeleton` rebuilt in Blender onto `rifle_fp_skel.chr` bones/bind (identical,
  controller ids too), GameSDK 1P clips/ADS/sounds/runtime unchanged; runtime copy `M16.xml` (30 rounds, Single first).
- Placement rule that mattered: y by grip silhouette fit, z by the SIGHT LINE (not the bore) — 3.5 mm moved the ADS picture 45 px.
- CDF CA_BONE Position/Rotation are absolute model-space defaults (magazine cgf in model space) — cost one run.
- Checked in game (Rider L_000, asserts none): idle, ADS, fire flash/shells, full+empty reload, mode switch, drop/world model,
  l_000 pickup spawn. Not checked: shadow TP weapon, inventory 3D viewer, sprint/traversal with it; reload hand reaches a side
  lever that the M16 does not have (top charging handle moves on its own).
- Look: worn metal reads lighter in bright outdoor CE light than in Unreal's dark rooms — Artur's eye pending.

**Why:** Artur wants CHALK's own guns instead of GameSDK placeholders, "по размерам и по всему" matching.
**How to apply:** next gun = skill [[cryengine-weapon-port]] (tools/weapons, specs/ak12_draft.json has the parts route; placement
must be fitted first). Related: [[project-cryengine-ragdoll]], [[project-weapon-models-import-2026-09-25]], [[project-protected-weapon-sets]].
