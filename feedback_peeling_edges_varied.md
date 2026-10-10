---
name: feedback-peeling-edges-varied
description: "Artur's art direction for procedural peeling paint — varied edges (sharp+rounded), essence over 1:1 match, roughness must not be forgotten"
metadata:
  node_type: memory
  type: feedback
  originSessionId: b0898a20-3540-4116-9321-4c45e9195191
  modified: 2026-10-09T22:48:32.948Z
---

For procedural wear/peeling materials: edges must be random in character — sharp in some places, rounded in others, never uniformly rounded. Match the reference in essence (structure, scale, layers), not 1:1 mask identity. Always author a meaningful Roughness, not flat per-layer constants.
**Why:** Artur reviewed v08 iterations (2026-10-09): "слишком много закругленных деталей… где-то острая, где-то закругленная"; "маска не может совпадать сто процентов… главное чтобы было похоже по сути"; "не забудь там шероховатости".
**How to apply:** drive edge style by a low-frequency mask (lobes vs shards), vary breakup depth, and show the Roughness panel in every review sheet. See [[project-peeling-plaster-designer-v08]].
