---
name: feedback-h2l-one-depth-check
description: "USER DECISION 2026-10-15 — High→Low agent may run ONE native depth check against the High and make ONE correction (2 runs max); all skill/prompt rules must be general for any object, never cabinet-specific"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 2fe7c274-bddc-45e1-ab33-fd1c3fc14bd2
  modified: 2026-10-15T20:08:36.789Z
---

Artur, after seeing door gaps and a hinge buried in a side wall: "Делай оба: правила в скилл и автоматическую сверку с High с одним проходом исправления агентом" and "инструкции … должны быть общими", not only for lockers.

Implemented in AZ AI Utilities 1.5.0: `depth_check.py` (six-axis ray depth High vs Low: hole/extra regions, systematic per-side offset, PNG per side), `reconstruction.preview_check` limited to 2 runs, runner rejects jobs without a check or with edits after the 2nd; High numeric report has `surface_planes_source_local` (bounds include protruding hardware — the agent once put the whole front 1.7 cm forward from them). This supersedes the older blanket "no Low comparison / no refine loop" rule only for this one bounded check.

**Why:** the agent builds blind; small measurement errors (pitch drift in `repeat`, missing frame members, wrong face depth) were invisible to it. Control run: holes 4075→104 cm² and front offset caught; with planes the front was right first time and the frame was modeled.
**How to apply:** keep rules object-agnostic (surface continuity, depth from the High, repeat step from first/last instance, nothing visible buried). Open issue seen 2026-10-15: after the correction the front can come out too flat (agent pushed protrusions in) — watch for over-correction before adding more penalties. Related: [[project-blender-ai-utilities-audit]].
