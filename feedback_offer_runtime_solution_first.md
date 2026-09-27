---
name: feedback-offer-runtime-solution-first
description: "★★★ When the user asks for a universal / 'always works' system, offer the RUNTIME data-driven design (markers on the asset + IK in the AnimBP) up front - do not quietly default to offline baking. User had to invent finger IK to markers himself (2026-09-27)."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-27T04:15:20.336Z
---

User asked many times for a universal grip ("универсальное решение", "маркеры на ружье?"); I kept an offline Python
solver that bakes one finger pose per weapon and re-runs on every socket change. The user himself proposed
per-finger sockets on every weapon + real-time IK in the Animation Blueprint, and asked "почему ты мне сам это не
предложил? Я же сто раз спрашивал".

**Why:** offline baking looked sufficient because the hand-weapon relation is constant while held, but it is not
universal (every placement change needs a re-run, it ignores per-clip wrist differences and arm/stock collisions).

**How to apply:** for any "universal / works everywhere" request, list the runtime option (authoring markers on the
asset, solver only GENERATES the markers, AnimBP/C++ node does constrained IK every frame) as the recommendation,
with the offline tool as the helper. Anticipate the failure axes yourself (other clips, clothing thickness, arm vs
weapon collision) before the user finds them in screenshots. Related: [[feedback-aaa-design-first]].
